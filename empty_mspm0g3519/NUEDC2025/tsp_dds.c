/**
 * @file    tsp_dds.c
 * @brief   AD9833 DDS Waveform Generator Driver
 *
 * Hardware:
 *   G3519 GPIO bit-bang -> J9 jumpers -> AD9833 (U4)
 *   PC2=SCLK, PC3=SDATA, PC24=FSYNC, MCLK=25MHz (X1)
 *
 * Output: J12 (coaxial) / J22 (2-pin header)
 */

#include "tsp_dds.h"

/* ===== Waveform metadata ===== */

const char * const tsp_dds_wave_names[] = {
    "Square", "Sine", "Triangle"
};

const char * const tsp_dds_vout_info[] = {
    "Vout ~3.1V (MSB)",
    "Vout ~0.6Vpp (DAC)",
    "Vout ~0.6Vpp (DAC)",
};

const uint16_t tsp_dds_wave_ctrl[] = {
    AD9833_SQUARE, AD9833_SINE, AD9833_TRIANGLE
};

/* ===== Core functions ===== */

void tsp_dds_write(uint16_t data)
{
    uint8_t i;
    /* AD9833 datasheet: SCLK idles HIGH (CPOL=1).
     * FSYNC falling edge must occur while SCLK is HIGH.
     * AD9833 samples SDATA on every SCLK falling edge. */
    DDS_FSYNC_HIGH();
    DDS_SCLK_HIGH();
    DDS_SDATA_LOW();
    DDS_FSYNC_LOW();
    for (i = 0; i < 16; i++) {
        if (data & 0x8000) {
            DDS_SDATA_HIGH();
        } else {
            DDS_SDATA_LOW();
        }
        DDS_SCLK_LOW();       /* Falling edge: AD9833 samples SDATA */
        DDS_SCLK_HIGH();      /* Rising  edge: prepare next bit */
        data <<= 1;
    }
    DDS_SDATA_LOW();
    DDS_FSYNC_HIGH();
    /* SCLK stays HIGH (idle per CPOL=1) */
}

void tsp_dds_set_output(uint32_t freq_hz, uint16_t waveform_ctrl)
{
    uint32_t reg;
    uint16_t lsb, msb;

    reg = DDS_FREQ_REG(freq_hz);
    lsb = AD9833_FREQ0 | ((uint16_t)(reg & 0x3FFF));
    msb = AD9833_FREQ0 | ((uint16_t)((reg >> 14) & 0x3FFF));

    tsp_dds_write(AD9833_RESET);
    tsp_dds_write(lsb);
    tsp_dds_write(msb);
    tsp_dds_write(waveform_ctrl);
}

void tsp_dds_stop(void)
{
    tsp_dds_write(AD9833_RESET);
}

uint32_t tsp_dds_get_step(uint32_t freq, int32_t delta)
{
    uint32_t base;
    if      (freq < 1000)  base = 10;
    else if (freq < 10000) base = 100;
    else                   base = 1000;   /* 10 kHz ~ 50 kHz */

    /* Fast rotation: coarser step */
    if (delta >= 3 || delta <= -3) base *= 10;

    return base;
}
