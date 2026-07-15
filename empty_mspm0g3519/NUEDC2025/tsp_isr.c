#include "tsp_isr.h"

volatile uint32_t sys_tick_counter=0;
volatile static uint32_t delay;

void delay_1ms(uint32_t count)
{
	delay = count;
	while(0U != delay) {}
}

void SysTick_Handler()
{
	sys_tick_counter++;
	if(0U != delay) {
		delay--;
	}
}

/*
void UART0_IRQHandler (void)
{
	switch(DL_UART_getPendingInterrupt(UART0))
	{
		case DL_UART_IIDX_TX:
		case DL_UART_IIDX_RX:
		default:
			break;
	}
	DL_UART_clearInterruptStatus(UART0, UART0->CPU_INT.RIS);
}

void UART1_IRQHandler (void)
{
	switch(DL_UART_getPendingInterrupt(UART1))
	{
		case DL_UART_IIDX_TX:
		case DL_UART_IIDX_RX:
		default:
			break;
	}
	DL_UART_clearInterruptStatus(UART1, UART1->CPU_INT.RIS);
}

void UART2_IRQHandler (void)
{
	switch(DL_UART_getPendingInterrupt(UART2))
	{
		case DL_UART_IIDX_TX:
		case DL_UART_IIDX_RX:
		default:
			break;
	}
	DL_UART_clearInterruptStatus(UART2, UART2->CPU_INT.RIS);
}

void UART3_IRQHandler (void)
{
	switch(DL_UART_getPendingInterrupt(UART3))
	{
		case DL_UART_IIDX_TX:
		case DL_UART_IIDX_RX:
		default:
			break;
	}
	DL_UART_clearInterruptStatus(UART3, UART3->CPU_INT.RIS);
}
*/