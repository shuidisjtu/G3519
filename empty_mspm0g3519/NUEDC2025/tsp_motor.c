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

void tsp_motor_init(void)
{
    /* TIMA0 PWM (20kHz, CC0=PB3/M1, CC2=PB0/M2) is configured by SysConfig in
     * SYSCFG_DL_MOTOR_PWM_init(). Counter is left stopped there so PWM only
     * runs while a motor scene is active. */
    DL_TimerA_startCounter(MOTOR_PWM_INST);
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
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, cc_val, cc_index);
        break;

    case MOTOR_BACKWARD:
        /* IN1=HIGH, IN2=PWM */
        DL_GPIO_setPins(PORTB_PORT, dir_pin);
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, cc_val, cc_index);
        break;

    case MOTOR_COAST:
        /* IN1=LOW, IN2=0 -> Hi-Z */
        DL_GPIO_clearPins(PORTB_PORT, dir_pin);
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, 0, cc_index);
        break;

    case MOTOR_BRAKE:
        /* IN1=HIGH, IN2=100% -> low-side brake */
        DL_GPIO_setPins(PORTB_PORT, dir_pin);
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST,
            MOTOR_PWM_PERIOD + 1U, cc_index);
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
    DL_TimerA_startCounter(MOTOR_PWM_INST);
}

void tsp_motor_pwm_stop(void)
{
    DL_TimerA_stopCounter(MOTOR_PWM_INST);
}
