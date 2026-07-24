#include "tsp_uart_k230.h"

/* ===== UART6 (K230) Configuration =====
 * All hardware setup (power, IOMUX PC10/PC11, BUSCLK, 8N1, 115200,
 * OVS=16X, IBRD=43/FBRD=26) is done by SYSCFG_DL_UART_K230_init()
 * inside SYSCFG_DL_init(). Instance macros come from ti_msp_dl_config.h.
 */
#define K230_UART            UART_K230_INST
#define K230_UART_INT_IRQN   UART_K230_INST_INT_IRQN

/* ─── Ring buffer ─── */
static volatile uint8_t  g_k230_rx_buf[K230_UART_RX_BUF_SIZE];
static volatile uint16_t g_k230_rx_in;   /* write index (ISR) */
static volatile uint16_t g_k230_rx_out;  /* read index (main) */

/* ─── Init ───
 * SysConfig enables the UART-level RX interrupt source by default
 * (enabledInterrupts=["RX"]). Follow the UART0 on-demand policy:
 * disable it here, re-enable via tsp_uart_k230_rx_enable() when the
 * application is ready to consume data. */
void tsp_uart_k230_init(void)
{
    /* 1. RX interrupt source off until explicitly enabled */
    DL_UART_disableInterrupt(K230_UART, DL_UART_INTERRUPT_RX);

    /* 2. Enable UART6 interrupt in NVIC (handler in tsp_isr.c → tsp_uart_k230_isr) */
    NVIC_ClearPendingIRQ(K230_UART_INT_IRQN);
    NVIC_EnableIRQ(K230_UART_INT_IRQN);

    /* 3. Init ring buffer */
    g_k230_rx_in  = 0;
    g_k230_rx_out = 0;
}

/* ─── RX enable/disable (call only when ready to receive) ─── */
void tsp_uart_k230_rx_enable(void)
{
    g_k230_rx_in  = 0;
    g_k230_rx_out = 0;
    DL_UART_enableInterrupt(K230_UART, DL_UART_INTERRUPT_RX);
}

void tsp_uart_k230_rx_disable(void)
{
    DL_UART_disableInterrupt(K230_UART, DL_UART_INTERRUPT_RX);
}

/* ─── TX (blocking) ─── */
void tsp_uart_k230_send_byte(uint8_t data)
{
    DL_UART_transmitDataBlocking(K230_UART, data);
}

void tsp_uart_k230_send_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_UART_transmitDataBlocking(K230_UART, data[i]);
    }
}

void tsp_uart_k230_send_string(const char *str)
{
    while (*str) {
        DL_UART_transmitDataBlocking(K230_UART, (uint8_t)*str);
        str++;
    }
}

/* ─── RX (non-blocking) ─── */
uint8_t tsp_uart_k230_read_byte(void)
{
    uint8_t data = 0;
    if (g_k230_rx_in != g_k230_rx_out) {
        data = g_k230_rx_buf[g_k230_rx_out];
        g_k230_rx_out = (g_k230_rx_out + 1) % K230_UART_RX_BUF_SIZE;
    }
    return data;
}

uint16_t tsp_uart_k230_available(void)
{
    if (g_k230_rx_in >= g_k230_rx_out) {
        return (uint16_t)(g_k230_rx_in - g_k230_rx_out);
    } else {
        return (uint16_t)(K230_UART_RX_BUF_SIZE - g_k230_rx_out + g_k230_rx_in);
    }
}

void tsp_uart_k230_flush_rx(void)
{
    g_k230_rx_out = g_k230_rx_in;
}

/* ─── ISR ───
 * ISR only enqueues bytes into the ring buffer. Frame parsing happens
 * in the main loop (tsp_k230.c) — never parse inside the ISR. */
void tsp_uart_k230_isr(void)
{
    DL_UART_IIDX status = DL_UART_getPendingInterrupt(K230_UART);

    /* RX data available */
    if (status == DL_UART_IIDX_RX) {
        uint8_t data = (uint8_t)DL_UART_receiveData(K230_UART);
        uint16_t next_in = (g_k230_rx_in + 1) % K230_UART_RX_BUF_SIZE;

        /* Ring buffer: drop byte if full (overflow protection) */
        if (next_in != g_k230_rx_out) {
            g_k230_rx_buf[g_k230_rx_in] = data;
            g_k230_rx_in = next_in;
        }

        DL_UART_clearInterruptStatus(K230_UART, DL_UART_INTERRUPT_RX);
    }
}
