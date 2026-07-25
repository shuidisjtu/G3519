/**
 * @file    tsp_pid.h
 * @brief   PID Controller Module (Incremental & Position form)
 *
 * Ported from HSP (HuashanPi) smart car project GD32F4xx platform.
 * Adapted for MSPM0G3519 Cortex-M0+ (no FPU, but soft-float is OK).
 *
 * Two PID forms:
 *   - Position PID:  output = Kp*error + Ki*integral + Kd*derivative
 *     Used for steering control (CCD line position -> PWM correction)
 *   - Incremental PID (D-on-PV):  dU = Kp*(e(k)-e(k-1)) + Ki*e(k) - Kd*(PV(k)-2*PV(k-1)+PV(k-2))
 *     Used for motor speed closed-loop control (avoids derivative kick)
 *
 * Key mapping (DISTRIBUTED across buttons):
 *   S0: increase, S1: decrease, S2: confirm/toggle, PUSH: exit
 *   Encoder knob: fine parameter adjustment
 */

#ifndef TSP_PID_H_
#define TSP_PID_H_

#include "tsp_common_headfile.h"

/*===========================================================================
 * Type Definitions
 *===========================================================================*/

/** Position-form PID state (for steering / line following) */
typedef struct {
    float kp;            /* Proportional gain */
    float ki;            /* Integral gain */
    float kd;            /* Derivative gain */
    float error;         /* Current error (setpoint - measurement) */
    float error_prev;    /* Previous error */
    float integral;      /* Accumulated integral */
    float integral_max;  /* Integral windup limit */
    float output_min;    /* Output lower clamp */
    float output_max;    /* Output upper clamp */
} pid_pos_t;

/** Incremental-form PID state (for motor speed control, D-on-PV) */
typedef struct {
    float kp;            /* Proportional gain */
    float ki;            /* Integral gain */
    float kd;            /* Derivative gain (applied to measurement) */
    float error[3];      /* error history: [0]=e(k), [1]=e(k-1), [2]=e(k-2) */
    float pv[3];         /* Process Variable history for D-on-PV */
    float output;        /* Accumulated output */
    float output_min;    /* Output lower clamp */
    float output_max;    /* Output upper clamp */
} pid_inc_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief  Initialize a position-form PID controller.
 * @param  pid   Pointer to pid_pos_t struct
 * @param  kp    Proportional gain
 * @param  ki    Integral gain (0 = no integral)
 * @param  kd    Derivative gain
 * @param  out_min  Minimum output clamp
 * @param  out_max  Maximum output clamp
 * @param  int_max  Integral windup limit
 */
void tsp_pid_pos_init(pid_pos_t *pid, float kp, float ki, float kd,
                       float out_min, float out_max, float int_max);

/**
 * @brief  Compute one iteration of position-form PID.
 * @param  pid         Pointer to pid_pos_t struct
 * @param  setpoint    Desired target value
 * @param  measurement Current measured value
 * @return PID output (clamped to [out_min, out_max])
 */
float tsp_pid_pos_step(pid_pos_t *pid, float setpoint, float measurement);

/**
 * @brief  Reset position PID state (clear integral and error history).
 * @param  pid  Pointer to pid_pos_t struct
 */
void tsp_pid_pos_reset(pid_pos_t *pid);

/**
 * @brief  Initialize an incremental-form PID controller (D-on-PV).
 * @param  pid      Pointer to pid_inc_t struct
 * @param  kp       Proportional gain
 * @param  ki       Integral gain
 * @param  kd       Derivative gain (applied to measurement)
 * @param  out_min  Minimum output clamp
 * @param  out_max  Maximum output clamp
 */
void tsp_pid_inc_init(pid_inc_t *pid, float kp, float ki, float kd,
                       float out_min, float out_max);

/**
 * @brief  Compute one iteration of incremental PID (D-on-PV).
 * @param  pid         Pointer to pid_inc_t struct
 * @param  setpoint    Desired target value
 * @param  measurement Current measured value
 * @param  dt          Time delta in seconds (used for Ki scaling; pass 1.0 if
 *                     called at fixed interval)
 * @return Accumulated PID output (clamped to [out_min, out_max])
 *
 * Formula (incremental, D-on-PV):
 *   dU = Kp*(e(k)-e(k-1)) + Ki*e(k)*dt - Kd*(PV(k)-2*PV(k-1)+PV(k-2))
 *   output += dU
 *
 * D-on-PV prevents derivative kick on setpoint changes.
 * Output is clamped and integrator is held when saturated (anti-windup).
 */
float tsp_pid_inc_step(pid_inc_t *pid, float setpoint, float measurement, float dt);

/**
 * @brief  Reset incremental PID state (clear output, error history, PV history).
 * @param  pid  Pointer to pid_inc_t struct
 */
void tsp_pid_inc_reset(pid_inc_t *pid);

#endif /* TSP_PID_H_ */
