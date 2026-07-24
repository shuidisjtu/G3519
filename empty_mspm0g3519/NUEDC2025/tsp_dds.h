/**
 * @file    tsp_dds.h
 * @brief   AD9833 DDS Waveform Generator Driver
 *
 * Hardware: GPIO bit-bang SPI (CPOL=1, MSB-first, falling-edge sample)
 *   PC2  = SCLK  (via J9 jumper -> AD9833 Pin 7)
 *   PC3  = SDATA (via J9 jumper -> AD9833 Pin 6)
 *   PC24 = FSYNC (via J9 jumper -> AD9833 Pin 8)
 *   MCLK = 25 MHz external oscillator (X1)
 *
 * Output: J12 (coaxial) / J22 (2-pin header)
 * WARNING: PC2/PC3 shared with CCD ADC — DDS and CCD cannot be used simultaneously.
 */

#ifndef _TSP_DDS_H
#define _TSP_DDS_H

#include "tsp_common_headfile.h"
#include "tsp_gpio.h"

/* ===== AD9833 constants ===== */

#define DDS_MCLK    25000000UL
#define DDS_FREQ_REG(f_hz) \
    ((uint32_t)(((uint64_t)(f_hz) * 268435456ULL + (DDS_MCLK / 2)) / DDS_MCLK))

#define AD9833_RESET     0x2100     /* B28=1, RESET=1 */
#define AD9833_FREQ0     0x4000     /* D15-D14=01 -> FREQ0 register select */
#define AD9833_SINE      0x2000     /* B28=1, OPBITEN=0, MODE=0 -> sine (DAC) */
#define AD9833_TRIANGLE  0x2002     /* B28=1, OPBITEN=0, MODE=1 -> triangle (DAC) */
#define AD9833_SQUARE    0x2028     /* B28=1, OPBITEN=1, DIV2=1 -> square (MSB) */

/* ===== Waveform metadata ===== */

#define DDS_WAVE_COUNT  3

extern const char * const tsp_dds_wave_names[];
extern const char * const tsp_dds_vout_info[];
extern const uint16_t     tsp_dds_wave_ctrl[];

/* ===== API ===== */

void     tsp_dds_write(uint16_t data);
void     tsp_dds_set_output(uint32_t freq_hz, uint16_t waveform_ctrl);
void     tsp_dds_stop(void);
uint32_t tsp_dds_get_step(uint32_t freq, int32_t delta);

#endif /* _TSP_DDS_H */
