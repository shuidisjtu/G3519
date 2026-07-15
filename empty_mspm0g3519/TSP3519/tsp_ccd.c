#include "tsp_ccd.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"

/* ===== ADC12 Configuration =====
 * CCD analog input pins (same as HSP):
 *   CCD1_AO -> PC2 (ADC0 CH12)
 *   CCD2_AO -> PC3 (ADC0 CH13)
 * ADC0 is configured manually (not via SysConfig).
 */

/* Exposure time between flush and snapshot (ms) */
static uint8_t g_ccd_exposure_ms = 10;

/* ─── Internal: ADC init ─── */

static void ccd_adc_init(void)
{
    /* 1. Configure IOMUX for analog function on PC2 and PC3 */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM80);  /* PC2 -> ADC0 CH12 */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM81);  /* PC3 -> ADC0 CH13 */

    /* 2. Power up ADC0 */
    DL_ADC12_enablePower(ADC0);

    /* 3. Init single-sample mode: stop after one, manual sample, sw trigger */
    DL_ADC12_initSingleSample(ADC0,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_MANUAL,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    /* 4. Configure conversion memories:
     *    MEM0 = CCD1 (CH12), MEM1 = CCD2 (CH13)
     *    VREF = internal, sample timer = SCOMP0, no averaging */
    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_0,
        DL_ADC12_INPUT_CHAN_12,
        DL_ADC12_REFERENCE_VOLTAGE_INTREF_VREFM,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_configConversionMem(ADC0, DL_ADC12_MEM_IDX_1,
        DL_ADC12_INPUT_CHAN_13,
        DL_ADC12_REFERENCE_VOLTAGE_INTREF_VREFM,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    /* 5. Set sample time0 (SCOMP0). ~100 ticks for stable CCD analog read.
     *    SCOMP0 clock = ULPCLK / (SCOMP0_DIV+1) ~= several MHz.
     *    100 ticks = ~10-20us sample window. */
    DL_ADC12_setSampleTime0(ADC0, 100);

    /* 6. Enable conversions */
    DL_ADC12_enableConversions(ADC0);
}

/* ─── Internal: read one ADC conversion memory (software trigger, blocking) ─── */

static uint16_t ccd_adc_read_mem(DL_ADC12_MEM_IDX mem_idx)
{
    /* Start software-triggered conversion */
    DL_ADC12_startConversion(ADC0);

    /* Wait for conversion complete (poll BUSY flag) */
    while (DL_ADC12_getStatus(ADC0) & DL_ADC12_STATUS_CONVERSION_ACTIVE) {
        /* busy-wait */
    }

    /* Read result from conversion memory */
    return (uint16_t)DL_ADC12_getMemResult(ADC0, mem_idx);
}

/* ─── Internal: CCD GPIO bit-bang helpers ─── */

static void ccd_si_high(uint8_t ccd_id) {
    if (ccd_id == CCD1) { CCD_SI1_HIGH; } else { CCD_SI2_HIGH; }
}
static void ccd_si_low(uint8_t ccd_id) {
    if (ccd_id == CCD1) { CCD_SI1_LOW; } else { CCD_SI2_LOW; }
}
static void ccd_clk_high(uint8_t ccd_id) {
    if (ccd_id == CCD1) { CCD_CLK1_HIGH; } else { CCD_CLK2_HIGH; }
}
static void ccd_clk_low(uint8_t ccd_id) {
    if (ccd_id == CCD1) { CCD_CLK1_LOW; } else { CCD_CLK2_LOW; }
}

/* ADC memory index per CCD */
static DL_ADC12_MEM_IDX ccd_adc_mem_idx(uint8_t ccd_id)
{
    return (ccd_id == CCD1) ? DL_ADC12_MEM_IDX_0 : DL_ADC12_MEM_IDX_1;
}

/* ─── Public API ─── */

void tsp_ccd_init(void)
{
    ccd_adc_init();

    /* Initialize CCD control pins to idle state */
    CCD_SI1_LOW;
    CCD_CLK1_LOW;
    CCD_SI2_LOW;
    CCD_CLK2_LOW;
}

void tsp_ccd_delay(void)
{
    /* Software delay — tuned for 80MHz CPUCLK (~1-2us).
     * HSP used count=160 at 240MHz -> ~1.3us.
     * At 80MHz, count≈60 yields similar delay. */
    volatile uint16_t cnt = 60;
    while (cnt--) { __no_operation(); }
}

void tsp_ccd_set_exposure(uint8_t ms)
{
    if (ms < 1) ms = 1;
    if (ms > 100) ms = 100;
    g_ccd_exposure_ms = ms;
}

void tsp_ccd_flush(uint8_t ccd_id)
{
    uint8_t i;

    /* ─── Start integration cycle ─── */
    ccd_si_high(ccd_id);
    tsp_ccd_delay();

    ccd_clk_high(ccd_id);
    tsp_ccd_delay();

    ccd_si_low(ccd_id);
    tsp_ccd_delay();

    ccd_clk_low(ccd_id);

    /* ─── Clock out 128 pixels to clear CCD shift register ─── */
    for (i = 0; i < CCD_PIXELS; i++) {
        tsp_ccd_delay();
        tsp_ccd_delay();
        ccd_clk_high(ccd_id);
        tsp_ccd_delay();
        tsp_ccd_delay();
        ccd_clk_low(ccd_id);
    }
}

void tsp_ccd_snapshot(uint8_t ccd_id, ccd_data_t pixels)
{
    uint8_t          i;
    DL_ADC12_MEM_IDX mem_idx = ccd_adc_mem_idx(ccd_id);

    /* 1. Flush previously integrated frame */
    tsp_ccd_flush(ccd_id);

    /* 2. Wait for new integration (exposure time) */
    delay_1ms(g_ccd_exposure_ms);

    /* 3. Start readout: SI pulse */
    ccd_si_high(ccd_id);
    tsp_ccd_delay();

    ccd_clk_high(ccd_id);
    tsp_ccd_delay();

    ccd_si_low(ccd_id);
    tsp_ccd_delay();

    ccd_clk_low(ccd_id);

    /* 4. Clock out 128 pixels, reading ADC after each CLK low period */
    for (i = 0; i < CCD_PIXELS; i++) {
        tsp_ccd_delay();

        /* Read ADC during CLK low (pixel output stable) */
        pixels[i] = ccd_adc_read_mem(mem_idx);

        /* Clock pulse to shift next pixel */
        ccd_clk_high(ccd_id);
        tsp_ccd_delay();
        ccd_clk_low(ccd_id);
    }

    /* 5. 129th clock pulse — terminate output of 128th pixel */
    ccd_clk_high(ccd_id);
    tsp_ccd_delay();
    ccd_clk_low(ccd_id);
}
