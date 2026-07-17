#ifndef _TSP_UART_K230_H
#define _TSP_UART_K230_H

#include "tsp_common_headfile.h"

/* ===== K230 Vision Module UART Driver (UART6 @ J11) =====
 * Mirrors the tsp_uart.c ring-buffer pattern for a second UART instance.
 *
 * Hardware: UART6 on PC11(TX) / PC10(RX), J11 header, 115200-8N1.
 * Clock: BUSCLK 80MHz (PD1 domain), baud rate fixed by SysConfig
 *        (UART_K230 instance, IBRD/FBRD preset — no runtime config needed).
 *
 * Features:
 *   - 256-byte ring buffer RX (interrupt-driven, non-blocking)
 *   - Blocking TX (rarely used: stock K230 firmware is report-only)
 *   - RX interrupt on-demand via rx_enable/disable (same policy as UART0)
 */

#define K230_UART_RX_BUF_SIZE    256

/* Initialize UART6 (call AFTER SYSCFG_DL_init) */
void tsp_uart_k230_init(void);

/* TX — blocking */
void tsp_uart_k230_send_byte(uint8_t data);
void tsp_uart_k230_send_bytes(const uint8_t *data, uint32_t len);
void tsp_uart_k230_send_string(const char *str);

/* RX — non-blocking */
uint8_t  tsp_uart_k230_read_byte(void);   /* returns 0 if buffer empty */
uint16_t tsp_uart_k230_available(void);   /* bytes available in ring buffer */
void     tsp_uart_k230_flush_rx(void);    /* clear ring buffer */
void     tsp_uart_k230_rx_enable(void);   /* enable RX interrupt (on-demand) */
void     tsp_uart_k230_rx_disable(void);  /* disable RX interrupt */

/* Called from UART ISR (UART6_IRQHandler in tsp_isr.c) */
void tsp_uart_k230_isr(void);

#endif
