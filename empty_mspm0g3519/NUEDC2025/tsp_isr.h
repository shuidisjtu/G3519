#ifndef _tsp_isr_h_
#define _tsp_isr_h_

#include "tsp_common_headfile.h"

/* SysTick variables */
extern volatile uint32_t sys_tick_counter;

void delay_1ms(uint32_t count);

/* ─── Interrupt handlers (defined in tsp_isr.c) ─── */
/* SysTick — 1ms tick + encoder speed update */
/* GROUP1 — GPIOA interrupt dispatch (encoder PHA0) */
/* UART0 — UART RX ring buffer */

#endif
