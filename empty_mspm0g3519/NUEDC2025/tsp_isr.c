#include "tsp_isr.h"
#include "tsp_encoder.h"
#include "tsp_uart.h"

volatile uint32_t sys_tick_counter = 0;
volatile static uint32_t delay;

void delay_1ms(uint32_t count)
{
    delay = count;
    while (0U != delay) {}
}

void SysTick_Handler(void)
{
    sys_tick_counter++;
    if (0U != delay) {
        delay--;
    }

    /* Periodic encoder speed update every 20ms */
    tsp_encoder_update_speed();
}

/* ================================================================
 * GROUP1_IRQHandler — handles GPIOA (encoder PHA0) interrupts.
 * MSPM0: GPIOA is grouped under INT_GROUP1.
 * ================================================================ */
void GROUP1_IRQHandler(void)
{
    uint32_t group_iidx;

    /* Determine which IIDX triggered GROUP1 */
    group_iidx = DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1);

    switch (group_iidx) {
    case DL_INTERRUPT_GROUP1_IIDX_GPIOA: {
        /* Get highest priority pending GPIOA pin interrupt */
        DL_GPIO_IIDX dio_iidx = DL_GPIO_getPendingInterrupt(GPIOA);
        tsp_encoder_isr((uint8_t)dio_iidx);
        DL_GPIO_clearInterruptStatus(GPIOA, dio_iidx);
        break;
    }
    default:
        break;
    }
}

/* ================================================================
 * UART0_IRQHandler — RX interrupt for ring buffer
 * UART0 has a dedicated interrupt vector in the startup table.
 * ================================================================ */
void UART0_IRQHandler(void)
{
    tsp_uart_isr();
}
