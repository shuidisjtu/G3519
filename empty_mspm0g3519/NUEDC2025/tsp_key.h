#ifndef _TSP_KEY_H
#define _TSP_KEY_H

#include "tsp_common_headfile.h"

/* Key code definitions */
#define KEY_S0    0
#define KEY_S1    1
#define KEY_S2    2
#define KEY_PUSH  3
#define KEY_NUM   4

/* GPIO macros for key reading */

void tsp_key_init(void);
void tsp_key_scan(void);                     /* call every ~10ms in main loop */
uint8_t tsp_key_pressed(uint8_t key);        /* rising edge, auto-clear after read */
uint8_t tsp_key_state(uint8_t key);          /* currently held down */

#endif
