#include "tsp_uart6.h"
#include "tsp_isr.h"

#define TSP_UART6            UART6
#define TSP_UART6_INT_IRQN   UART6_INT_IRQn
#define TX6_TIMEOUT_MS       10

/* SysConfig will generate UART_K230_INST_FREQUENCY = 80000000 (BUSCLK).
 * Use it if available, otherwise fallback. */
#ifndef UART_K230_INST_FREQUENCY
#define UART_K230_INST_FREQUENCY  80000000
#endif

/* ─── Ring buffer ─── */
static volatile uint8_t  g_uart6_rx_buf[UART6_RX_BUF_SIZE];
static volatile uint16_t g_uart6_rx_in;
static volatile uint16_t g_uart6_rx_out;

/* ─── TX with timeout ─── */
static void uart6_tx_byte(uint8_t data)
{
    uint32_t start = sys_tick_counter;
    while (DL_UART_isBusy(TSP_UART6)) {
        if ((sys_tick_counter - start) > TX6_TIMEOUT_MS) return;
    }
    DL_UART_transmitData(TSP_UART6, data);
}

/* ─── Init ─── */
void tsp_uart6_init(uint32_t baudrate)
{
    DL_UART_configBaudRate(TSP_UART6, UART_K230_INST_FREQUENCY, baudrate);
    DL_UART_enable(TSP_UART6);
    NVIC_EnableIRQ(TSP_UART6_INT_IRQN);
    g_uart6_rx_in  = 0;
    g_uart6_rx_out = 0;
}

/* ─── RX enable/disable ─── */
void tsp_uart6_rx_enable(void)
{
    g_uart6_rx_in  = 0;
    g_uart6_rx_out = 0;
    DL_UART_enableInterrupt(TSP_UART6, DL_UART_INTERRUPT_RX);
}

void tsp_uart6_rx_disable(void)
{
    DL_UART_disableInterrupt(TSP_UART6, DL_UART_INTERRUPT_RX);
}

/* ─── TX ─── */
void tsp_uart6_send_byte(uint8_t data)
{
    uart6_tx_byte(data);
}

void tsp_uart6_send_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        uart6_tx_byte(data[i]);
    }
}

void tsp_uart6_send_string(const char *str)
{
    while (*str) {
        uart6_tx_byte((uint8_t)*str);
        str++;
    }
}

/* ─── RX ─── */
uint8_t tsp_uart6_read_byte(void)
{
    uint8_t data = 0;
    uint16_t in  = g_uart6_rx_in;
    uint16_t out = g_uart6_rx_out;
    if (in != out) {
        data = g_uart6_rx_buf[out];
        g_uart6_rx_out = (out + 1) % UART6_RX_BUF_SIZE;
    }
    return data;
}

uint16_t tsp_uart6_available(void)
{
    uint16_t in  = g_uart6_rx_in;
    uint16_t out = g_uart6_rx_out;
    if (in >= out) {
        return (uint16_t)(in - out);
    } else {
        return (uint16_t)(UART6_RX_BUF_SIZE - out + in);
    }
}

void tsp_uart6_flush_rx(void)
{
    g_uart6_rx_out = g_uart6_rx_in;
}

/* ─── ISR ─── */
void tsp_uart6_isr(void)
{
    DL_UART_IIDX status = DL_UART_getPendingInterrupt(TSP_UART6);

    if (status == DL_UART_IIDX_RX) {
        uint8_t data = (uint8_t)DL_UART_receiveData(TSP_UART6);
        uint16_t next_in = (g_uart6_rx_in + 1) % UART6_RX_BUF_SIZE;

        if (next_in != g_uart6_rx_out) {
            g_uart6_rx_buf[g_uart6_rx_in] = data;
            g_uart6_rx_in = next_in;
        }

        DL_UART_clearInterruptStatus(TSP_UART6, DL_UART_INTERRUPT_RX);
    }
}
