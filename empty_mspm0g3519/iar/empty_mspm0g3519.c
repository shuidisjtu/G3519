#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;
static uint8_t  g_in_submenu = 0;
static uint8_t  g_blink_on   = 0;
static uint32_t g_blink_tick = 0;

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

/* ===== LED Sub-Menu ===== */

static void sub_led_toggle(void)
{
	LED_TOGGLE();
}

static void sub_led_blink(void)
{
	g_blink_on = 1;
	LED_OFF();
}

static tsp_menu_item_t led_submenu[] = {
	{"LED Toggle",  sub_led_toggle},
	{"LED Blink",   sub_led_blink},
};

/* Enter LED sub-menu from main menu */
static void action_enter_led(void)
{
	tsp_menu_switch("--- LED Menu ---", led_submenu, 2);
	g_in_submenu = 1;
}

/* ===== Main Menu Actions ===== */

static void action_buzzer(void)
{
	BUZZ_ON();
	delay_1ms(80);
	BUZZ_OFF();
}

static void action_counter(void)
{
	static uint16_t cnt_s0 = 0, cnt_s1 = 0, cnt_s2 = 0, cnt_push = 0;
	uint8_t i;

	/* Clear rows 1-7 so old menu items don't show underneath */
	for (i = 1; i < 8; i++) {
		tsp_tft18_show_str_color(0, i,
		                         (uint8_t *)"                    ",
		                         WHITE, BLACK);
	}

	/* Row 1: title */
	tsp_tft18_show_str_color(0, 1, (uint8_t *)"Key Counter",
	                         YELLOW, BLACK);

	/* Row 2: S0 + S1 labels and initial values */
	tsp_tft18_show_str_color(0,  2, (uint8_t *)"S0:", YELLOW, BLACK);
	tsp_tft18_show_uint16(24, 2, cnt_s0);
	tsp_tft18_show_str_color(72, 2, (uint8_t *)"S1:", YELLOW, BLACK);
	tsp_tft18_show_uint16(96, 2, cnt_s1);

	/* Row 3: S2 + PUSH labels and initial values */
	tsp_tft18_show_str_color(0,  3, (uint8_t *)"S2:", YELLOW, BLACK);
	tsp_tft18_show_uint16(24, 3, cnt_s2);
	tsp_tft18_show_str_color(72, 3, (uint8_t *)"PUSH:", YELLOW, BLACK);
	tsp_tft18_show_uint16(112, 3, cnt_push);

	/* Row 4: hint */
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Any key +1 PUSH:exit",
	                         CYAN, BLACK);

	/* Any key increments its counter; PUSH also exits */
	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_S0))   { cnt_s0++;   tsp_tft18_show_uint16(24,  2, cnt_s0);   }
		if (tsp_key_pressed(KEY_S1))   { cnt_s1++;   tsp_tft18_show_uint16(96,  2, cnt_s1);   }
		if (tsp_key_pressed(KEY_S2))   { cnt_s2++;   tsp_tft18_show_uint16(24,  3, cnt_s2);   }
		if (tsp_key_pressed(KEY_PUSH)) { cnt_push++; tsp_tft18_show_uint16(112, 3, cnt_push); break; }
		delay_1ms(10);
	}

	/* Request full menu redraw when we return */
	tsp_menu_request_redraw();
}

static void action_about(void)
{
	uint8_t i;

	/* Clear rows 1-7 so old menu items don't show underneath */
	for (i = 1; i < 8; i++) {
		tsp_tft18_show_str_color(0, i,
		                         (uint8_t *)"                    ",
		                         WHITE, BLACK);
	}

	/* Row 1: action title, rows 2-3: content */
	tsp_tft18_show_str_color(0, 1, (uint8_t *)"About",
	                         YELLOW, BLACK);
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"NUEDC-2026 SAIS@SJTU",
	                         GREEN, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"PUSH to return...",
	                         CYAN, BLACK);

	/* Wait for PUSH to dismiss */
	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;
		delay_1ms(10);
	}

	/* Request full menu redraw when we return */
	tsp_menu_request_redraw();
}

/* ===== Main Menu (4 items) ===== */

static tsp_menu_item_t main_menu[] = {
	{"LED Menu",      action_enter_led},
	{"Buzzer Test",   action_buzzer},
	{"Show Counter",  action_counter},
	{"About",         action_about},
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

	tsp_key_init();
	tsp_menu_init("=== NUEDC 2026 ===", main_menu, MAIN_MENU_COUNT);

	while (1) {
		tsp_key_scan();

		/* Background: LED blink at ~2Hz */
		if (g_blink_on) {
			uint32_t now = sys_tick_counter;
			if (now - g_blink_tick >= 250) {
				g_blink_tick = now;
				LED_TOGGLE();
			}
		}

		/* Menu returns 1 when PUSH (back/exit) pressed */
		if (tsp_menu_run()) {
			if (g_in_submenu) {
				/* Return from sub-menu to main menu */
				tsp_menu_switch("=== NUEDC 2026 ===", main_menu,
				                MAIN_MENU_COUNT);
				g_in_submenu = 0;
				g_blink_on   = 0;
				LED_OFF();
			}
		}

		delay_1ms(10);
	}
}
