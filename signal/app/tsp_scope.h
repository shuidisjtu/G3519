#ifndef _TSP_SCOPE_H
#define _TSP_SCOPE_H

#include "tsp_common_headfile.h"

/* ===== Graph area layout (inside 160x128 TFT) ===== */
#define SCOPE_GRAPH_Y0    16      /* top of graph (below title row) */
#define SCOPE_GRAPH_Y1    111     /* bottom of graph (above status row) */
#define SCOPE_GRAPH_H     96      /* graph height in pixels */
#define SCOPE_WIDTH       160     /* display width = samples shown */
#define SCOPE_SAMPLE_N    320     /* total burst samples (extra for trigger search) */

/* ===== Timebase presets (delay param for tsp_adc_burst_sample) ===== */
#define SCOPE_TB_FAST     0       /* ~4.9us/sample, for >5kHz */
#define SCOPE_TB_MED      40      /* ~20us/sample,  for 500Hz~5kHz */
#define SCOPE_TB_SLOW     200     /* ~100us/sample, for <500Hz */

/* ===== API ===== */
void tsp_scope_clear(void);
void tsp_scope_draw_grid(void);
void tsp_scope_draw_wave(uint16_t *samples, uint16_t count);
void tsp_scope_vline(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color);

#endif
