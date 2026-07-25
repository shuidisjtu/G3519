#ifndef TSP_MPU6050_H_
#define TSP_MPU6050_H_

#include "tsp_common_headfile.h"

/* ===== MPU6050 6-Axis IMU Driver (I2C0, Polling) =====
 * Hardware: U3 MPU6050, I2C0 (PB21-SCL, PB22-SDA), 2.2k pull-up, AD0=GND.
 * INT pin PC8 not used in this version (polling mode).
 *
 * Usage:
 *   tsp_mpu6050_init()          -- configure MPU6050 registers
 *   tsp_mpu6050_read_raw(&d)    -- burst read 14 bytes (accel+temp+gyro)
 *   tsp_mpu6050_update_yaw()    -- call every ~10ms for Z-axis integration
 *   tsp_mpu6050_get_yaw()       -- read accumulated yaw (degrees)
 */

/* I2C address (7-bit): AD0 pulled LOW via 10K -> 0x68 */
#define MPU6050_ADDR            0x68

/* Key registers */
#define MPU6050_REG_SMPLRT_DIV  0x19
#define MPU6050_REG_CONFIG      0x1A
#define MPU6050_REG_GYRO_CONFIG 0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_INT_PIN_CFG 0x37
#define MPU6050_REG_INT_ENABLE  0x38
#define MPU6050_REG_INT_STATUS  0x3A
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H  0x41
#define MPU6050_REG_GYRO_XOUT_H 0x43
#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_PWR_MGMT_2  0x6C
#define MPU6050_REG_WHO_AM_I    0x75

/* Gyro sensitivity for +/-500 deg/s range */
#define MPU6050_GYRO_SENS_500   65.5f

typedef struct {
    int16_t ax, ay, az;
    int16_t temp;
    int16_t gx, gy, gz;
} mpu6050_raw_t;

/* Init: configure I2C + MPU6050 registers. Returns 0 on success, -1 on failure */
int8_t tsp_mpu6050_init(void);

/* Read WHO_AM_I register (should return 0x68) */
uint8_t tsp_mpu6050_who_am_i(void);

/* Burst read all 7 values (accel xyz + temp + gyro xyz) */
int8_t tsp_mpu6050_read_raw(mpu6050_raw_t *data);

/* Yaw integration (call frequently, self-paces at 10ms intervals) */
void tsp_mpu6050_update_yaw(void);
float tsp_mpu6050_get_yaw(void);
void tsp_mpu6050_reset_yaw(void);
void tsp_mpu6050_yaw_enable(void);
void tsp_mpu6050_yaw_disable(void);
int16_t tsp_mpu6050_get_gz_offset(void);

#endif
