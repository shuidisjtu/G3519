/**
 * @file    tsp_motor.c
 * @brief   DC Motor Driver for DRV8874 H-Bridge (PMODE=HIGH)
 *
 * Hardware:
 *   G3519 TIMA0 -> Expansion Board J5 -> DRV8874 (Qty.2)
 *   Motor1: PB3=TIMA0_CCP0(PWM->IN2) + PB4=GPIO(DIR->IN1)
 *   Motor2: PB0=TIMA0_CCP2(PWM->IN2) + PB2=GPIO(DIR->IN1)
 *   nSLEEP: PB1 (shared, active HIGH)
 *   nFAULT: PA7 (shared, active LOW = fault)
 *
 * DRV8874 control logic (PMODE=HIGH):
 *   Forward:  IN1=L,  IN2=PWM  -> current out1->out2
 *   Backward: IN1=H,  IN2=PWM  -> current out2->out1
 *   Coast:    IN1=L,  IN2=L    -> Hi-Z
 *   Brake:    IN1=H,  IN2=H    -> low-side brake
 *
 * Reference: D:\EDC26_HSPv2\Utilities\HSP_MOTOR.c
 */

#include "tsp_motor.h"

/* ===== TIMA0 PWM clock configuration =====
 * timerClkFreq = BUSCLK / (divideRatio * (prescale + 1))
 *   80000000 Hz = 80000000 Hz / (1 * (0 + 1))
 * PWM frequency = 80000000 / 4000 = 20000 Hz
 */
static const DL_TimerG_ClockConfig gMotorPWMClockConfig = {
    .clockSel    = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale    = 0U
};

static const DL_TimerG_PWMConfig gMotorPWMConfig = {
    .pwmMode           = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period            = MOTOR_PWM_PERIOD,
    .isTimerWithFourCC = true,
    .startTimer        = DL_TIMER_STOP,
};

void tsp_motor_init(void)
{
    /* 1. Enable TIMA0 power and reset */
    DL_TimerG_reset(MOTOR_PWM_INST);
    DL_TimerG_enablePower(MOTOR_PWM_INST);
    delay_cycles(POWER_STARTUP_DELAY);

    /* 2. Configure PB3 (TIMA0_CCP0) and PB0 (TIMA0_CCP2) as peripheral outputs.
     *    PB4 and PB2 are already configured as GPIO outputs in SYSCFG_DL_GPIO_init(). */
    DL_GPIO_initPeripheralOutputFunction(MOTOR_PWM_C0_IOMUX,
        MOTOR_PWM_C0_IOMUX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(MOTOR_PWM_C2_IOMUX,
        MOTOR_PWM_C2_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_3 | DL_GPIO_PIN_0);

    /* 3. Set TIMA0 clock and initialize edge-aligned PWM mode */
    DL_TimerG_setClockConfig(MOTOR_PWM_INST,
        (DL_TimerG_ClockConfig *) &gMotorPWMClockConfig);
    DL_TimerG_initPWMMode(MOTOR_PWM_INST,
        (DL_TimerG_PWMConfig *) &gMotorPWMConfig);

    /* 4. Set Counter zero/load/compare control to smallest CC index (CC0) */
    DL_TimerG_setCounterControl(MOTOR_PWM_INST,
        DL_TIMER_CZC_CCCTL0_ZCOND,
        DL_TIMER_CAC_CCCTL0_ACOND,
        DL_TIMER_CLC_CCCTL0_LCOND);

    /* 5. Configure CC0 (PB3 = M1IN2): Motor1 PWM
     *    EDGE_ALIGN (down-count): LACT=HIGH at LOAD, CDACT=LOW at CC match.
     *    INIT_VAL_LOW (SDK default): CC=0 → output stays LOW (0% duty).
     *    CC=PERIOD → output stays HIGH (100% duty). */
    DL_TimerG_setCaptCompUpdateMethod(MOTOR_PWM_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
        DL_TIMERG_CAPTURE_COMPARE_0_INDEX);
    DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, 0,
        DL_TIMER_CC_0_INDEX);

    /* 6. Configure CC2 (PB0 = M2IN2): Motor2 PWM */
    DL_TimerG_setCaptCompUpdateMethod(MOTOR_PWM_INST,
        DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE,
        DL_TIMERG_CAPTURE_COMPARE_2_INDEX);
    DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, 0,
        DL_TIMER_CC_2_INDEX);

    /* 7. Enable TIMA0 clock and set CC0/CC2 as outputs */
    DL_TimerG_enableClock(MOTOR_PWM_INST);
    DL_TimerG_setCCPDirection(MOTOR_PWM_INST,
        DL_TIMER_CC0_OUTPUT | DL_TIMER_CC2_OUTPUT);

    /* 8. Start PWM counter (nSLEEP controlled by caller, HSPv2: MEN_HIGH/MEN_LOW) */
    DL_TimerG_startCounter(MOTOR_PWM_INST);
}

void tsp_motor_set(uint8_t motor, uint8_t dir, uint16_t duty_pct)
{
    uint16_t cc_val;
    uint32_t dir_pin;
    uint32_t cc_index;

    if (motor > 1) return;

    /* Clamp duty cycle */
    if (duty_pct > MOTOR_DC_LIMIT) duty_pct = MOTOR_DC_LIMIT;

    /* Convert percent to CC value (0..3999) */
    cc_val = (uint16_t)(((uint32_t)duty_pct * (MOTOR_PWM_PERIOD + 1U)) / 100U);

    /* Per-motor pin assignments */
    if (motor == MOTOR1) {
        dir_pin  = PORTB_M1DIR_PIN;     /* PB4 = M1IN1 = direction */
        cc_index = DL_TIMER_CC_0_INDEX; /* PB3 = M1IN2 = PWM */
    } else {
        dir_pin  = PORTB_M2DIR_PIN;     /* PB2 = M2IN1 = direction */
        cc_index = DL_TIMER_CC_2_INDEX; /* PB0 = M2IN2 = PWM */
    }

    switch (dir) {
    case MOTOR_FORWARD:
        /* IN1=LOW, IN2=PWM */
        DL_GPIO_clearPins(PORTB_PORT, dir_pin);
        DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, cc_val, cc_index);
        break;

    case MOTOR_BACKWARD:
        /* IN1=HIGH, IN2=PWM */
        DL_GPIO_setPins(PORTB_PORT, dir_pin);
        DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, cc_val, cc_index);
        break;

    case MOTOR_COAST:
        /* IN1=LOW, IN2=0 -> Hi-Z */
        DL_GPIO_clearPins(PORTB_PORT, dir_pin);
        DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, 0, cc_index);
        break;

    case MOTOR_BRAKE:
        /* IN1=HIGH, IN2=100% -> low-side brake */
        DL_GPIO_setPins(PORTB_PORT, dir_pin);
        DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST,
            MOTOR_PWM_PERIOD, cc_index);
        break;

    default:
        break;
    }
}

void tsp_motor_stop(uint8_t motor)
{
    tsp_motor_set(motor, MOTOR_COAST, 0);
}

void tsp_motor_stop_all(void)
{
    tsp_motor_stop(MOTOR1);
    tsp_motor_stop(MOTOR2);
}

uint8_t tsp_motor_fault(void)
{
    /* nFAULT is active LOW: return 1 if faulted */
    return (FAULT() == 0) ? 1 : 0;
}

void tsp_motor_pwm_start(void)
{
    DL_TimerG_startCounter(MOTOR_PWM_INST);
}

void tsp_motor_pwm_stop(void)
{
    DL_TimerG_stopCounter(MOTOR_PWM_INST);
}
