#ifndef _TSP_UART6_H
#define _TSP_UART6_H

#include "tsp_common_headfile.h"

/* UART6 on PC11(TX) / PC10(RX), connector J11, for K230 vision module.
 * BUSCLK 80MHz, 115200-8N1 default. */

#define UART6_RX_BUF_SIZE    256

void tsp_uart6_init(uint32_t baudrate);

/* TX — with timeout (safe for offline use) */
void tsp_uart6_send_byte(uint8_t data);
void tsp_uart6_send_bytes(const uint8_t *data, uint32_t len);
void tsp_uart6_send_string(const char *str);

/* RX — non-blocking */
uint8_t  tsp_uart6_read_byte(void);
uint16_t tsp_uart6_available(void);
void     tsp_uart6_flush_rx(void);
void     tsp_uart6_rx_enable(void);
void     tsp_uart6_rx_disable(void);

/* Called from UART6_IRQHandler in tsp_isr.c */
void tsp_uart6_isr(void);

#endif
