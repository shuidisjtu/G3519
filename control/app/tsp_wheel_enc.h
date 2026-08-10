#ifndef TSP_WHEEL_ENC_H_
#define TSP_WHEEL_ENC_H_

#include "tsp_common_headfile.h"
#include "tsp_motor.h"

#define WHEEL_ENC_CENTER    0x8000U
#define WHEEL_ENC_SPEED_MS  20U

void    tsp_wheel_enc_init(void);
void    tsp_wheel_enc_start(void);
void    tsp_wheel_enc_stop(void);
void    tsp_wheel_enc_update(void);
int16_t tsp_wheel_enc_speed(uint8_t motor);
int32_t tsp_wheel_enc_count(uint8_t motor);
void    tsp_wheel_enc_reset(void);

#endif
