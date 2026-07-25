#include "tsp_fft.h"
#include "tsp_adc.h"
#include "tsp_isr.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include <math.h>

static q15_t g_fft_buf[FFT_N * 2];
static q15_t g_mag_buf[FFT_N];

void tsp_fft_analyze(uint8_t adc_ch, uint16_t speed, tsp_fft_result_t *out)
{
    uint16_t i;
    uint32_t sum = 0;
    uint16_t dc_raw;
    uint32_t t_start, t_end;

    /* Select ADC channel */
    tsp_adc_select_channel(adc_ch);

    /* Burst sample with timing measurement */
    t_start = sys_tick_counter;
    uint16_t *raw = tsp_adc_burst_sample(FFT_N, speed);
    t_end = sys_tick_counter;

    /* Compute actual sample rate from elapsed time */
    uint32_t elapsed_ms = t_end - t_start;
    if (elapsed_ms == 0) elapsed_ms = 1;
    out->fs_hz = ((uint32_t)FFT_N * 1000) / elapsed_ms;

    /* DC offset + Vpp from time-domain raw samples */
    uint16_t vmax = 0, vmin = 4095;
    for (i = 0; i < FFT_N; i++) {
        sum += raw[i];
        if (raw[i] > vmax) vmax = raw[i];
        if (raw[i] < vmin) vmin = raw[i];
    }
    dc_raw = (uint16_t)(sum / FFT_N);
    out->dc_mv = (uint16_t)(((uint32_t)dc_raw * ADC_VREF_MV) / ADC_RESOLUTION);
    out->amp_mv = (uint16_t)(((uint32_t)(vmax - vmin) * ADC_VREF_MV) / (ADC_RESOLUTION * 2));

    /* Convert to Q15 complex: real = (raw - dc) << 3, imag = 0 */
    for (i = 0; i < FFT_N; i++) {
        int16_t centered = (int16_t)raw[i] - (int16_t)dc_raw;
        g_fft_buf[2 * i]     = (q15_t)(centered << 3);
        g_fft_buf[2 * i + 1] = 0;
    }

    /* 256-point CFFT in-place */
    arm_cfft_q15(&arm_cfft_sR_q15_len256, g_fft_buf, 0, 1);

    /* Magnitude spectrum */
    arm_cmplx_mag_q15(g_fft_buf, g_mag_buf, FFT_N);

    /* Find peak in bins 1..N/2-1 (skip DC bin 0) */
    q15_t max_val;
    uint32_t max_idx;
    arm_max_q15(&g_mag_buf[1], FFT_N / 2 - 1, &max_val, &max_idx);
    max_idx += 1;  /* adjust for skipped DC bin */
    out->peak_bin = (uint16_t)max_idx;

    /* Parabolic interpolation for sub-bin frequency accuracy */
    float delta = 0.0f;
    if (max_idx > 1 && max_idx < FFT_N / 2 - 1) {
        float alpha = (float)g_mag_buf[max_idx - 1];
        float beta  = (float)g_mag_buf[max_idx];
        float gamma = (float)g_mag_buf[max_idx + 1];
        float denom = alpha - 2.0f * beta + gamma;
        if (denom != 0.0f) {
            delta = 0.5f * (alpha - gamma) / denom;
        }
    }
    float true_bin = (float)max_idx + delta;
    out->freq_x10 = (uint32_t)(true_bin * (float)out->fs_hz * 10.0f / (float)FFT_N + 0.5f);

    /* THD: harmonics 2~8 relative to fundamental */
    {
        uint32_t h1_sq = (uint32_t)max_val * (uint32_t)max_val;
        uint32_t harm_sq = 0;
        uint8_t h;
        for (h = 2; h <= 8; h++) {
            uint16_t hbin = (uint16_t)(max_idx * h);
            if (hbin >= FFT_N / 2) break;
            uint32_t hv = (uint32_t)g_mag_buf[hbin];
            harm_sq += hv * hv;
        }
        if (h1_sq > 0) {
            /* THD = sqrt(sum(Hn^2)) / H1 * 100, in 0.1% units */
            float thd = sqrtf((float)harm_sq / (float)h1_sq) * 1000.0f;
            out->thd_x10 = (uint16_t)(thd + 0.5f);
        } else {
            out->thd_x10 = 0;
        }
    }
}
