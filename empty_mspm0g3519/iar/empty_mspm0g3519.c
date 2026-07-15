#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_uart.h"
#include "tsp_ccd.h"

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
	uint8_t blank[] = "                    ";

	cnt++;

	tsp_tft18_show_str_color(0, 7, (uint8_t *)"Count:", YELLOW, BLACK);
	tsp_tft18_show_uint16(48, 7, (uint16_t)cnt);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"PUSH to return...",
	                         CYAN, BLACK);

	/* S2=increment, PUSH=exit */
	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;
		if (tsp_key_pressed(KEY_S2)) {
			cnt++;
			tsp_tft18_show_str_color(0, 7, (uint8_t *)"Count:", YELLOW, BLACK);
			tsp_tft18_show_uint16(48, 7, (uint16_t)cnt);
		}
		delay_1ms(10);
	}

	tsp_tft18_show_str_color(0, 6, blank, WHITE, BLACK);
	tsp_tft18_show_str_color(0, 7, blank, WHITE, BLACK);
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

/* ===== Encoder Test ===== */

static void action_encoder(void)
{
	uint8_t  blank[] = "                    ";
	int32_t  count, last_count = 0;
	int16_t  speed;

	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Enc: PUSH=exit",
	                         CYAN, BLACK);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;

		count = tsp_encoder_get_count();
		speed = tsp_encoder_get_speed();

		if (count != last_count) {
			last_count = count;
			tsp_tft18_show_str_color(0, 7, (uint8_t *)"Cnt:", YELLOW, BLACK);
			tsp_tft18_show_int16(40, 7, (int16_t)count);
			tsp_tft18_show_str_color(100, 7, (uint8_t *)"Spd:", GREEN, BLACK);
			tsp_tft18_show_int16(136, 7, speed);
		}
		delay_1ms(10);
	}

	tsp_tft18_show_str_color(0, 6, blank, WHITE, BLACK);
	tsp_tft18_show_str_color(0, 7, blank, WHITE, BLACK);
}

/* ===== CCD Test ===== */

static void action_ccd(void)
{
	uint8_t     blank[] = "                    ";
	ccd_data_t  ccd_buf;

	tsp_tft18_show_str_color(0, 6, (uint8_t *)"CCD1 capturing...",
	                         CYAN, BLACK);

	tsp_ccd_snapshot(CCD1, ccd_buf);

	tsp_tft18_show_str_color(0, 6, (uint8_t *)"CCD1: PUSH=return",
	                         CYAN, BLACK);

	/* Show first pixel value */
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"P0:", YELLOW, BLACK);
	tsp_tft18_show_uint16(24, 7, ccd_buf[0]);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;
		delay_1ms(10);
	}

	tsp_tft18_show_str_color(0, 6, blank, WHITE, BLACK);
	tsp_tft18_show_str_color(0, 7, blank, WHITE, BLACK);
}

/* ===== Main Menu (6 items) ===== */

static tsp_menu_item_t main_menu[] = {
	{"LED Menu",      action_enter_led},
	{"Buzzer Test",   action_buzzer},
	{"Show Counter",  action_counter},
	{"Encoder Test",  action_encoder},
	{"CCD Test",      action_ccd},
	{"About",         action_about},
};

#define MAIN_MENU_COUNT  (sizeof(main_menu) / sizeof(main_menu[0]))

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Init encoder (uses SysConfig PHA0 interrupt) */
	tsp_encoder_init();

	/* Init UART0 for printf debug output (115200-8N1 on PA10/PA11) */
	tsp_uart_init(115200);

	/* Init CCD (manual ADC + GPIO for TSL1401) */
	tsp_ccd_init();

	/* Startup beep */
	BUZZ_ON();
	delay_1ms(50);
	BUZZ_OFF();

	/* Hello via UART */
	tsp_uart_send_string("\r\n=== MSPM0G3519 NUEDC-2026 ===\r\n");
	tsp_uart_send_string("UART0 online @ 115200\r\n");

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
