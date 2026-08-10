#ifndef _TSP_UART3_H
#define _TSP_UART3_H

#include "tsp_common_headfile.h"

/* UART3 on PC6(TX) / PC7(RX), connector J4, for TJC serial screen.
 * BUSCLK 80MHz (PD1 domain), 115200-8N1. */

#define UART3_RX_BUF_SIZE    256

void tsp_uart3_init(uint32_t baudrate);

/* TX — with timeout (safe for offline use) */
void tsp_uart3_send_byte(uint8_t data);
void tsp_uart3_send_bytes(const uint8_t *data, uint32_t len);
void tsp_uart3_send_string(const char *str);

/* RX — non-blocking */
uint8_t  tsp_uart3_read_byte(void);
uint16_t tsp_uart3_available(void);
void     tsp_uart3_flush_rx(void);
void     tsp_uart3_rx_enable(void);
void     tsp_uart3_rx_disable(void);

/* Called from UART3_IRQHandler in tsp_isr.c */
void tsp_uart3_isr(void);

#endif
