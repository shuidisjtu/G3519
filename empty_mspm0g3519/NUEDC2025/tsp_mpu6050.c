#include "tsp_mpu6050.h"
#include "tsp_isr.h"

/* I2C timeout (loop iterations) */
#define I2C_TIMEOUT     50000U

/* Yaw update interval */
#define YAW_INTERVAL_MS 10

/* Calibration samples */
#define CALIB_SAMPLES   200

/* Dead zone: ignore gyro readings smaller than this (LSB) after offset removal */
#define GYRO_DEADZONE   5

/* ===== Static state ===== */
static volatile float   g_yaw         = 0.0f;
static volatile uint8_t g_yaw_enabled = 0;
static          uint32_t g_yaw_last_tick = 0;
static          int16_t g_gz_offset   = 0;   /* gyro Z zero-bias from calibration */

/* ===== I2C low-level (polling) ===== */

static int8_t i2c_wait_done(void)
{
    volatile uint32_t timeout;

    /* Phase 1: wait for bus to become busy (transfer started) */
    timeout = I2C_TIMEOUT;
    while (!(DL_I2C_getControllerStatus(IMU_I2C_INST) &
             DL_I2C_CONTROLLER_STATUS_BUSY_BUS)) {
        if (--timeout == 0) return -1;
    }

    /* Phase 2: wait for bus to become idle (transfer complete) */
    timeout = I2C_TIMEOUT;
    while (DL_I2C_getControllerStatus(IMU_I2C_INST) &
           DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return -1;
    }

    return 0;
}

static int8_t i2c_write(uint8_t *data, uint16_t len)
{
    volatile uint32_t timeout;

    /* Ensure bus is idle before starting (previous STOP may still be in progress) */
    timeout = I2C_TIMEOUT;
    while (DL_I2C_getControllerStatus(IMU_I2C_INST) &
           DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {
        if (--timeout == 0) return -1;
    }

    DL_I2C_flushControllerTXFIFO(IMU_I2C_INST);
    DL_I2C_fillControllerTXFIFO(IMU_I2C_INST, data, len);
    DL_I2C_startControllerTransfer(IMU_I2C_INST, MPU6050_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, len);

    return i2c_wait_done();
}

static int8_t i2c_read(uint8_t *buf, uint16_t len)
{
    volatile uint32_t timeout;
    uint16_t i = 0;

    DL_I2C_flushControllerRXFIFO(IMU_I2C_INST);
    DL_I2C_startControllerTransfer(IMU_I2C_INST, MPU6050_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    timeout = I2C_TIMEOUT;
    while (i < len) {
        if (!DL_I2C_isControllerRXFIFOEmpty(IMU_I2C_INST)) {
            buf[i++] = DL_I2C_receiveControllerData(IMU_I2C_INST);
            timeout = I2C_TIMEOUT;
        } else {
            if (--timeout == 0) return -1;
        }
    }

    return 0;
}

/* Write one register */
static int8_t mpu6050_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_write(buf, 2);
}

/* Read one register */
static int8_t mpu6050_read_reg(uint8_t reg, uint8_t *val)
{
    if (i2c_write(&reg, 1) != 0) return -1;
    return i2c_read(val, 1);
}

/* Burst read: write reg address, then read len bytes */
static int8_t mpu6050_read_burst(uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (i2c_write(&reg, 1) != 0) return -1;
    return i2c_read(buf, len);
}

/* ===== MPU6050 API ===== */

uint8_t tsp_mpu6050_who_am_i(void)
{
    uint8_t val = 0;
    mpu6050_read_reg(MPU6050_REG_WHO_AM_I, &val);
    return val;
}

int8_t tsp_mpu6050_init(void)
{
    uint8_t id;

    /* Reset MPU6050 */
    mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80);
    delay_1ms(100);

    /* Wake up, PLL with X-axis gyro reference */
    mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01);
    delay_1ms(10);

    /* Verify WHO_AM_I */
    id = tsp_mpu6050_who_am_i();
    if (id != 0x68) return -1;

    /* Sample rate: 1kHz / (1+7) = 125 Hz */
    mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, 0x07);

    /* DLPF bandwidth 44 Hz */
    mpu6050_write_reg(MPU6050_REG_CONFIG, 0x03);

    /* Gyro: +/-500 deg/s */
    mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, 0x08);

    /* Accel: +/-4g */
    mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, 0x08);

    /* Disable interrupts from MPU6050 side */
    mpu6050_write_reg(MPU6050_REG_INT_ENABLE, 0x00);

    g_yaw = 0.0f;
    g_yaw_enabled = 0;
    g_yaw_last_tick = sys_tick_counter;

    /* Calibrate gyro Z offset: average CALIB_SAMPLES readings while stationary */
    {
        int32_t sum = 0;
        uint16_t n;
        uint8_t cbuf[2];

        for (n = 0; n < CALIB_SAMPLES; n++) {
            if (mpu6050_read_burst(MPU6050_REG_GYRO_XOUT_H + 4, cbuf, 2) == 0) {
                sum += (int16_t)((cbuf[0] << 8) | cbuf[1]);
            }
            delay_1ms(5);
        }
        g_gz_offset = (int16_t)(sum / CALIB_SAMPLES);
    }

    return 0;
}

int8_t tsp_mpu6050_read_raw(mpu6050_raw_t *data)
{
    uint8_t buf[14];

    if (mpu6050_read_burst(MPU6050_REG_ACCEL_XOUT_H, buf, 14) != 0)
        return -1;

    data->ax   = (int16_t)((buf[0]  << 8) | buf[1]);
    data->ay   = (int16_t)((buf[2]  << 8) | buf[3]);
    data->az   = (int16_t)((buf[4]  << 8) | buf[5]);
    data->temp = (int16_t)((buf[6]  << 8) | buf[7]);
    data->gx   = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gy   = (int16_t)((buf[10] << 8) | buf[11]);
    data->gz   = (int16_t)((buf[12] << 8) | buf[13]);

    return 0;
}

/* ===== Yaw integration ===== */

void tsp_mpu6050_update_yaw(void)
{
    uint32_t now, elapsed;

    if (!g_yaw_enabled) return;

    now = sys_tick_counter;
    elapsed = now - g_yaw_last_tick;
    if (elapsed < YAW_INTERVAL_MS) return;

    g_yaw_last_tick = now;

    {
        uint8_t buf[2];
        int16_t gz;

        /* Read only gyro Z (2 bytes at 0x47-0x48) */
        if (mpu6050_read_burst(MPU6050_REG_GYRO_XOUT_H + 4, buf, 2) != 0)
            return;

        gz = (int16_t)((buf[0] << 8) | buf[1]) - g_gz_offset;

        /* Dead zone: ignore noise when stationary */
        if (gz > -GYRO_DEADZONE && gz < GYRO_DEADZONE)
            return;

        g_yaw += (float)gz / MPU6050_GYRO_SENS_500
                 * ((float)elapsed / 1000.0f);
    }
}

float tsp_mpu6050_get_yaw(void)
{
    return g_yaw;
}

void tsp_mpu6050_reset_yaw(void)
{
    g_yaw = 0.0f;
    g_yaw_last_tick = sys_tick_counter;
}

void tsp_mpu6050_yaw_enable(void)
{
    g_yaw_enabled = 1;
    g_yaw_last_tick = sys_tick_counter;
}

void tsp_mpu6050_yaw_disable(void)
{
    g_yaw_enabled = 0;
}

int16_t tsp_mpu6050_get_gz_offset(void)
{
    return g_gz_offset;
}
