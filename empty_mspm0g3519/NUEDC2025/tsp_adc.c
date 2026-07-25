#include "tsp_adc.h"

/* ===== Channel lookup table ===== */
const tsp_adc_channel_t tsp_adc_channels[ADC_CH_COUNT] = {
    { DL_ADC12_INPUT_CHAN_2,  "VIN1", "ADC0-CH2  PA25" },
    { DL_ADC12_INPUT_CHAN_11, "VIN2", "ADC1-CH11 PB23" },
    { DL_ADC12_INPUT_CHAN_3,  "VIN3", "ADC0-CH3  PA24" },
    { DL_ADC12_INPUT_CHAN_5,  "VIN4", "ADC0-CH5  PB24" },
    { DL_ADC12_INPUT_CHAN_12, "VIN5", "ADC1-CH12 PA23" },
};

static ADC12_Regs *g_cur_adc = NULL;

static ADC12_Regs *adc_inst_for(uint8_t ch_idx)
{
    if (ch_idx == ADC_CH_VIN2 || ch_idx == ADC_CH_VIN5)
        return ADC12_1_INST;
    return ADC12_0_INST;
}

/* ===== Init: configure extra analog pins ===== */
void tsp_adc_init(void)
{
    /* SysConfig configures PA25 (PINCM55) for ADC0 CH2 and
     * PB23 (PINCM51) for ADC1 CH11.
     * Manually set the other pins as analog inputs. */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM54);  /* PA24, ADC0 CH3 */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM52);  /* PB24, ADC0 CH5 */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM53);  /* PA23, ADC1 CH12 */
}

