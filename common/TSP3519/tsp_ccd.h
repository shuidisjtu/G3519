#ifndef TSP_CCD_H_
#define TSP_CCD_H_

#include "tsp_common_headfile.h"

/* ===== 128-Pixel Linear CCD Driver for MSPM0G3519 =====
 * Supports 4 CCD sensors in 2 groups sharing SI/CLK:
 *   Group1: CCD1(PB18,CH5) + CCD2(PB19,CH6), SI1=PC9, CLK1=PB20
 *   Group2: CCD3(PB17,CH4) + CCD4(PA17,CH2), SI2=PC4, CLK2=PC5
 * ADC1 configured via SysConfig (instance: CCD_ADC).
 * Sequence mode: MEM0→CH5, MEM1→CH6, MEM2→CH4, MEM3→CH2.
 */

#define CCD_PIXELS  128U              /* pixel count */

/* CCD channel IDs */
#define CCD1        1
#define CCD2        2
#define CCD3        3
#define CCD4        4

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
