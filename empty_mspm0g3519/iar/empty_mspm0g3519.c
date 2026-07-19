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

/* ===== K230 Vision Test (color tracking with LCD overlay) ===== */

static void action_k230_test(void)
{
	k230_target_t tgt;
	int16_t lcd_x, lcd_y, lcd_w, lcd_h;
	int16_t old_x = 0, old_y = 0, old_w = 0, old_h = 0;
	int16_t last_disp_x = -1, last_disp_y = -1;
	uint16_t last_fc = 0xFFFF, last_ec = 0xFFFF;
	uint8_t has_old = 0, showed_x = 0;

	/* Full-screen tracking view */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"K230 Track", YELLOW, BLACK);
	tsp_tft18_draw_line_h(0, 104, 160, BLUE);   /* divider y=104 */

	/* Initial diagnostic: show we're waiting */
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Waiting for K230...", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"F:   0 E:   0", WHITE, BLACK);

	tsp_k230_init();
	tsp_uart_k230_flush_rx();
	tsp_uart_k230_rx_enable();

	while (1) {
		/* Scan keys at 1ms intervals for responsive PUSH detection */
		{
			uint8_t i;
			for (i = 0; i < 5; i++) {
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) goto exit_k230;
				delay_1ms(1);
			}
		}

		tsp_k230_task();

		if (tsp_k230_get_target(&tgt)) {
			/* Map K230 320x240 -> LCD 160x128: divide by 2 */
			lcd_x = tgt.x / 2;
			lcd_y = tgt.y / 2;
			lcd_w = tgt.w / 2;
			lcd_h = tgt.h / 2;

			/* Clamp to canvas (pixel y <= 103, x <= 159) */
			if (lcd_x < 0) lcd_x = 0;
			if (lcd_y < 0) lcd_y = 0;
			if (lcd_x + lcd_w > 159) lcd_w = 159 - lcd_x;
			if (lcd_y + lcd_h > 103) lcd_h = 103 - lcd_y;
			if (lcd_w < 2) lcd_w = 2;
			if (lcd_h < 2) lcd_h = 2;

			/* Erase previous rect with BLACK */
			if (has_old) {
				tsp_tft18_draw_frame((uint8_t)old_x, (uint8_t)old_y,
				                     (uint8_t)old_w, (uint8_t)old_h, BLACK);
			}

			/* Draw current rect in GREEN */
			tsp_tft18_draw_frame((uint8_t)lcd_x, (uint8_t)lcd_y,
			                     (uint8_t)lcd_w, (uint8_t)lcd_h, GREEN);

			old_x = lcd_x; old_y = lcd_y;
			old_w = lcd_w; old_h = lcd_h;
			has_old = 1;

			/* Update X/Y display (row 6) only when values change */
			if (!showed_x || tgt.x != last_disp_x || tgt.y != last_disp_y) {
				if (!showed_x) {
					/* First frame: clear "Waiting" hint */
					tsp_tft18_show_str_color(0, 6, (uint8_t *)"               ",
					                         WHITE, BLACK);
					showed_x = 1;
				}
				tsp_tft18_show_str_color(0, 6, (uint8_t *)"X:", WHITE, BLACK);
				tsp_tft18_show_int16(16, 6, tgt.x);
				tsp_tft18_show_str_color(56, 6, (uint8_t *)"Y:", WHITE, BLACK);
				tsp_tft18_show_int16(72, 6, tgt.y);
				last_disp_x = tgt.x;
				last_disp_y = tgt.y;
			}
		}

		/* Refresh F/E counters (row 7) only when values change */
		{
			uint16_t fc = (uint16_t)tsp_k230_frame_count();
			uint16_t ec = (uint16_t)tsp_k230_error_count();

			if (fc != last_fc || ec != last_ec) {
				char buf[21];
				uint8_t p = 0;

				buf[p++] = 'F'; buf[p++] = ':';
				buf[p++] = '0' + (fc / 1000) % 10;
				buf[p++] = '0' + (fc / 100) % 10;
				buf[p++] = '0' + (fc / 10) % 10;
				buf[p++] = '0' + (fc % 10);
				buf[p++] = ' ';
				buf[p++] = 'E'; buf[p++] = ':';
				buf[p++] = '0' + (ec / 1000) % 10;
				buf[p++] = '0' + (ec / 100) % 10;
				buf[p++] = '0' + (ec / 10) % 10;
				buf[p++] = '0' + (ec % 10);
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';

				tsp_tft18_show_str_color(0, 7, (uint8_t *)buf, WHITE, BLACK);
				last_fc = fc;
				last_ec = ec;
			}
		}

	}

exit_k230:
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
