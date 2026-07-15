#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"

/* ===== Menu Action Callbacks ===== */

static void action_led_toggle(void)
{
	LED_TOGGLE();
}

static void action_buzzer_test(void)
{
	/* Short beep for feedback */
	BUZZ_ON();
	delay_1ms(80);
	BUZZ_OFF();
}

static void action_show_counter(void)
{
	static uint32_t cnt = 0;
	cnt++;

	tsp_tft18_show_str_color(0, 7, (uint8_t *)"Count:", YELLOW, BLACK);
	tsp_tft18_show_uint16(48, 7, (uint16_t)cnt);
}

static void action_about(void)
{
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"NUEDC-2026 SAIS@SJTU",
	                         GREEN, BLACK);
}

/* ===== Menu Definition ===== */

static tsp_menu_item_t menu_items[] = {
	{"LED Toggle",   action_led_toggle},
	{"Buzzer Test",  action_buzzer_test},
	{"Show Counter", action_show_counter},
	{"About",        action_about},
};

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Startup beep: short buzz then stop */
	BUZZ_ON();
	delay_1ms(50);
	BUZZ_OFF();

	tsp_key_init();
	tsp_menu_init("=== NUEDC 2026 ===", menu_items,
	              sizeof(menu_items) / sizeof(menu_items[0]));

	while (1) {
		tsp_key_scan();
		tsp_menu_run();
		delay_1ms(10);  /* 10ms scan interval */
	}
}
