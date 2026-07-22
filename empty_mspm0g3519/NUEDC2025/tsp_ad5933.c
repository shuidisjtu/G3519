#include "tsp_ad5933.h"
#include <math.h>

/* Forward declare: driverlib I2C instance from SysConfig */
/* I2C_AD5933_INST and delay_cycles are defined in ti_msp_dl_config.h */

/* Busy-wait for ~cycles CPU cycles.
 * Rough calibration: each loop iteration = ~3 cycles on M0+ at 80MHz. */
static void ad5933_delay_cycles(uint32_t cycles)
{
    volatile uint32_t i;
    for (i = 0; i < (cycles / 3); i++) { __NOP(); }
}

#define I2C_TIMEOUT  AD5933_I2C_TIMEOUT

static uint8_t i2c_wait_idle(void);
static uint8_t i2c_wait_done(void);

/* --- I2C helper: block write to AD5933 (reg + N bytes in one transaction) --- */
void tsp_ad5933_write_block(uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    uint8_t tx[9];
    uint8_t i;
    tx[0] = reg_addr;
    for (i = 0; i < len && i < 8; i++) tx[i + 1] = data[i];

    /* Reset controller state before new transfer */
    DL_I2C_resetControllerTransfer(I2C_AD5933_INST);

    /* Wait for idle */
    {
        volatile uint32_t w = I2C_TIMEOUT;
        while (w && !(DL_I2C_getControllerStatus(I2C_AD5933_INST) &
                      DL_I2C_CONTROLLER_STATUS_IDLE)) w--;
    }

    DL_I2C_fillControllerTXFIFO(I2C_AD5933_INST, tx, len + 1);

    DL_I2C_startControllerTransfer(I2C_AD5933_INST, AD5933_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, len + 1);

    ad5933_delay_cycles(500);
    i2c_wait_done();
}

void tsp_ad5933_write_reg(uint8_t reg_addr, uint8_t data)
{
    uint8_t d = data;
    tsp_ad5933_write_block(reg_addr, &d, 1);
}

/* --- I2C helper: write one byte to AD5933 register --- */
static uint8_t i2c_wait_idle(void)
{
    volatile uint32_t to = I2C_TIMEOUT;
    while (to) {
        uint32_t s = DL_I2C_getControllerStatus(I2C_AD5933_INST);
        if (s & DL_I2C_CONTROLLER_STATUS_IDLE) return 0;
        /* If controller is stuck in error, reset it */
        if (s & DL_I2C_CONTROLLER_STATUS_ERROR) {
            DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
        }
        to--;
    }
    /* Force reset on timeout */
    DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
    return 1;
}

static uint8_t i2c_wait_done(void)
{
    volatile uint32_t to = I2C_TIMEOUT;
    while (to) {
        uint32_t s = DL_I2C_getControllerStatus(I2C_AD5933_INST);
        if (s & DL_I2C_CONTROLLER_STATUS_ERROR) {
            DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
            return 2;
        }
        if (!(s & DL_I2C_CONTROLLER_STATUS_BUSY)) return 0;
        to--;
    }
    DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
    return 1;
}

uint8_t tsp_ad5933_read_reg(uint8_t reg_addr)
{
    uint8_t rx = 0;

    /* Step 1: Write register address */
    {
        DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
        {
            volatile uint32_t w = I2C_TIMEOUT;
            while (w && !(DL_I2C_getControllerStatus(I2C_AD5933_INST) &
                          DL_I2C_CONTROLLER_STATUS_IDLE)) w--;
        }

        DL_I2C_fillControllerTXFIFO(I2C_AD5933_INST, &reg_addr, 1);

        DL_I2C_startControllerTransfer(I2C_AD5933_INST, AD5933_I2C_ADDR,
            DL_I2C_CONTROLLER_DIRECTION_TX, 1);

        ad5933_delay_cycles(500);
        i2c_wait_done();
    }

    /* Step 2: Read data */
    {
        DL_I2C_resetControllerTransfer(I2C_AD5933_INST);
        {
            volatile uint32_t w = I2C_TIMEOUT;
            while (w && !(DL_I2C_getControllerStatus(I2C_AD5933_INST) &
                          DL_I2C_CONTROLLER_STATUS_IDLE)) w--;
        }

        DL_I2C_startControllerTransfer(I2C_AD5933_INST, AD5933_I2C_ADDR,
            DL_I2C_CONTROLLER_DIRECTION_RX, 1);

        {
            volatile uint32_t to = I2C_TIMEOUT;
            while (DL_I2C_isControllerRXFIFOEmpty(I2C_AD5933_INST) && to) to--;
        }
        rx = DL_I2C_receiveControllerData(I2C_AD5933_INST);

        i2c_wait_done();
    }

    return rx;
}

/* --- Read status register --- */
uint8_t tsp_ad5933_read_status(void)
{
    return tsp_ad5933_read_reg(AD5933_REG_STATUS);
}

/* --- Read temperature (returns Celsius) --- */
float tsp_ad5933_read_temperature(void)
{
    uint8_t th, tl;
    int16_t raw;

    /* Issue measure temperature command (must >>8: macro is 16-bit).
     * Keep external clock (AD5933_CLK_EXTERNAL = 0x08) — board uses X2 16MHz. */
    tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
        (uint8_t)(AD5933_CTRL_MEASURE_TEMP >> 8));
    tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);

    /* Wait for valid temperature with ~500ms timeout.
       AD5933 datasheet: typical temp measurement ~30ms; 500ms is generous.
       Each iteration = ~1ms (ad5933_delay_cycles 80000 @ 80MHz). */
    {
        uint16_t timeout = 500;
        while (!(tsp_ad5933_read_status() & AD5933_STATUS_TEMP_VALID)) {
            if (--timeout == 0) return NAN;
            ad5933_delay_cycles(80000);  /* ~1ms */
        }
    }

    /* Read 14-bit two's complement temperature (D15-D14 are don't-care).
     * Mask to 14 bits, then sign-extend from D13. */
    th  = tsp_ad5933_read_reg(AD5933_REG_TEMP_H);
    tl  = tsp_ad5933_read_reg(AD5933_REG_TEMP_L);
    raw = ((int16_t)th << 8) | tl;
    raw &= 0x3FFF;

    /* AD5933 formula: 14-bit two's complement, 0.03125°C/LSB */
    if (raw & 0x2000) {
        return (float)(raw - 0x4000) / 32.0f;
    } else {
        return (float)raw / 32.0f;
    }
}

