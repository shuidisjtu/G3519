/**
 * @file    tsp_motor.h
 * @brief   DC Motor Driver for DRV8874 H-Bridge (PMODE=HIGH)
 *
 * Dual-PWM 4-channel architecture (TIMA0, SysConfig instance MOTOR_PWM):
 *   Motor1(right): PB3(CCP0=IN2) + PB4(CCP1=IN1)
 *   Motor2(left):  PB0(CCP2=IN2) + PB2(CCP3=IN1)
 *   nSLEEP: PB1 (shared, active HIGH)
 *   nFAULT: PA7 (shared, active LOW, 10k pull-up)
 *
 * Both directions use fast-decay (PWM-low = coast = IN1=L,IN2=L).
 * Mirrored mounting: tsp_motor_set() swaps IN1/IN2 roles per motor
 * so MOTOR_FORWARD always means vehicle forward.
 */

#ifndef TSP_MOTOR_H_
#define TSP_MOTOR_H_

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

/* ===== Chassis mounting =====
 * MOTOR1 drives the right wheel, MOTOR2 the left. They are mounted mirrored,
 * so the two motors swap which half-bridge carries the PWM for a given
 * direction -- see tsp_motor_set(). MOTOR_FORWARD always means the vehicle
 * moves forward. */
#define MOTOR_RIGHT      MOTOR1
#define MOTOR_LEFT       MOTOR2

/* ===== PWM parameters ===== */
/* Must stay in sync with PWM1.timerCount in empty_mspm0g3519.syscfg */
#define MOTOR_PWM_PERIOD    3999U   /* TIMA0 period, 80MHz/4000=20kHz */
/* DL_Timer_initTwoCCPWMMode() programs LOAD = period-1 (dl_timer.c:445), so
 * the counter tops out at 3998. A CC above LOAD is never matched, which leaves
 * the output stuck high (100%) -- CC must stay within 0..MOTOR_PWM_LOAD. */
#define MOTOR_PWM_LOAD      (MOTOR_PWM_PERIOD - 1U)
#define MOTOR_DC_LIMIT      99U     /* max duty cycle percent */
/* Duty consumed by static friction before the wheel turns. Measured on this
 * chassis by HSPv2 (Utilities/HSP_MOTOR.h); added to any non-zero request so
 * the caller's 0-100 maps onto the usable band. */
#define MOTOR_DEAD_ZONE     50U

/* ===== API ===== */

/**
 * @brief  Start TIMA0 PWM counter
 * @note   Call after SYSCFG_DL_init(). SysConfig configures TIMA0 (20kHz,
 *         CC0=PB3/M1, CC2=PB0/M2) but leaves the counter stopped.
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
