#ifndef _TSP_ENCODER_H
#define _TSP_ENCODER_H

#include "tsp_common_headfile.h"

/* ===== Encoder Driver for MSPM0G3519 =====
 * Software quadrature decode using PHA0 (PA14) interrupt + PHB0 (PA15) level.
 * PHA0 must have RISE_FALL interrupt configured in SysConfig.
 *
 * Speed is computed as pulse delta per SPEED_INTERVAL_MS (default 20ms).
 * Positive = forward (PHA0 leads PHB0), Negative = reverse.
 */

void tsp_encoder_init(void);
void tsp_encoder_enable(void);         /* enable PHA0 interrupt + reset count */
void tsp_encoder_disable(void);        /* disable PHA0 interrupt + reset count */
int32_t tsp_encoder_get_count(void);   /* accumulated pulse count */
int16_t tsp_encoder_get_speed(void);   /* speed: pulses per speed interval */
void    tsp_encoder_reset(void);       /* zero count and speed */

/* Called from GROUP1_IRQHandler (GPIOA interrupt) in tsp_isr.c */
void tsp_encoder_isr(uint8_t dio_index);

/* Called periodically (e.g. in main loop or SysTick) to update speed */
void tsp_encoder_update_speed(void);

#endif
