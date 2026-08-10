/**
 * @file    tsp_linefollow.h
 * @brief   CCD-based Line Following — PD steering + speed control
 *
 * Ported from HSP (HuashanPi) Project1_LineFollower.c line tracking algorithm.
 * Adapted for MSPM0G3519 using CCD 128-pixel linear sensor instead of camera.
 *
 * Algorithm:
 *   1. Read 128-pixel CCD data via tsp_ccd_snapshot()
 *   2. Find black tape position: scan for dark region (threshold),
 *      validate width, compute center
 *   3. PD steering: correct = Kp_steer * error + Kd_steer * delta_error
 *      (IIR filtered for smooth actuation)
 *   4. PD speed:   dc     = base_dc + Kp_spd * spd_error + Kd_spd * delta_spd
 *      (calculated from encoder feedback, rate-limited)
 *   5. Differential drive: left = dc + correct, right = dc - correct
 *
 * Key mapping (DISTRIBUTED across buttons):
 *   S0: Speed -5         S1: Speed +5
 *   S2: Run/Stop toggle  PUSH: Exit
 */

#ifndef TSP_LINEFOLLOW_H_
#define TSP_LINEFOLLOW_H_

#include "tsp_common_headfile.h"
#include "tsp_ccd.h"
#include "tsp_pid.h"

/*===========================================================================
 * Calibration Constants — MUST be tuned for your track & robot
 *===========================================================================*/

/** ADC threshold for detecting black tape (12-bit ADC, lower = darker).
 *  Calibrate: read CCD with tape centered, set between tape_min and bg_min. */
#define LF_TAPE_THRESHOLD    1800U

/** Minimum valid tape width in pixels (rejects noise) */
#define LF_TAPE_WIDTH_MIN       2U

/** Maximum valid tape width in pixels (rejects > full track width) */
#define LF_TAPE_WIDTH_MAX      40U

/** CCD center pixel (128 pixels, center = 64) */
#define LF_CCD_CENTER          64

/** Default motor base speed (duty cycle %, 0-100) */
#define LF_DEFAULT_SPEED       30U

/** Speed adjustment step on S0/S1 press */
#define LF_SPEED_STEP           5U

/** Maximum steering correction (should match actuator range) */
#define LF_STEER_MAX           60

/** Motor duty cycle limits */
#define LF_DC_MAX              60
#define LF_DC_MIN             (-35)

/** Speed scale: duty% to approximate encoder pulses/20ms (calibrate!) */
#define LF_SPEED_SCALE         2.0f

/** Rate limit: max duty change per frame */
#define LF_DC_RATE_LIMIT       5

/*===========================================================================
 * PD Control Default Gains (from HSP, verified on HuashanPi)
 *===========================================================================*/

/** Steering PD: Kp proportional to line position error (pixels) */
#define LF_KP_STEER_DEF       1.8f

/** Steering PD: Kd derivative to error change rate */
#define LF_KD_STEER_DEF       5.0f

/** Speed PD: Kp proportional to speed error */
#define LF_KP_SPEED_DEF       0.15f

/** Speed PD: Kd derivative to speed error change rate */
#define LF_KD_SPEED_DEF       0.003f

/*===========================================================================
 * Line Detection Result
 *===========================================================================*/

typedef struct {
    uint8_t  line_valid;     /* 1 = line detected, 0 = lost */
    uint8_t  line_center;    /* pixel index of tape center (0-127) */
    uint8_t  tape_width;     /* width of detected tape in pixels */
    int16_t  steer_correct;  /* steering correction value */
    int16_t  dc_base;        /* base motor duty cycle (signed for braking) */
} lf_result_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief  Initialize line follower module.
 *
 * Initializes CCD sensor, motor driver, encoder, and PID controllers.
 * Sets default speed, default PD gains.
 * Caller must call SLEEP_HIGH() before using motors.
 */
void tsp_linefollow_init(void);

/**
 * @brief  Process one CCD frame and compute steering/speed control.
 * @param  ccd_ch   CCD channel (CCD1 or CCD2)
 * @param  result   Output: line detection and control result
 *
 * Steps:
 *   1. Capture CCD snapshot
 *   2. Scan for black tape (left/right edge detection)
 *   3. Validate tape width, compute center
 *   4. PD steering: compute correction from center error
 *   5. PD speed: compute duty from encoder speed feedback
 *   6. Fill result struct
 */
void tsp_linefollow_process(uint8_t ccd_ch, lf_result_t *result);

/**
 * @brief  Apply differential drive based on line follow result.
 * @param  result  Line follow result from tsp_linefollow_process()
 *
 * Left motor:  dc_left  = dc_base + steer_correct
 * Right motor: dc_right = dc_base - steer_correct
 *
 * Each motor channel is clamped to safe limits and deadband filtered.
 */
void tsp_linefollow_drive(const lf_result_t *result);

/**
 * @brief  Get current encoder speed (pulses per 20ms).
 * @return Speed value from tsp_encoder.
 */
int16_t tsp_linefollow_get_speed(void);

/**
 * @brief  Set base speed (duty cycle 0-100).
 * @param  speed  New base speed
 */
void tsp_linefollow_set_speed(uint8_t speed);

/**
 * @brief  Get current base speed.
 * @return Base speed duty cycle
 */
uint8_t tsp_linefollow_get_speed_val(void);

/**
 * @brief  Set steering PD gains at runtime.
 */
void tsp_linefollow_set_steer_gains(float kp, float kd);

/**
 * @brief  Set speed PD gains at runtime.
 */
void tsp_linefollow_set_speed_gains(float kp, float kd);

/**
 * @brief  Get current steering PD gains.
 */
void tsp_linefollow_get_steer_gains(float *kp, float *kd);

/**
 * @brief  Get current speed PD gains.
 */
void tsp_linefollow_get_speed_gains(float *kp, float *kd);

/**
 * @brief  Run line following demo (blocking, own while(1) loop).
 *
 * CCD continuous capture, line detection, differential drive.
 * LCD shows real-time: line position, tape width, speed, DC.
 *
 * Key controls (distributed across all buttons):
 *   S0: Speed -5%        S1: Speed +5%
 *   S2: Run/Stop toggle  PUSH: Exit
 */
void tsp_linefollow_demo(void);

#endif /* TSP_LINEFOLLOW_H_ */
