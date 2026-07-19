#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_uart.h"
#include "tsp_uart_k230.h"
#include "tsp_k230.h"
#include <stdio.h>

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;

/* ===== Boot Animation ===== */

static void boot_animation(void)
{
	/* Color test sequence (like teacher's tsp_tft18_test_color) */
	tsp_tft18_test_color();

	/* Show boot info */
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"NUEDC-2026 SAIS@SJTU", BLUE, YELLOW);
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"MSPM0G3519  80MHz", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"LCD 160x128  SPI1", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Initializing...", GREEN, BLACK);

	/* LED blink during boot */
	LED_ON();
	delay_1ms(200);
	LED_OFF();
	delay_1ms(200);
	LED_ON();
	delay_1ms(200);
	LED_OFF();

	/* Startup beep */
	BUZZ_ON();
	delay_1ms(50);
	BUZZ_OFF();

	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Ready!          ", GREEN, BLACK);
	delay_1ms(500);

	/* Clear screen before entering menu */
	tsp_tft18_clear(BLACK);
}

/* ===== UART Test ===== */

static void action_uart_test(void)
{
	uint8_t i;
	static uint16_t tx_count = 0;

	for (i = 1; i < 8; i++) {
		tsp_tft18_show_str_color(0, i, (uint8_t *)"                    ", WHITE, BLACK);
	}

	tsp_tft18_show_str_color(0, 1, (uint8_t *)"UART0 Test", YELLOW, BLACK);
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"115200 8N1  COM11", WHITE, BLACK);

	/* TX test */
	tsp_uart_send_string("=== UART0 TX Test ===\r\n");
	tsp_uart_send_string("MSPM0G3519 @ 80MHz\r\n");
	tx_count++;
	printf("printf test: tx_count=%u\r\n", tx_count);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"TX sent. Check SSCOM", GREEN, BLACK);

	/* RX echo mode */
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Send from SSCOM ->", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", CYAN, BLACK);
	tsp_uart_flush_rx();
	tsp_uart_rx_enable();

	{
		uint8_t echo_buf[21];
		uint8_t echo_pos = 0;
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"RX:                 ", WHITE, BLACK);

		while (1) {
			tsp_key_scan();
			if (tsp_key_pressed(KEY_PUSH)) break;

			while (tsp_uart_available()) {
				uint8_t ch = tsp_uart_read_byte();
				tsp_uart_send_byte(ch);  /* echo back */
				if (ch >= 0x20 && ch <= 0x7E) {
					echo_buf[echo_pos] = ch;
					echo_pos++;
					if (echo_pos >= 17) echo_pos = 0;
					echo_buf[echo_pos] = '\0';
					tsp_tft18_show_str_color(24, 6, echo_buf, GREEN, BLACK);
				}
			}
			delay_1ms(5);
		}
	}

	tsp_uart_rx_disable();
	tsp_uart_send_string("\r\n=== UART0 Test End ===\r\n");
	tsp_menu_request_redraw();
}

/* ===== K230 Vision Test ===== */

static void action_k230_test(void)
{
	uint8_t i;
	k230_target_t tgt;

	for (i = 1; i < 8; i++) {
		tsp_tft18_show_str_color(0, i, (uint8_t *)"                    ", WHITE, BLACK);
	}

	tsp_tft18_show_str_color(0, 1, (uint8_t *)"K230 Test (UART6)", YELLOW, BLACK);
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"ID:      Frm:", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"X:       Y:", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"W:       H:", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"MSG:", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Err:", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", CYAN, BLACK);

	/* Reset parser, start receiving from K230 (open a GUI demo on it) */
	tsp_k230_init();
	tsp_uart_k230_flush_rx();
	tsp_uart_k230_rx_enable();

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;

		/* Consume ring buffer, parse YbProtocol frames (main-loop context) */
		tsp_k230_task();

		if (tsp_k230_get_target(&tgt)) {
			char disp[16];
			uint8_t n = 0;

			tsp_tft18_show_uint16(24,  2, tgt.func_id);
			tsp_tft18_show_uint16(104, 2, (uint16_t)tsp_k230_frame_count());
			tsp_tft18_show_int16(24,  3, tgt.x);
			tsp_tft18_show_int16(96,  3, tgt.y);
			tsp_tft18_show_int16(24,  4, tgt.w);
			tsp_tft18_show_int16(96,  4, tgt.h);
			tsp_tft18_show_uint16(32, 6, (uint16_t)tsp_k230_error_count());

			/* MSG field: pad to fixed width to clear stale chars */
			while (n < 15 && tgt.msg[n]) { disp[n] = tgt.msg[n]; n++; }
			while (n < 15) { disp[n++] = ' '; }
			disp[15] = '\0';
			tsp_tft18_show_str_color(40, 5, (uint8_t *)disp, GREEN, BLACK);
		}

		delay_1ms(5);
	}

	tsp_uart_k230_rx_disable();
	tsp_menu_request_redraw();
}

/* ===== Main Menu (2 items) ===== */

static tsp_menu_item_t main_menu[] = {
	{"UART Test",     action_uart_test},
	{"K230 Test",     action_k230_test},
};

#define MAIN_MENU_COUNT  (sizeof(main_menu) / sizeof(main_menu[0]))

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Boot animation: color test + info display (like teacher's example) */
	boot_animation();

	/* Init encoder (uses SysConfig PHA0 interrupt) */
	tsp_encoder_init();

	/* Init UART0: 115200-8N1, BUSCLK/2=40MHz (PD0 max), OVS auto-select */
	tsp_uart_init(115200);
	tsp_uart_send_string("MSPM0G3519 booted\r\n");

	/* Init UART6 for K230 vision module (J11, BUSCLK 80MHz, 115200 preset).
	 * RX interrupt stays off until K230 Test enables it on demand. */
	tsp_uart_k230_init();
	tsp_k230_init();

	tsp_key_init();
	tsp_menu_init("=== NUEDC 2026 ===", main_menu, MAIN_MENU_COUNT);

	while (1) {
		tsp_key_scan();
		tsp_menu_run();
		delay_1ms(10);
	}
}
