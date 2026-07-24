#include "tsp_ccd.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"
#include <intrinsics.h>

/* ===== ADC12 Configuration =====
 * CCD analog input pins (verified against M0G3519 schematics):
 *   CCD1_AO -> PB18 (ADC1 CH5, J3)
 *   CCD2_AO -> PB17 (ADC1 CH4, J17)
 * ADC1 is configured manually (not via SysConfig).
 */

/* Exposure time between flush and snapshot (ms) */
static uint8_t g_ccd_exposure_ms = 10;

/* ─── Internal: ADC init ─── */

static const DL_ADC12_ClockConfig gCcdAdcClockConfig = {
    .clockSel    = DL_ADC12_CLOCK_ULPCLK,
    .divideRatio = DL_ADC12_CLOCK_DIVIDE_8,
    .freqRange   = DL_ADC12_CLOCK_FREQ_RANGE_24_TO_32,
};

static void ccd_adc_init(void)
{
    /* 1. Configure IOMUX for analog function on PB18 and PB17 */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM44);  /* PB18 -> ADC1 CH5 (CCD1/J3) */
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM43);  /* PB17 -> ADC1 CH4 (CCD2/J17) */

    /* 2. Reset and power up ADC1 (SDK standard sequence) */
    DL_ADC12_reset(ADC1);
    DL_ADC12_enablePower(ADC1);
    delay_cycles(POWER_STARTUP_DELAY);

    /* 3. Configure ADC clock */
    DL_ADC12_setClockConfig(ADC1,
        (DL_ADC12_ClockConfig *) &gCcdAdcClockConfig);

    /* 4. Init sequence-sample mode: MEM0→MEM1 per trigger, stop after one pass.
     *    Sequence mode ensures both CH5 (CCD1) and CH4 (CCD2) are converted
     *    on each software trigger, so ccd_adc_read_mem() can read either memory. */
    DL_ADC12_initSeqSample(ADC1,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_MANUAL,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_MEM_IDX_0,  /* startAdd: first conversion memory */
        DL_ADC12_MEM_IDX_1,  /* endAdd:   last conversion memory */
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);

    /* 5. Configure conversion memories:
     *    MEM0 = CCD1 (CH5/PB18), MEM1 = CCD2 (CH4/PB17)
     *    VREF = VDDA (3.3V), sample timer = SCOMP0, no averaging */
    DL_ADC12_configConversionMem(ADC1, DL_ADC12_MEM_IDX_0,
        DL_ADC12_INPUT_CHAN_5,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    DL_ADC12_configConversionMem(ADC1, DL_ADC12_MEM_IDX_1,
        DL_ADC12_INPUT_CHAN_4,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA_VSSA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);

    /* 6. Set sample time0 (SCOMP0). ~100 ticks for stable CCD analog read. */
    DL_ADC12_setSampleTime0(ADC1, 100);

    /* 7. Enable conversions */
    DL_ADC12_enableConversions(ADC1);
}

/* ─── Internal: read one ADC conversion memory (software trigger, blocking) ───
 * In sequence mode (MEM0→MEM1), startConversion() triggers a full pass through
 * both conversion memories. After BUSY clears, both MEM0 (CH5) and MEM1 (CH4)
 * hold fresh results. We simply read the requested memory — the other channel's
 * result is discarded (wasted conversion ~2us, acceptable for CCD debugging). */

#define CCD_ADC_TIMEOUT  10000U

static uint16_t ccd_adc_read_mem(DL_ADC12_MEM_IDX mem_idx)
{
    volatile uint32_t timeout = CCD_ADC_TIMEOUT;

    /* Start software-triggered sequence conversion (MEM0 then MEM1) */
    DL_ADC12_startConversion(ADC1);

    /* Wait for sequence complete (poll BUSY flag with timeout) */
    while (DL_ADC12_getStatus(ADC1) & DL_ADC12_STATUS_CONVERSION_ACTIVE) {
        if (--timeout == 0) {
            return 0;
        }
    }

    /* Read result from conversion memory */
    return (uint16_t)DL_ADC12_getMemResult(ADC1, mem_idx);
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