/* ===== Select ADC channel ===== */
void tsp_adc_select_channel(uint8_t ch_idx)
{
    if (ch_idx >= ADC_CH_COUNT) return;

    g_cur_adc = adc_inst_for(ch_idx);

    DL_ADC12_disableConversions(g_cur_adc);

    DL_ADC12_configConversionMem(g_cur_adc, DL_ADC12_MEM_IDX_0,
        tsp_adc_channels[ch_idx].dl_chan,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_enableConversions(g_cur_adc);
}

/* ===== Single blocking read (12-bit) ===== */
uint16_t tsp_adc_read_raw(void)
{
    uint16_t result;

    DL_ADC12_startConversion(g_cur_adc);

    while (!DL_ADC12_getRawInterruptStatus(
               g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))
        ;

    DL_ADC12_clearInterruptStatus(
        g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    result = DL_ADC12_getMemResult(g_cur_adc, DL_ADC12_MEM_IDX_0);

    DL_ADC12_enableConversions(g_cur_adc);

    return result;
}

/* ===== Read voltage in mV ===== */
uint16_t tsp_adc_read_mv(void)
{
    uint16_t raw = tsp_adc_read_raw();
    return (uint16_t)(((uint32_t)raw * ADC_VREF_MV) / ADC_RESOLUTION);
}

/* ===== Average of n readings in mV ===== */
uint16_t tsp_adc_read_avg_mv(uint8_t n)
{
    uint32_t sum = 0;
    uint8_t i;
    if (n == 0) n = 1;
    for (i = 0; i < n; i++) {
        sum += tsp_adc_read_raw();
    }
    return (uint16_t)(((sum / n) * ADC_VREF_MV) / ADC_RESOLUTION);
}

/* ===== Frequency measurement: burst sample + zero-crossing ===== */

static uint16_t g_freq_buf[ADC_FREQ_BUF_SIZE];

static uint32_t freq_from_buf(uint16_t count, uint32_t sample_period_us_x10,
                              uint16_t *out_max, uint16_t *out_min, uint16_t *out_dc)
{
    uint32_t sum = 0;
    uint16_t dc_offset;
    uint16_t vmax = 0, vmin = 4095;
    int16_t  hyst;
    uint8_t  above;
    uint16_t cross_count = 0;
    uint16_t first_cross = 0, last_cross = 0;
    uint16_t i;

    for (i = 0; i < count; i++) {
        uint16_t s = g_freq_buf[i];
        sum += s;
        if (s > vmax) vmax = s;
        if (s < vmin) vmin = s;
    }
    dc_offset = (uint16_t)(sum / count);

    *out_max = vmax;
    *out_min = vmin;
    *out_dc  = dc_offset;

    hyst = 82;  /* ~2% of 4096 */

    above = (g_freq_buf[0] > dc_offset + hyst) ? 1 : 0;

    for (i = 1; i < count; i++) {
        if (!above && g_freq_buf[i] > (uint16_t)(dc_offset + hyst)) {
            above = 1;
            if (cross_count == 0) first_cross = i;
            last_cross = i;
            cross_count++;
        } else if (above && g_freq_buf[i] < (uint16_t)(dc_offset - hyst)) {
            above = 0;
        }
    }

    if (cross_count < 2) return 0;

    {
        uint32_t span = last_cross - first_cross;
        uint32_t periods = cross_count - 1;
        return (periods * 10000000UL) / (span * sample_period_us_x10);
    }
}

void tsp_adc_measure(tsp_adc_meas_t *out)
{
    uint16_t i;
    uint16_t vmax, vmin, dc_raw;

    /* Pass 1: fast burst (~4.9us/sample = 49 in 0.1us units) */
    for (i = 0; i < ADC_FREQ_BUF_SIZE; i++) {
        DL_ADC12_startConversion(g_cur_adc);
        while (!DL_ADC12_getRawInterruptStatus(
                   g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))
            ;
        DL_ADC12_clearInterruptStatus(
            g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        g_freq_buf[i] = DL_ADC12_getMemResult(g_cur_adc, DL_ADC12_MEM_IDX_0);
        DL_ADC12_enableConversions(g_cur_adc);
    }

    out->freq_hz = freq_from_buf(ADC_FREQ_BUF_SIZE, 49, &vmax, &vmin, &dc_raw);

    if (out->freq_hz == 0) {
        /* Pass 2: slow burst (~200us/sample = 2000 in 0.1us units) */
        for (i = 0; i < ADC_FREQ_BUF_SIZE; i++) {
            DL_ADC12_startConversion(g_cur_adc);
            while (!DL_ADC12_getRawInterruptStatus(
                       g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))
                ;
            DL_ADC12_clearInterruptStatus(
                g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
            g_freq_buf[i] = DL_ADC12_getMemResult(g_cur_adc, DL_ADC12_MEM_IDX_0);
            DL_ADC12_enableConversions(g_cur_adc);
            delay_cycles(15760);
        }
        out->freq_hz = freq_from_buf(ADC_FREQ_BUF_SIZE, 2000, &vmax, &vmin, &dc_raw);
    }

    out->vmax_mv = (uint16_t)(((uint32_t)vmax * ADC_VREF_MV) / ADC_RESOLUTION);
    out->vmin_mv = (uint16_t)(((uint32_t)vmin * ADC_VREF_MV) / ADC_RESOLUTION);
    out->vpp_mv  = out->vmax_mv - out->vmin_mv;
    out->dc_mv   = (out->vmax_mv + out->vmin_mv) / 2;
}

uint16_t *tsp_adc_burst_sample(uint16_t count, uint16_t delay)
{
    uint16_t i;
    if (count > ADC_FREQ_BUF_SIZE) count = ADC_FREQ_BUF_SIZE;

    for (i = 0; i < count; i++) {
        DL_ADC12_startConversion(g_cur_adc);
        while (!DL_ADC12_getRawInterruptStatus(
                   g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED))
            ;
        DL_ADC12_clearInterruptStatus(
            g_cur_adc, DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);
        g_freq_buf[i] = DL_ADC12_getMemResult(g_cur_adc, DL_ADC12_MEM_IDX_0);
        DL_ADC12_enableConversions(g_cur_adc);
        if (delay > 0) delay_cycles((uint32_t)delay * 80);
    }

    return (uint16_t *)g_freq_buf;
}
