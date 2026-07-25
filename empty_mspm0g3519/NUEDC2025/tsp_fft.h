#ifndef _TSP_FFT_H
#define _TSP_FFT_H

#include "tsp_common_headfile.h"

#define FFT_N  256

/* Sample rate presets (delay_cycles multiplier for tsp_adc_burst_sample) */
#define FFT_FS_FAST    0     /* ~204kSPS, Nyquist ~102kHz, df~800Hz */
#define FFT_FS_MED     200   /* ~20kSPS,  Nyquist ~10kHz,  df~78Hz  */
#define FFT_FS_SLOW    800   /* ~5kSPS,   Nyquist ~2.5kHz, df~20Hz  */

typedef struct {
    uint32_t freq_x10;      /* frequency in 0.1 Hz (interpolated) */
    uint16_t amp_mv;        /* fundamental amplitude mV (peak) */
    uint16_t thd_x10;       /* THD in 0.1% (e.g. 125 = 12.5%) */
    uint16_t dc_mv;         /* DC offset mV */
    uint16_t peak_bin;      /* raw peak bin index */
    uint32_t fs_hz;         /* actual sample rate Hz */
} tsp_fft_result_t;

void tsp_fft_analyze(uint8_t adc_ch, uint16_t speed, tsp_fft_result_t *out);

#endif
