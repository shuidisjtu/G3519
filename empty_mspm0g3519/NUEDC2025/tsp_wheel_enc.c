#include "tsp_wheel_enc.h"
#include "tsp_isr.h"

static volatile int32_t g_count[2];
static int16_t          g_speed[2];
static uint16_t         g_last_raw[2];
static uint32_t         g_last_tick;
static uint8_t          g_first_run;

static inline GPTIMER_Regs *enc_inst(uint8_t motor)
{
    return (motor == MOTOR1) ? WHEEL_ENC_R_INST : WHEEL_ENC_L_INST;
}

static uint16_t read_raw(uint8_t motor)
{
    return (uint16_t)DL_TimerG_getTimerCount(enc_inst(motor));
}

void tsp_wheel_enc_init(void)
{
    g_count[0] = 0;  g_count[1] = 0;
    g_speed[0] = 0;  g_speed[1] = 0;
    g_last_tick = sys_tick_counter;
    g_first_run = 1;

    DL_TimerG_setTimerCount(WHEEL_ENC_R_INST, WHEEL_ENC_CENTER);
    DL_TimerG_setTimerCount(WHEEL_ENC_L_INST, WHEEL_ENC_CENTER);
    g_last_raw[0] = WHEEL_ENC_CENTER;
    g_last_raw[1] = WHEEL_ENC_CENTER;
}

void tsp_wheel_enc_start(void)
{
    tsp_wheel_enc_init();
    DL_TimerG_startCounter(WHEEL_ENC_R_INST);
    DL_TimerG_startCounter(WHEEL_ENC_L_INST);
}

void tsp_wheel_enc_stop(void)
{
    DL_TimerG_stopCounter(WHEEL_ENC_R_INST);
    DL_TimerG_stopCounter(WHEEL_ENC_L_INST);
}

void tsp_wheel_enc_update(void)
{
    uint32_t now = sys_tick_counter;
    if (now - g_last_tick < WHEEL_ENC_SPEED_MS) return;

    for (uint8_t i = 0; i < 2; i++) {
        uint16_t raw = read_raw(i);
        int16_t  delta = (int16_t)(raw - g_last_raw[i]);
        g_last_raw[i] = raw;

        if (!g_first_run) {
            g_count[i] += delta;
            g_speed[i]  = delta;
        }
    }
    g_first_run = 0;
    g_last_tick = now;
}

int16_t tsp_wheel_enc_speed(uint8_t motor)
{
    if (motor > 1) return 0;
    return g_speed[motor];
}

int32_t tsp_wheel_enc_count(uint8_t motor)
{
    if (motor > 1) return 0;
    return g_count[motor];
}

void tsp_wheel_enc_reset(void)
{
    g_count[0] = 0;  g_count[1] = 0;
    g_speed[0] = 0;  g_speed[1] = 0;
    DL_TimerG_setTimerCount(WHEEL_ENC_R_INST, WHEEL_ENC_CENTER);
    DL_TimerG_setTimerCount(WHEEL_ENC_L_INST, WHEEL_ENC_CENTER);
    g_last_raw[0] = WHEEL_ENC_CENTER;
    g_last_raw[1] = WHEEL_ENC_CENTER;
    g_first_run = 1;
    g_last_tick = sys_tick_counter;
}
