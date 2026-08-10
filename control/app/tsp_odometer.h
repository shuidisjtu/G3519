/**
 * @file    tsp_odometer.h
 * @brief   Odometer — Distance & Angle measurement via wheel encoder
 *
 * Ported from HSP (HuashanPi) smart car project Odometer.h.
 * Adapted for MSPM0G3519 with dual-wheel QEI encoder (TIMG8/TIMG9).
 *
 * Uses tsp_wheel_enc driver for pulse counting (hardware quadrature decode).
 *
 * Modes:
 *   - ODOM_MODE_STRAIGHT: track linear distance (cm)
 *   - ODOM_MODE_ROTATE:   track rotation angle (degrees)
 *
 * Key mapping (DISTRIBUTED across buttons):
 *   S0: Straight mode     S1: Rotate mode
 *   S2: Reset counts      PUSH: Exit
 *
 * Calibration constants MUST be tuned for your specific robot:
 *   COUNTS_PER_CM: encoder pulses per centimeter of travel
 *   COUNTS_PER_DEGREE: encoder pulses per degree of rotation
 */

#ifndef TSP_ODOMETER_H_
#define TSP_ODOMETER_H_

#include "tsp_common_headfile.h"

/*===========================================================================
 * Calibration Constants — MUST be tuned for your robot
 *
 * Calibration procedure:
 *   COUNTS_PER_CM: Drive 100cm straight, read pulse count, divide by 100.
 *   COUNTS_PER_DEGREE: Pivot 360 degrees, read pulse count, divide by 360.
 *===========================================================================*/

/** Encoder counts per centimeter of straight-line travel */
#define ODOM_COUNTS_PER_CM       849U

/** Encoder counts per degree of in-place rotation */
#define ODOM_COUNTS_PER_DEGREE    79U

/*===========================================================================
 * Type Definitions
 *===========================================================================*/

/** Odometer operating mode */
typedef enum {
    ODOM_MODE_STRAIGHT = 0,   /* Track straight-line distance */
    ODOM_MODE_ROTATE   = 1    /* Track rotation angle */
} odometer_mode_t;

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief  Run odometer demo (blocking, own while(1) loop).
 *
 * Reads encoder counts from tsp_wheel_enc (right wheel QEI), computes
 * distance/angle, displays on LCD with incremental refresh.
 *
 * Key controls (distributed across all buttons):
 *   S0: Switch to Straight mode (distance tracking)
 *   S1: Switch to Rotate mode (angle tracking)
 *   S2: Reset all counts to zero
 *   PUSH: Exit back to menu
 *
 * LCD layout (8 lines):
 *   Line 0: Title bar (YELLOW on BLUE)
 *   Line 1: Mode indicator (STRAIGHT or ROTATE)
 *   Line 2: Distance value (cm, 1 decimal)
 *   Line 3: Angle value (deg, 1 decimal)
 *   Line 4: -------- divider --------
 *   Line 5: Raw encoder pulse count
 *   Line 6: Speed (pulses/20ms)
 *   Line 7: Hint bar (key legends)
 */
void tsp_odometer_demo(void);

#endif /* TSP_ODOMETER_H_ */
