#ifndef _TSP_UART_H
#define _TSP_UART_H

#include "tsp_common_headfile.h"

/* ===== UART Communication Module for MSPM0G3519 =====
 * Adapted from HSP hsp_uart.c ring-buffer + printf redirect pattern.
 *
 * Hardware: UART0 on PA10(TX) / PA11(RX), 115200-8N1 default.
 * If different pins are needed, modify tsp_uart_init().
 *
 * Features:
 *   - 256-byte ring buffer RX (interrupt-driven, non-blocking)
 *   - Blocking TX for strings (uses DL_UART_transmitDataBlocking)
 *   - printf() redirect to UART0 (via __write hook for IAR DLIB)
 *   - Ring buffer overflow protection
 */

#define UART_RX_BUF_SIZE    256

/* Initialize UART0 with given baud rate */
void tsp_uart_init(uint32_t baudrate);

/* TX — blocking */
void tsp_uart_send_byte(uint8_t data);
void tsp_uart_send_bytes(const uint8_t *data, uint32_t len);
void tsp_uart_send_string(const char *str);

/* RX — non-blocking */
uint8_t  tsp_uart_read_byte(void);       /* returns 0 if buffer empty */
uint16_t tsp_uart_available(void);       /* bytes available in ring buffer */
void     tsp_uart_flush_rx(void);        /* clear ring buffer */

/* Called from UART ISR (GROUP1_IRQHandler or UART0_IRQHandler) */
void tsp_uart_isr(void);

#endif