/* --- Program frequency sweep parameters ---
 * num_increments: number of frequency steps (9-bit, 0–511). Total points = num_increments + 1.
 * settling_cycles: base settling time in output cycles × settle_multiplier.
 * settle_multiplier: use AD5933_SETTLE_X1 / X2 / X4 from tsp_ad5933.h. */
void tsp_ad5933_set_sweep(uint32_t start_hz, uint32_t delta_hz,
                           uint16_t num_increments, uint16_t settling_cycles,
                           uint16_t settle_multiplier)
{
    uint32_t mclk = 16000000UL;   /* X2 = 16 MHz external crystal */

    /* AD5933 datasheet p.14: DDS core clock = MCLK/4.
     * Frequency code = f_OUT × 2^27 / (MCLK/4) = f_OUT × 2^29 / MCLK */
    uint32_t start_code = (uint32_t)(((uint64_t)start_hz * 536870912ULL) / mclk);
    uint32_t delta_code = (uint32_t)(((uint64_t)delta_hz  * 536870912ULL) / mclk);

    /* Write start frequency (24-bit, registers 0x82-0x84) */
    tsp_ad5933_write_reg(AD5933_REG_START_FREQ_H, (uint8_t)((start_code >> 16) & 0xFF));
    tsp_ad5933_write_reg(AD5933_REG_START_FREQ_M, (uint8_t)((start_code >> 8)  & 0xFF));
    tsp_ad5933_write_reg(AD5933_REG_START_FREQ_L, (uint8_t)( start_code        & 0xFF));

    /* Write frequency increment (24-bit, registers 0x85-0x87) */
    tsp_ad5933_write_reg(AD5933_REG_FREQ_INCR_H, (uint8_t)((delta_code >> 16) & 0xFF));
    tsp_ad5933_write_reg(AD5933_REG_FREQ_INCR_M, (uint8_t)((delta_code >> 8)  & 0xFF));
    tsp_ad5933_write_reg(AD5933_REG_FREQ_INCR_L, (uint8_t)( delta_code        & 0xFF));

    /* Write number of increments (9-bit, registers 0x88-0x89).
     * AD5933 datasheet: total measurement points = num_increments + 1. */
    tsp_ad5933_write_reg(AD5933_REG_NUM_INCR_H, (uint8_t)((num_increments >> 8) & 0x01));
    tsp_ad5933_write_reg(AD5933_REG_NUM_INCR_L, (uint8_t)( num_increments       & 0xFF));

    /* Write settling time: 9-bit cycle count (D8:D0, max 511) in 0x8A[D8]+0x8B[D7:0].
     * D10:D9 of 0x8A = multiplier (×1/×2/×4). Validate to avoid corrupting multiplier bits. */
    if (settling_cycles > 511) settling_cycles = 511;
    tsp_ad5933_write_reg(AD5933_REG_SETTLING_H,
        (uint8_t)((settle_multiplier >> 8) | ((settling_cycles >> 8) & 0x01)));
    tsp_ad5933_write_reg(AD5933_REG_SETTLING_L,
        (uint8_t)( settling_cycles       & 0xFF));
}

/* --- Initialize AD5933: reset, set external clock, standby --- */
void tsp_ad5933_init(void)
{
    /* Reset: write to CTRL_L with RESET bit (clears automatically) */
    tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_RESET);

    /* Settle after reset */
    ad5933_delay_cycles(800000);  /* ~10ms */

    /* Set external clock (MCLK = X2 16MHz), standby mode */
    tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
        (uint8_t)((AD5933_CTRL_STANDBY >> 8) | AD5933_VOLT_2000MV | AD5933_PGA_X1));
    tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);
}

/* --- Start frequency sweep --- */
void tsp_ad5933_start_sweep(void)
{
    /* Initialize with start frequency */
    tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
        (uint8_t)((AD5933_CTRL_INIT_FREQ >> 8) | AD5933_VOLT_2000MV | AD5933_PGA_X1));
    tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);

    /* Wait for initialization to complete (status bit?) */
    ad5933_delay_cycles(800000);  /* ~10ms */

    /* Start sweep */
    tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
        (uint8_t)((AD5933_CTRL_START_SWEEP >> 8) | AD5933_VOLT_2000MV | AD5933_PGA_X1));
    tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);
}

/* --- Read 16-bit real value (two's complement) --- */
int16_t tsp_ad5933_read_real(void)
{
    uint8_t rh = tsp_ad5933_read_reg(AD5933_REG_REAL_H);
    uint8_t rl = tsp_ad5933_read_reg(AD5933_REG_REAL_L);
    return ((int16_t)rh << 8) | rl;
}

/* --- Read 16-bit imaginary value (two's complement) --- */
int16_t tsp_ad5933_read_imag(void)
{
    uint8_t ih = tsp_ad5933_read_reg(AD5933_REG_IMAG_H);
    uint8_t il = tsp_ad5933_read_reg(AD5933_REG_IMAG_L);
    return ((int16_t)ih << 8) | il;
}
