/**
 * @file    tsp_pid.c
 * @brief   PID Controller implementation (Position & Incremental forms)
 *
 * Ported from HSP (HuashanPi) smart car project GD32F4xx platform.
 * Adapted for MSPM0G3519 (Cortex-M0+, software float OK).
 *
 * References:
 *   - HSP Ex6_pwm.c: hsp_demo_motor_closeloop() — incremental PID w/ D-on-PV
 *   - HSP Project1_LineFollower.c: hsp_image_judge4() — PD steering control
 */

#include "tsp_pid.h"

/* =========================================================================
 * Position-Form PID
 * ========================================================================= */

void tsp_pid_pos_init(pid_pos_t *pid, float kp, float ki, float kd,
                       float out_min, float out_max, float int_max)
{
    pid->kp           = kp;
    pid->ki           = ki;
    pid->kd           = kd;
    pid->error        = 0.0f;
    pid->error_prev   = 0.0f;
    pid->integral     = 0.0f;
    pid->integral_max = int_max;
    pid->output_min   = out_min;
    pid->output_max   = out_max;
}

float tsp_pid_pos_step(pid_pos_t *pid, float setpoint, float measurement)
{
    float error, derivative, output;

    /* Compute error */
    error = setpoint - measurement;

    /* Integrate with anti-windup (clamping) */
    pid->integral += error;
    if (pid->integral > pid->integral_max)
        pid->integral = pid->integral_max;
    else if (pid->integral < -pid->integral_max)
        pid->integral = -pid->integral_max;

    /* Derivative on error: use pid->error which holds e(k-1) */
    derivative = error - pid->error;

    /* PID output */
    output = pid->kp * error
           + pid->ki * pid->integral
           + pid->kd * derivative;

    /* Clamp output */
    if (output > pid->output_max)
        output = pid->output_max;
    else if (output < pid->output_min)
        output = pid->output_min;

    /* Store error history: e(k-1) -> error_prev, e(k) -> error */
    pid->error_prev = pid->error;
    pid->error      = error;

    return output;
}

void tsp_pid_pos_reset(pid_pos_t *pid)
{
    pid->error      = 0.0f;
    pid->error_prev = 0.0f;
    pid->integral   = 0.0f;
}

/* =========================================================================
 * Incremental-Form PID (D-on-PV)
 * ========================================================================= */

void tsp_pid_inc_init(pid_inc_t *pid, float kp, float ki, float kd,
                       float out_min, float out_max)
{
    pid->kp         = kp;
    pid->ki         = ki;
    pid->kd         = kd;
    pid->error[0]   = 0.0f;
    pid->error[1]   = 0.0f;
    pid->error[2]   = 0.0f;
    pid->pv[0]      = 0.0f;
    pid->pv[1]      = 0.0f;
    pid->pv[2]      = 0.0f;
    pid->output     = 0.0f;
    pid->output_min = out_min;
    pid->output_max = out_max;
}

float tsp_pid_inc_step(pid_inc_t *pid, float setpoint, float measurement, float dt)
{
    float error_now, du;

    /* Current error */
    error_now = setpoint - measurement;

    /*
     * Incremental PID with Derivative-on-Measurement (D-on-PV).
     *
     * Standard incremental:
     *   dU = Kp*(e(k)-e(k-1)) + Ki*e(k) - Kd*(PV(k)-2*PV(k-1)+PV(k-2))
     *
     * Using D-on-PV prevents derivative kick when setpoint changes abruptly.
     * The dt factor scales Ki term when called at non-unit intervals.
     */
    du = pid->kp * (error_now - pid->error[0])
       + pid->ki * error_now * dt
       - pid->kd * (measurement - 2.0f * pid->pv[0] + pid->pv[1]);

    /* Accumulate output */
    pid->output += du;

    /* Clamp output and implement anti-windup:
     * if saturated, don't accumulate further in that direction */
    if (pid->output > pid->output_max) {
        pid->output = pid->output_max;
    } else if (pid->output < pid->output_min) {
        pid->output = pid->output_min;
    }

    /* Shift histories */
    pid->error[2] = pid->error[1];
    pid->error[1] = pid->error[0];
    pid->error[0] = error_now;

    pid->pv[2] = pid->pv[1];
    pid->pv[1] = pid->pv[0];
    pid->pv[0] = measurement;

    return pid->output;
}

void tsp_pid_inc_reset(pid_inc_t *pid)
{
    pid->error[0] = 0.0f;
    pid->error[1] = 0.0f;
    pid->error[2] = 0.0f;
    pid->pv[0]    = 0.0f;
    pid->pv[1]    = 0.0f;
    pid->pv[2]    = 0.0f;
    pid->output   = 0.0f;
}
