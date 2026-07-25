#include "tsp_ccd.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"
#include <intrinsics.h>

/* ===== ADC12 Configuration =====
 * CCD analog input pins (per board schematic):
 *   CCD1_AO -> PB18 (ADC1 CH5)    Group1: SI1(PC9), CLK1(PB20)
 *   CCD2_AO -> PB19 (ADC1 CH6)    Group1: SI1(PC9), CLK1(PB20)
 *   CCD3_AO -> PB17 (ADC1 CH4)    Group2: SI2(PC4), CLK2(PC5)
 *   CCD4_AO -> PA17 (ADC1 CH2)    Group2: SI2(PC4), CLK2(PC5)
 * ADC1 is configured via SysConfig (instance: CCD_ADC).
 * Sequence mode: MEM0→CH5, MEM1→CH6, MEM2→CH4, MEM3→CH2.
 */

/* Exposure time between flush and snapshot (ms) */
static uint8_t g_ccd_exposure_ms = 10;

/* ─── Internal: read one ADC conversion memory (software trigger, blocking) ───
 * Sequence mode (MEM0→MEM1): each startConversion() converts both channels.
 * We read the requested memory; the other channel's result is discarded. */

#define CCD_ADC_TIMEOUT  10000U

static uint16_t ccd_adc_read_mem(DL_ADC12_MEM_IDX mem_idx)
{
    volatile uint32_t timeout = CCD_ADC_TIMEOUT;

    /* Re-arm ENC bit — hardware auto-clears it after each non-repeat sequence */
    DL_ADC12_enableConversions(CCD_ADC_INST);
    DL_ADC12_startConversion(CCD_ADC_INST);

    while (DL_ADC12_getStatus(CCD_ADC_INST) & DL_ADC12_STATUS_CONVERSION_ACTIVE) {
        if (--timeout == 0) {
            return 0;
        }
    }

    return (uint16_t)DL_ADC12_getMemResult(CCD_ADC_INST, mem_idx);
}

/* ─── Internal: CCD GPIO bit-bang helpers ─── */

static void ccd_si_high(uint8_t ccd_id) {
    if (ccd_id <= CCD2) { CCD_SI1_HIGH; } else { CCD_SI2_HIGH; }
}
static void ccd_si_low(uint8_t ccd_id) {
    if (ccd_id <= CCD2) { CCD_SI1_LOW; } else { CCD_SI2_LOW; }
}
static void ccd_clk_high(uint8_t ccd_id) {
    if (ccd_id <= CCD2) { CCD_CLK1_HIGH; } else { CCD_CLK2_HIGH; }
}
static void ccd_clk_low(uint8_t ccd_id) {
    if (ccd_id <= CCD2) { CCD_CLK1_LOW; } else { CCD_CLK2_LOW; }
}

/* ADC memory index per CCD channel */
static DL_ADC12_MEM_IDX ccd_adc_mem_idx(uint8_t ccd_id)
{
    switch (ccd_id) {
    case CCD1: return DL_ADC12_MEM_IDX_0;  /* CH5, PB18 */
    case CCD2: return DL_ADC12_MEM_IDX_1;  /* CH6, PB19 */
    case CCD3: return DL_ADC12_MEM_IDX_2;  /* CH4, PB17 */
    case CCD4: return DL_ADC12_MEM_IDX_3;  /* CH2, PA17 */
    default:   return DL_ADC12_MEM_IDX_0;
    }
}

/* ─── Public API ─── */

void tsp_ccd_init(void)
{
    /* ADC0 is initialized by SYSCFG_DL_init() (SysConfig instance: CCD_ADC) */

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
