#ifndef _TSP_AD8302_H
#define _TSP_AD8302_H

#include "tsp_adc.h"

/* ===== Channel mapping (J14 -> J2 fly-wire) ===== */
#define AD8302_CH_VMAG   ADC_CH_VIN4   /* J14-1 -> J2 VIN4, PB24, ADC0-CH5  */
#define AD8302_CH_VPHS   ADC_CH_VIN5   /* J14-5 -> J2 VIN5, PA23, ADC1-CH12 */

/* ===== Transfer function constants (AD8302 datasheet) ===== */
#define AD8302_VMAG_CENTER_MV   900    /* 0 dB  -> 900 mV */
#define AD8302_VMAG_SLOPE_MV     30    /* 30 mV / dB      */
#define AD8302_VPHS_ZERO_MV    1800    /* 0 deg -> 1800 mV */
#define AD8302_VPHS_SLOPE_MV     10    /* 10 mV / deg      */

/* ===== Result structure ===== */
typedef struct {
    uint16_t vmag_mv;      /* raw VMAG voltage (mV) */
    uint16_t vphs_mv;      /* raw VPHS voltage (mV) */
    float    gain_db;      /* gain difference (dB), positive = INPA > INPB */
    float    phase_deg;    /* phase difference (0~180 degrees) */
} tsp_ad8302_result_t;

/* ===== API ===== */
void tsp_ad8302_init(void);
void tsp_ad8302_read(tsp_ad8302_result_t *out);

#endif
