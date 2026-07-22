#ifndef _TSP_AD5933_H
#define _TSP_AD5933_H

#include "tsp_common_headfile.h"

/* AD5933 I2C slave address (7-bit) */
#define AD5933_I2C_ADDR              0x0D

/* Register addresses */
#define AD5933_REG_CTRL_H            0x80
#define AD5933_REG_CTRL_L            0x81
#define AD5933_REG_START_FREQ_H      0x82
#define AD5933_REG_START_FREQ_M      0x83
#define AD5933_REG_START_FREQ_L      0x84
#define AD5933_REG_FREQ_INCR_H       0x85
#define AD5933_REG_FREQ_INCR_M       0x86
#define AD5933_REG_FREQ_INCR_L       0x87
#define AD5933_REG_NUM_INCR_H        0x88
#define AD5933_REG_NUM_INCR_L        0x89
#define AD5933_REG_SETTLING_H        0x8A
#define AD5933_REG_SETTLING_L        0x8B
#define AD5933_REG_STATUS            0x8F
#define AD5933_REG_TEMP_H            0x92
#define AD5933_REG_TEMP_L            0x93
#define AD5933_REG_REAL_H            0x94
#define AD5933_REG_REAL_L            0x95
#define AD5933_REG_IMAG_H            0x96
#define AD5933_REG_IMAG_L            0x97

/* Control register commands (CTRL_H D15-D12, 16-bit for consistent >>8 with VOLT/PGA) */
#define AD5933_CTRL_INIT_FREQ        0x1000  /* Initialize with start frequency */
#define AD5933_CTRL_START_SWEEP      0x2000  /* Start frequency sweep */
#define AD5933_CTRL_INCREMENT_FREQ   0x3000  /* Increment frequency */
#define AD5933_CTRL_REPEAT_FREQ      0x4000  /* Repeat current frequency */
#define AD5933_CTRL_MEASURE_TEMP     0x9000  /* Measure temperature */
#define AD5933_CTRL_POWER_DOWN       0xA000  /* Power-down mode */
#define AD5933_CTRL_STANDBY          0xB000  /* Standby mode */

/* Output voltage range (CTRL_H D10-D9) */
#define AD5933_VOLT_2000MV           0x0000  /* 2.0 V p-p */
#define AD5933_VOLT_200MV            0x0200  /* 0.2 V p-p */
#define AD5933_VOLT_400MV            0x0400  /* 0.4 V p-p */
#define AD5933_VOLT_1000MV           0x0600  /* 1.0 V p-p */

/* PGA gain (CTRL_H D8): datasheet Table 7 — D8=1 means ×1, D8=0 means ×5 */
#define AD5933_PGA_X1                0x0100
#define AD5933_PGA_X5                0x0000

/* Internal clock (CTRL_L D3) */
#define AD5933_CLK_INTERNAL          0x0000
#define AD5933_CLK_EXTERNAL          0x0008

/* Reset (CTRL_L D4) */
#define AD5933_RESET                 0x0010

/* Settling time multiplier (SETTLING_H D10:D9) */
#define AD5933_SETTLE_X1             0x0000
#define AD5933_SETTLE_X2             0x0200
#define AD5933_SETTLE_X4             0x0600

/* Status register bits */
#define AD5933_STATUS_TEMP_VALID     0x01
#define AD5933_STATUS_DATA_VALID     0x02
#define AD5933_STATUS_SWEEP_DONE     0x04

/* I2C timeout (cycles for polling loops) */
#define AD5933_I2C_TIMEOUT  0xA000UL

/* AD5933 API */
void    tsp_ad5933_init(void);
uint8_t tsp_ad5933_read_reg(uint8_t reg_addr);
void    tsp_ad5933_write_reg(uint8_t reg_addr, uint8_t data);
void    tsp_ad5933_write_block(uint8_t reg_addr, uint8_t *data, uint8_t len);
uint8_t tsp_ad5933_read_status(void);
float   tsp_ad5933_read_temperature(void);
void    tsp_ad5933_set_sweep(uint32_t start_hz, uint32_t delta_hz,
                             uint16_t num_increments, uint16_t settling_cycles,
                             uint16_t settle_multiplier);
void    tsp_ad5933_start_sweep(void);
int16_t tsp_ad5933_read_real(void);
int16_t tsp_ad5933_read_imag(void);

#endif
