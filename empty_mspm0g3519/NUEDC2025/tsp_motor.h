/**
 * @file    tsp_motor.h
 * @brief   DC Motor Driver for DRV8874 H-Bridge (PMODE=HIGH)
 *
 * Pin mapping (G3519 TIMA0 → expansion board J5 → DRV8874):
 *   Motor1: PB3(TIMA0_CCP0=PWM=IN2) + PB4(GPIO=DIR=IN1)
 *   Motor2: PB0(TIMA0_CCP2=PWM=IN2) + PB2(GPIO=DIR=IN1)
 *   nSLEEP: PB1 (shared, active HIGH) -- already defined in ti_msp_dl_config.h
 *   nFAULT: PA7 (shared, active LOW)  -- already defined in ti_msp_dl_config.h
 *
 * DRV8874 control scheme (PMODE=HIGH):
 *   Forward:  IN1=LOW,  IN2=PWM
 *   Backward: IN1=HIGH, IN2=PWM
 *   Coast:    IN1=LOW,  IN2=LOW
 *   Brake:    IN1=HIGH, IN2=HIGH
 *
 * Reference: D:\EDC26_HSPv2\Utilities\HSP_MOTOR.h
 */

#ifndef _TSP_MOTOR_H
#define _TSP_MOTOR_H

#include "tsp_common_headfile.h"
#include "tsp_gpio.h"

/* ===== Motor channel select ===== */
#define MOTOR1  0
#define MOTOR2  1

/* ===== Direction constants ===== */
#define MOTOR_FORWARD    0
#define MOTOR_BACKWARD   1
#define MOTOR_COAST      2    /* IN1=L, IN2=L -> Hi-Z coast */
#define MOTOR_BRAKE      3    /* IN1=H, IN2=H -> low-side brake */

/* ===== PWM parameters ===== */
#define MOTOR_PWM_PERIOD    3999U   /* TIMA0 period (0..3999), 80MHz/4000=20kHz */
#define MOTOR_DC_LIMIT      100U    /* max duty cycle percent */

/* ===== TIMA0 PWM instance ===== */
#define MOTOR_PWM_INST      TIMA0

/* ===== Motor PWM pin IOMUX (PB3=TIMA0_CCP0, PB0=TIMA0_CCP2) ===== */
#define MOTOR_PWM_C0_IOMUX       IOMUX_PINCM16
#define MOTOR_PWM_C0_IOMUX_FUNC  IOMUX_PINCM16_PF_TIMA0_CCP0
#define MOTOR_PWM_C2_IOMUX       IOMUX_PINCM12
#define MOTOR_PWM_C2_IOMUX_FUNC  IOMUX_PINCM12_PF_TIMA0_CCP2

/* ===== API ===== */

/**
 * @brief  Initialize TIMA0 PWM and motor driver
 * @note   Call after SYSCFG_DL_init(). Configures PB3/PB0 as TIMA0 CCP outputs,
 *         initializes edge-aligned PWM at 20kHz, starts counter.
 *         Caller must enable H-bridge via SLEEP_HIGH() before use
 *         and SLEEP_LOW() after (matching HSPv2 MEN_HIGH/MEN_LOW pattern).
 */
void tsp_motor_init(void);

/**
 * @brief  Set motor speed and direction
 * @param  motor    MOTOR1 or MOTOR2
 * @param  dir      MOTOR_FORWARD, MOTOR_BACKWARD, MOTOR_COAST, or MOTOR_BRAKE
 * @param  duty_pct Duty cycle 0-100 (%), clamped to MOTOR_DC_LIMIT
 */
void tsp_motor_set(uint8_t motor, uint8_t dir, uint16_t duty_pct);

/**
 * @brief  Stop motor (coast)
 * @param  motor  MOTOR1 or MOTOR2
 */
void tsp_motor_stop(uint8_t motor);

/**
 * @brief  Stop both motors
 */
void tsp_motor_stop_all(void);

/**
 * @brief  Check nFAULT status
 * @return 0 = OK (nFAULT HIGH), 1 = fault detected (nFAULT LOW)
 */
uint8_t tsp_motor_fault(void);

/**
 * @brief  Start PWM counter (already started in tsp_motor_init)
 */
void tsp_motor_pwm_start(void);

/**
 * @brief  Stop PWM counter
 */
void tsp_motor_pwm_stop(void);

#endif /* _TSP_MOTOR_H */
