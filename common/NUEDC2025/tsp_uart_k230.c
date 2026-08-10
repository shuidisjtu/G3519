#include "tsp_uart_k230.h"
#include "tsp_isr.h"

/* ===== UART6 (K230) Configuration =====
 * All hardware setup (power, IOMUX PC10/PC11, BUSCLK, 8N1, 115200,
 * OVS=16X, IBRD=43/FBRD=26) is done by SYSCFG_DL_UART_K230_init()
 * inside SYSCFG_DL_init(). Instance macros come from ti_msp_dl_config.h.
 */
#define K230_UART            UART_K230_INST
#define K230_UART_INT_IRQN   UART_K230_INST_INT_IRQN
#define K230_TX_TIMEOUT_MS  10

/* ─── Ring buffer ─── */
static volatile uint8_t  g_k230_rx_buf[K230_UART_RX_BUF_SIZE];
static volatile uint16_t g_k230_rx_in;   /* write index (ISR) */
static volatile uint16_t g_k230_rx_out;  /* read index (main) */

/* TX with timeout (prevents hang when DAPLink disconnected) */
static void k230_tx_byte(uint8_t data)
{
    uint32_t start = sys_tick_counter;
    while (DL_UART_isBusy(K230_UART)) {
        if ((sys_tick_counter - start) > K230_TX_TIMEOUT_MS) return;
    }
    DL_UART_transmitData(K230_UART, data);
}

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
    k230_tx_byte(data);
}

void tsp_uart_k230_send_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        k230_tx_byte(data[i]);
    }
}

void tsp_uart_k230_send_string(const char *str)
{
    while (*str) {
        k230_tx_byte((uint8_t)*str);
        str++;
    }
}

/* ─── RX (non-blocking) ─── */
uint8_t tsp_uart_k230_read_byte(void)
{
    uint8_t data = 0;
    uint16_t in  = g_k230_rx_in;
    uint16_t out = g_k230_rx_out;
    if (in != out) {
        data = g_k230_rx_buf[out];
        g_k230_rx_out = (out + 1) % K230_UART_RX_BUF_SIZE;
    }
    return data;
}

uint16_t tsp_uart_k230_available(void)
{
    uint16_t in  = g_k230_rx_in;
    uint16_t out = g_k230_rx_out;
    if (in >= out) {
        return (uint16_t)(in - out);
    } else {
        return (uint16_t)(K230_UART_RX_BUF_SIZE - out + in);
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
