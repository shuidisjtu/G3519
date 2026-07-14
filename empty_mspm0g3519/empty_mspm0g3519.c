#include "ti_msp_dl_config.h"

int main(void)
{
    SYSCFG_DL_init();

    /* PB5 as output, initial high (D1 LED off, active low) */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM18);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_5);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_5);

    while (1) {
        delay_cycles(8000000);               /* ~250ms @ 32MHz */
        DL_GPIO_togglePins(GPIOB, DL_GPIO_PIN_5);
    }
}