#include "tsp_uart.h"
#include "tsp_isr.h"

#define TSP_UART            UART0
#define TSP_UART_INT_IRQN   UART0_INT_IRQn
#define TX_TIMEOUT_MS       10

/* ─── Ring buffer ─── */
static volatile uint8_t  g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t g_uart_rx_in;   /* write index (ISR) */
static volatile uint16_t g_uart_rx_out;  /* read index (main) */

/* ─── TX with timeout (prevents hang when DAPLink disconnected) ─── */
static void uart_tx_byte(uint8_t data)
{
    uint32_t start = sys_tick_counter;
    while (DL_UART_isBusy(TSP_UART)) {
        if ((sys_tick_counter - start) > TX_TIMEOUT_MS) return;
    }
    DL_UART_transmitData(TSP_UART, data);
}

/* ─── Init ───
 * SysConfig (SYSCFG_DL_UART_0_init via SYSCFG_DL_init) handles:
 *   - Power, IOMUX, clock source (MFCLK=4MHz), basic config (8N1),
 *     OVS=16X, baud rate preset to 9600, UART enable.
 * This function only adjusts baud rate and sets up the ring buffer.
 * Call AFTER SYSCFG_DL_init() in main(). */
void tsp_uart_init(uint32_t baudrate)
{
    /* 1. Update baud rate: SysConfig preset is 9600, override with requested rate.
     *    Clock = MFCLK = 4 MHz (set by SysConfig, confirmed PD0-safe).
     *    Using DL_UART_configBaudRate for auto OVS selection. */
    DL_UART_configBaudRate(TSP_UART, UART_0_INST_FREQUENCY, baudrate);

    /* 2. Ensure UART is enabled (SysConfig does this, but be defensive) */
    DL_UART_enable(TSP_UART);

    /* 3. Enable UART0 interrupt in NVIC (handler in tsp_isr.c → tsp_uart_isr).
     *    RX interrupt at UART level is NOT enabled here — use tsp_uart_rx_enable()
     *    on demand to prevent floating-pin interrupt storms. */
    NVIC_EnableIRQ(TSP_UART_INT_IRQN);

    /* 4. Init ring buffer */
    g_uart_rx_in  = 0;
    g_uart_rx_out = 0;
}

/* ─── RX enable/disable (call only when ready to receive) ─── */
void tsp_uart_rx_enable(void)
{
    g_uart_rx_in  = 0;
    g_uart_rx_out = 0;
    DL_UART_enableInterrupt(TSP_UART, DL_UART_INTERRUPT_RX);
}

void tsp_uart_rx_disable(void)
{
    DL_UART_disableInterrupt(TSP_UART, DL_UART_INTERRUPT_RX);
}

/* ─── TX (blocking) ─── */
void tsp_uart_send_byte(uint8_t data)
{
    uart_tx_byte(data);
}

void tsp_uart_send_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        uart_tx_byte(data[i]);
    }
}

void tsp_uart_send_string(const char *str)
{
    while (*str) {
        uart_tx_byte((uint8_t)*str);
        str++;
    }
}

/* ─── RX (non-blocking) ─── */
uint8_t tsp_uart_read_byte(void)
{
    uint8_t data = 0;
    uint16_t in  = g_uart_rx_in;
    uint16_t out = g_uart_rx_out;
    if (in != out) {
        data = g_uart_rx_buf[out];
        g_uart_rx_out = (out + 1) % UART_RX_BUF_SIZE;
    }
    return data;
}

uint16_t tsp_uart_available(void)
{
    uint16_t in  = g_uart_rx_in;
    uint16_t out = g_uart_rx_out;
    if (in >= out) {
        return (uint16_t)(in - out);
    } else {
        return (uint16_t)(UART_RX_BUF_SIZE - out + in);
    }
}

void tsp_uart_flush_rx(void)
{
    g_uart_rx_out = g_uart_rx_in;
}

/* ─── ISR ─── */
void tsp_uart_isr(void)
{
    DL_UART_IIDX status = DL_UART_getPendingInterrupt(TSP_UART);

    /* RX data available */
    if (status == DL_UART_IIDX_RX) {
        uint8_t data = (uint8_t)DL_UART_receiveData(TSP_UART);
        uint16_t next_in = (g_uart_rx_in + 1) % UART_RX_BUF_SIZE;

        /* Ring buffer: drop byte if full (overflow protection) */
        if (next_in != g_uart_rx_out) {
            g_uart_rx_buf[g_uart_rx_in] = data;
            g_uart_rx_in = next_in;
        }

        DL_UART_clearInterruptStatus(TSP_UART, DL_UART_INTERRUPT_RX);
    }
}

/* ================================================================
 * printf() redirect via IAR DLIB Low-Level I/O
 * Implement __write so printf outputs to UART0.
 * ================================================================ */

#include <yfuns.h>   /* IAR DLIB: _LLIO_STDIN, _LLIO_STDOUT */

size_t __write(int handle, const unsigned char *buf, size_t bufSize)
{
    size_t n;

    /* Only handle stdout and stderr */
    if (handle == _LLIO_STDOUT || handle == _LLIO_STDERR) {
        for (n = 0; n < bufSize; n++) {
            uart_tx_byte(buf[n]);
        }
        return bufSize;
    }

    return 0;
}
