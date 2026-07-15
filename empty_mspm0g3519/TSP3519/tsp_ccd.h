#ifndef _TSP_CCD_H
#define _TSP_CCD_H

#include "tsp_common_headfile.h"

/* ===== TSL1401 Linear CCD Driver for MSPM0G3519 =====
 * Supports dual CCD sensors (CCD1 and CCD2).
 * Adapted from HSP TSL1401 driver pattern:
 *   - Bit-banged SI/CLK via GPIO macros from tsp_gpio.h
 *   - ADC software-triggered read per pixel
 *   - Hardware integration time controlled by delay after flush
 */

#define CCD_PIXELS  128U              /* TSL1401 pixel count */

/* CCD channel IDs */
#define CCD1        1
#define CCD2        2

typedef uint16_t ccd_data_t[CCD_PIXELS];

/* Initialize ADC + GPIO for CCD reading */
void tsp_ccd_init(void);

/* Flush (clear) CCD integration without reading */
void tsp_ccd_flush(uint8_t ccd_id);

/* Capture one frame: flush → integrate → read 128 pixels */
void tsp_ccd_snapshot(uint8_t ccd_id, ccd_data_t pixels);

/* Set integration time in ms (default 10ms) */
void tsp_ccd_set_exposure(uint8_t ms);

/* Software delay for CCD timing (tuned per CPUCLK) */
void tsp_ccd_delay(void);

#endif
