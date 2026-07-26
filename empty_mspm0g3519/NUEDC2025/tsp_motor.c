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

/* Duty percent -> CC value. EDGE_ALIGN counts DOWN from LOAD with
 * LACT=CCP_HIGH at load and CDACT=CCP_LOW at match, so the output is high over
 * LOAD..CC -- a larger CC means a shorter high time.
 *   duty 0%   -> CC = LOAD (match on the first tick, output stays low)
 *   duty 100% -> CC = 0    (output stays high) */
static uint16_t duty_to_cc(uint16_t duty_pct)
{
    return (uint16_t)(MOTOR_PWM_LOAD -
        (((uint32_t)duty_pct * MOTOR_PWM_LOAD) / 100U));
}

void tsp_motor_init(void)
{
    /* TIMA0 PWM (20kHz, CC0=PB3/CC1=PB4 for M1, CC2=PB0/CC3=PB2 for M2) is
     * configured by SysConfig in SYSCFG_DL_MOTOR_PWM_init(). The counter is
     * left stopped there so PWM only runs while a motor scene is active. */

    /* Force all four channels to 0% before the counter runs. CC survives
     * across scene exits, so starting without this makes the motor spin on
     * entry. */
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST,
        duty_to_cc(0), DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST,
        duty_to_cc(0), DL_TIMER_CC_1_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST,
        duty_to_cc(0), DL_TIMER_CC_2_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST,
        duty_to_cc(0), DL_TIMER_CC_3_INDEX);

    DL_TimerA_startCounter(MOTOR_PWM_INST);
}

void tsp_motor_set(uint8_t motor, uint8_t dir, uint16_t duty_pct)
{
    uint16_t dc;
    DL_TIMER_CC_INDEX cc_in1, cc_in2;

    if (motor > 1) return;

    /* Both half-bridges are PWM (IN1 and IN2); the idle side is parked at 0%
     * and only the driving side carries duty. That keeps both directions in
     * fast-decay mode -- with IN1 as a static GPIO the reverse direction fell
     * into slow decay, and the two wheels needed wildly different duty to
     * start moving (5% vs 55% measured on this chassis). */
    if (motor == MOTOR1) {
        cc_in2 = DL_TIMER_CC_0_INDEX;   /* PB3 */
        cc_in1 = DL_TIMER_CC_1_INDEX;   /* PB4 */
    } else {
        cc_in2 = DL_TIMER_CC_2_INDEX;   /* PB0 */
        cc_in1 = DL_TIMER_CC_3_INDEX;   /* PB2 */
    }

    switch (dir) {
    case MOTOR_FORWARD:
    case MOTOR_BACKWARD:
        dc = duty_pct;
        if (dc > 0U) dc += MOTOR_DEAD_ZONE;
        if (dc > MOTOR_DC_LIMIT) dc = MOTOR_DC_LIMIT;

        /* Mirrored mounting: MOTOR1 drives IN2 where MOTOR2 drives IN1, so
         * both move the vehicle the same way for a given dir. */
        if ((dir == MOTOR_FORWARD) == (motor == MOTOR1)) {
            DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(dc), cc_in2);
            DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(0),  cc_in1);
        } else {
            DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(0),  cc_in2);
            DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(dc), cc_in1);
        }
        break;

    case MOTOR_COAST:
        /* IN1=L, IN2=L -> Hi-Z */
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(0), cc_in2);
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(0), cc_in1);
        break;

    case MOTOR_BRAKE:
        /* IN1=H, IN2=H -> low-side brake */
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(100), cc_in2);
        DL_TimerA_setCaptureCompareValue(MOTOR_PWM_INST, duty_to_cc(100), cc_in1);
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
