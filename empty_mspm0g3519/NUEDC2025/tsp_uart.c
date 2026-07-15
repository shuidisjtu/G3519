#include "tsp_uart.h"

/* ===== UART0 Pin Configuration =====
 * Default: PA10 = RX, PA11 = TX.
 * These pins are free (not in current SysConfig).
 * Change IOMUX macros below if using different pins.
 */
#define TSP_UART            UART0
#define TSP_UART_INT_IRQN   UART0_INT_IRQn

/* UART0 RX pin: PA10 -> IOMUX_PINCM32, alternate function UART0_RX */
#define UART_RX_IOMUX       IOMUX_PINCM22
#define UART_RX_FUNC        IOMUX_PINCM22_PF_UART0_RX

/* UART0 TX pin: PB2 -> IOMUX_PINCM21, alternate function UART0_TX */
#define UART_TX_IOMUX       IOMUX_PINCM21
#define UART_TX_FUNC        IOMUX_PINCM21_PF_UART0_TX

/* ─── Ring buffer ─── */
static volatile uint8_t  g_uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t g_uart_rx_in;   /* write index (ISR) */
static volatile uint16_t g_uart_rx_out;  /* read index (main) */

/* ─── Init ─── */
void tsp_uart_init(uint32_t baudrate)
{
    /* 1. Configure IOMUX for RX and TX pins */
    DL_GPIO_initPeripheralFunction(UART_RX_IOMUX, UART_RX_FUNC);
    DL_GPIO_initPeripheralFunction(UART_TX_IOMUX, UART_TX_FUNC);

    /* 2. Configure UART peripheral (struct-based init) */
    DL_UART_Config uartCfg;
    uartCfg.mode        = DL_UART_MODE_NORMAL;
    uartCfg.direction   = DL_UART_DIRECTION_TX_RX;
    uartCfg.flowControl = DL_UART_FLOW_CONTROL_NONE;
    uartCfg.parity      = DL_UART_PARITY_NONE;
    uartCfg.wordLength  = DL_UART_WORD_LENGTH_8_BITS;
    uartCfg.stopBits    = DL_UART_STOP_BITS_ONE;
    DL_UART_init(TSP_UART, &uartCfg);

    /* 3. Clock config: BUSCLK (80MHz), no divider */
    DL_UART_ClockConfig clkCfg;
    clkCfg.clockSel    = DL_UART_CLOCK_BUSCLK;
    clkCfg.divideRatio = DL_UART_CLOCK_DIVIDE_RATIO_1;
    DL_UART_setClockConfig(TSP_UART, &clkCfg);

    /* 4. Baud rate: IBRD + FBRD (16x oversampling)
     *    divisor = BUSCLK / (16 * baudRate)
     *    For 80MHz / (16 * 115200) = 43.40 -> IBRD=43, FBRD=26 */
    uint32_t divisor = (80000000u + (8u * baudrate)) / (16u * baudrate);
    uint32_t ibrd    = divisor / 64u;
    uint32_t fbrd    = divisor % 64u;
    DL_UART_setBaudRateDivisor(TSP_UART, ibrd, fbrd);

    /* 5. Enable UART */
    DL_UART_enable(TSP_UART);

    /* 6. Enable RX interrupt */
    DL_UART_enableInterrupt(TSP_UART, DL_UART_INTERRUPT_RX);

    /* 7. Enable UART0 interrupt in NVIC */
    NVIC_EnableIRQ(TSP_UART_INT_IRQN);

    /* 8. Init ring buffer */
    g_uart_rx_in  = 0;
    g_uart_rx_out = 0;
}

/* ─── TX (blocking) ─── */
void tsp_uart_send_byte(uint8_t data)
{
    DL_UART_transmitDataBlocking(TSP_UART, data);
}

void tsp_uart_send_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        DL_UART_transmitDataBlocking(TSP_UART, data[i]);
    }
}

void tsp_uart_send_string(const char *str)
{
    while (*str) {
        DL_UART_transmitDataBlocking(TSP_UART, (uint8_t)*str);
        str++;
    }
}

/* ─── RX (non-blocking) ─── */
uint8_t tsp_uart_read_byte(void)
{
    uint8_t data = 0;
    if (g_uart_rx_in != g_uart_rx_out) {
        data = g_uart_rx_buf[g_uart_rx_out];
        g_uart_rx_out = (g_uart_rx_out + 1) % UART_RX_BUF_SIZE;
    }
    return data;
}

uint16_t tsp_uart_available(void)
{
    if (g_uart_rx_in >= g_uart_rx_out) {
        return (uint16_t)(g_uart_rx_in - g_uart_rx_out);
    } else {
        return (uint16_t)(UART_RX_BUF_SIZE - g_uart_rx_out + g_uart_rx_in);
    }
}

void tsp_uart_flush_rx(void)
{
    DL_UART_disableInterrupt(TSP_UART, DL_UART_INTERRUPT_RX);
    g_uart_rx_out = g_uart_rx_in;
    DL_UART_enableInterrupt(TSP_UART, DL_UART_INTERRUPT_RX);
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
    }

	DL_UART_clearInterruptStatus(TSP_UART, DL_UART_INTERRUPT_RX);
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
            DL_UART_transmitDataBlocking(TSP_UART, buf[n]);
        }
        return bufSize;
    }

    return 0;
}
