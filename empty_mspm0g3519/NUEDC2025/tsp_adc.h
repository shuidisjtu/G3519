#ifndef _TSP_ADC_H
#define _TSP_ADC_H

#include "tsp_common_headfile.h"

/* ===== Channel index (for tsp_adc_select_channel) ===== */
#define ADC_CH_VIN1    0    /* ADC0 CH2,  PA25, J2 pin 1 */
#define ADC_CH_VIN3    1    /* ADC0 CH3,  PA24, J2 pin 3 */
#define ADC_CH_VIN4    2    /* ADC0 CH5,  PB24, J2 pin 4 */
#define ADC_CH_COUNT   3

/* ===== Voltage conversion ===== */
#define ADC_VREF_MV    3300
#define ADC_RESOLUTION 4095

/* ===== Frequency measurement ===== */
#define ADC_FREQ_BUF_SIZE  512

/* ===== Channel metadata ===== */
typedef struct {
    uint32_t    dl_chan;
    const char *label;
    const char *detail;
} tsp_adc_channel_t;

extern const tsp_adc_channel_t tsp_adc_channels[ADC_CH_COUNT];

/* ===== Measurement result ===== */
typedef struct {
    uint32_t freq_hz;       /* 0 = DC or unmeasurable */
    uint16_t vpp_mv;        /* peak-to-peak in mV */
    uint16_t dc_mv;         /* DC offset (average) in mV */
    uint16_t vmax_mv;
    uint16_t vmin_mv;
} tsp_adc_meas_t;

/* ===== API ===== */
void     tsp_adc_init(void);
void     tsp_adc_select_channel(uint8_t ch_idx);
uint16_t tsp_adc_read_raw(void);
uint16_t tsp_adc_read_mv(void);
uint16_t tsp_adc_read_avg_mv(uint8_t n);
void     tsp_adc_measure(tsp_adc_meas_t *out);

#endif
