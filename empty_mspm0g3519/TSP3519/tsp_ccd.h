#ifndef _TSP_CCD_H
#define _TSP_CCD_H

#include "tsp_common_headfile.h"

/* ===== 128-Pixel Linear CCD Driver for MSPM0G3519 =====
 * Supports dual CCD sensors (CCD1 and CCD2).
 * Pin mapping (verified against M0G3519 schematics):
 *   CCD1(J3):  AO=PB18(ADC1 CH5), SI=PC9,  CLK=PB20
 *   CCD2(J17): AO=PB17(ADC1 CH4), SI=PC4,  CLK=PC5
 * ADC1 sequence mode: MEM0→CH5, MEM1→CH4, manual sample, software trigger.
 */

#define CCD_PIXELS  128U              /* pixel count */

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
