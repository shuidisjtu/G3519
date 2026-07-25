#include "tsp_ad8302.h"

static uint8_t g_ad8302_inited = 0;

void tsp_ad8302_init(void)
{
    if (!g_ad8302_inited) {
        tsp_adc_init();
        g_ad8302_inited = 1;
    }
}

void tsp_ad8302_read(tsp_ad8302_result_t *out)
{
    tsp_adc_select_channel(AD8302_CH_VMAG);
    out->vmag_mv = tsp_adc_read_avg_mv(8);

    tsp_adc_select_channel(AD8302_CH_VPHS);
    out->vphs_mv = tsp_adc_read_avg_mv(8);

    out->gain_db  = ((float)out->vmag_mv - AD8302_VMAG_CENTER_MV) /
                    (float)AD8302_VMAG_SLOPE_MV;
    out->phase_deg = ((float)AD8302_VPHS_ZERO_MV - (float)out->vphs_mv) /
                     (float)AD8302_VPHS_SLOPE_MV;
}
