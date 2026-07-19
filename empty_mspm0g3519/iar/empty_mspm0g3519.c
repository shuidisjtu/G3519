#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_uart_k230.h"
#include "tsp_k230.h"

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

/* ===== K230 Vision Test (color tracking with LCD overlay) ===== */

static void action_k230_test(void)
{
	k230_target_t tgt;
	int16_t lcd_x, lcd_y, lcd_w, lcd_h;
	int16_t old_x = 0, old_y = 0, old_w = 0, old_h = 0;
	int16_t last_disp_x = -1, last_disp_y = -1;
	uint16_t last_fc = 0xFFFF, last_ec = 0xFFFF;
	uint8_t has_old = 0, showed_x = 0, toggle_cnt = 0;

	/* Full-screen tracking view */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"K230 Track", YELLOW, BLACK);
	tsp_tft18_draw_line_h(0, 96, 160, BLUE);    /* divider y=96 */

	/* Initial diagnostic: show we're waiting */
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Waiting for K230...", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"F:   0 E:   0", WHITE, BLACK);

	tsp_k230_init();
	tsp_uart_k230_flush_rx();
	tsp_uart_k230_rx_enable();

	while (1) {
		/* Scan keys at 1ms intervals */
		{
			uint8_t i;
			for (i = 0; i < 5; i++) {
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) goto exit_k230;
				if (tsp_key_pressed(KEY_S0)) {
					/* Send toggle command to K230 over UART6 TX */
					tsp_uart_k230_send_string("$SWITCH#\n");
					toggle_cnt++;
					/* Show toggle count on row 5 */
					{
						char tb[4];
						tb[0] = 'T'; tb[1] = ':';
						tb[2] = '0' + (toggle_cnt % 10);
						tb[3] = '\0';
						tsp_tft18_show_str_color(90, 0, (uint8_t *)tb, WHITE, BLACK);
					}
				}
				delay_1ms(1);
			}
		}

		tsp_k230_task();

		if (tsp_k230_get_target(&tgt)) {
			/* Map K230 640x480 -> LCD canvas 160x80 (y=16..95):
			 *   X: /4 (0..639 → 0..159),  Y: /6+16 (0..479 → 16..95) */
			lcd_x = tgt.x / 4;
			lcd_y = 16 + tgt.y / 6;
			lcd_w = tgt.w / 4;
			lcd_h = tgt.h / 6;

			/* Clamp to canvas (title y=0..15, bottom divider y=104, x <= 159) */
			if (lcd_x < 0) lcd_x = 0;
			if (lcd_x + lcd_w > 159) lcd_w = 159 - lcd_x;

			/* Protect title row: push top edge below row 0 (16 px) */
			if (lcd_y < 16) {
				lcd_h -= (16 - lcd_y);
				lcd_y = 16;
			}
			if (lcd_y + lcd_h > 95) lcd_h = 95 - lcd_y;
			if (lcd_w < 2) lcd_w = 2;
			if (lcd_h < 2) lcd_h = 2;

			/* Redraw rect only when position/size changes (prevents flicker) */
			if (!has_old || lcd_x != old_x || lcd_y != old_y ||
			    lcd_w != old_w || lcd_h != old_h) {

				if (has_old) {
					/* Erase old rect at previous position */
					tsp_tft18_draw_frame((uint8_t)old_x, (uint8_t)old_y,
					                     (uint8_t)old_w, (uint8_t)old_h, BLACK);
				}

				/* Draw new rect at current position */
				tsp_tft18_draw_frame((uint8_t)lcd_x, (uint8_t)lcd_y,
				                     (uint8_t)lcd_w, (uint8_t)lcd_h, GREEN);

				old_x = lcd_x; old_y = lcd_y;
				old_w = lcd_w; old_h = lcd_h;
				has_old = 1;
			}

			/* Update X/Y display (row 6) only when values change */
			if (!showed_x || tgt.x != last_disp_x || tgt.y != last_disp_y) {
				if (!showed_x) {
					/* First frame: clear "Waiting" hint */
					tsp_tft18_show_str_color(0, 6,
					    (uint8_t *)"                    ", WHITE, BLACK);
					showed_x = 1;
				}
				/* uint16 (5 digits, 40 px) — fits within allocated space */
				tsp_tft18_show_str_color(0, 6, (uint8_t *)"X:", WHITE, BLACK);
				tsp_tft18_show_uint16(16, 6, (uint16_t)(tgt.x / 4));
				tsp_tft18_show_str_color(56, 6, (uint8_t *)" Y:", WHITE, BLACK);
				tsp_tft18_show_uint16(80, 6, (uint16_t)tgt.y);
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

/* ===== Main Menu ===== */

static tsp_menu_item_t main_menu[] = {
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
