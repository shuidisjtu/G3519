#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;
static uint8_t  g_in_submenu = 0;
static uint8_t  g_blink_on   = 0;
static uint32_t g_blink_tick = 0;

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
	static uint32_t cnt = 0;
	cnt++;

	tsp_tft18_show_str_color(0, 7, (uint8_t *)"Count:", YELLOW, BLACK);
	tsp_tft18_show_uint16(48, 7, (uint16_t)cnt);
}

static void action_about(void)
{
	uint8_t blank[] = "                    ";

	tsp_tft18_show_str_color(0, 6, (uint8_t *)"NUEDC-2026 SAIS@SJTU",
	                         GREEN, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to return...",
	                         CYAN, BLACK);

	/* Wait for PUSH to dismiss */
	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;
		delay_1ms(10);
	}

	/* Clear lines and let menu redraw */
	tsp_tft18_show_str_color(0, 6, blank, WHITE, BLACK);
	tsp_tft18_show_str_color(0, 7, blank, WHITE, BLACK);
}

static tsp_menu_item_t main_menu[] = {
	{"LED Menu",      action_enter_led},
	{"Buzzer Test",   action_buzzer},
	{"Show Counter",  action_counter},
	{"About",         action_about},
};

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Startup beep */
	BUZZ_ON();
	delay_1ms(50);
	BUZZ_OFF();

	tsp_key_init();
	tsp_menu_init("=== NUEDC 2026 ===", main_menu,
	              sizeof(main_menu) / sizeof(main_menu[0]));

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
				tsp_menu_switch("=== NUEDC 2026 ===", main_menu, 4);
				g_in_submenu = 0;
				g_blink_on   = 0;
				LED_OFF();
			}
		}

		delay_1ms(10);
	}
}
