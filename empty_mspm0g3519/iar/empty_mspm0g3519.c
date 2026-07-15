#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"

int main(void)
{
	uint32_t count=0;

	SYSCFG_DL_init();

	tsp_tft18_init();
	tsp_tft18_test_color();
	tsp_tft18_show_str_color(0, 0, "NUEDC-2026 SAIS@SJTU", BLUE, YELLOW);

	while (1) {
      delay_1ms(100);
      LED_TOGGLE();
      BUZZ_ON();
      delay_1ms(5);
      BUZZ_OFF();
      delay_1ms(100);
   }
}
