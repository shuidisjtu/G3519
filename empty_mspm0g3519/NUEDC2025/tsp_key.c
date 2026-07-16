#include "tsp_key.h"
#include "tsp_gpio.h"

/* Debounce: require 2 consecutive identical readings (20ms total at 10ms interval) */
#define KEY_DEBOUNCE_CNT    2

typedef struct {
    uint8_t  debounce_cnt;
    uint8_t  raw_last;       /* last raw reading */
    uint8_t  state;          /* 0=released, 1=pressed (debounced current state) */
    uint8_t  state_prev;     /* previous debounced state (for edge detection) */
} key_ctx_t;

static key_ctx_t g_keys[KEY_NUM];

/*
 * Map key code -> normalized GPIO read (1 = pressed, 0 = released).
 *
 * S0  (PA18): 47kΩ pull-down on main board → active HIGH.
 * S1  (PC0) : 10kΩ pull-up  on expansion board → active LOW.
 * S2  (PA16): 10kΩ pull-up  on expansion board → active LOW.
 * PUSH(PA12): 10kΩ pull-up  on expansion board → active LOW.
 */
static uint8_t key_read(uint8_t key)
{
    switch (key) {
        case KEY_S0:   return (S0() != 0) ? 1 : 0;    /* active HIGH */
        case KEY_S1:   return (S1() == 0) ? 1 : 0;    /* active LOW  */
        case KEY_S2:   return (S2() == 0) ? 1 : 0;    /* active LOW  */
        case KEY_PUSH: return (PUSH() == 0) ? 1 : 0;   /* active LOW  */
        default:       return 0;
    }
}

void tsp_key_init(void)
{
    uint8_t i;
    for (i = 0; i < KEY_NUM; i++) {
        g_keys[i].debounce_cnt = 0;
        g_keys[i].raw_last     = key_read(i);
        g_keys[i].state        = 0;
        g_keys[i].state_prev   = 0;
    }
}

void tsp_key_scan(void)
{
    uint8_t i;
    uint8_t raw;

    for (i = 0; i < KEY_NUM; i++) {
        raw = key_read(i);

        if (raw != g_keys[i].raw_last) {
            /* Reading changed - reset debounce counter */
            g_keys[i].debounce_cnt = 0;
            g_keys[i].raw_last = raw;
        } else {
            /* Reading stable - increment debounce counter */
            if (g_keys[i].debounce_cnt < KEY_DEBOUNCE_CNT) {
                g_keys[i].debounce_cnt++;
            }
            if (g_keys[i].debounce_cnt == KEY_DEBOUNCE_CNT) {
                /* Confirmed stable - update state */
                g_keys[i].state_prev = g_keys[i].state;
                g_keys[i].state = raw;
            }
        }
    }
}

uint8_t tsp_key_pressed(uint8_t key)
{
    if (key >= KEY_NUM) return 0;

    /* Rising edge: state transitioned from 0 to 1 */
    if (g_keys[key].state == 1 && g_keys[key].state_prev == 0) {
        g_keys[key].state_prev = 1;  /* clear edge, avoid repeat trigger */
        return 1;
    }
    return 0;
}

uint8_t tsp_key_state(uint8_t key)
{
    if (key >= KEY_NUM) return 0;
    return g_keys[key].state;
}
