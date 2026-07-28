#include "tsp_mcp41010.h"
#include "tsp_gpio.h"
#include "ti_msp_dl_config.h"
#include <intrinsics.h>

#define MCP41010_CMD_WRITE  0x11

/* MCP41010 SPI timing at 3.3V: tSU >= 50ns, tCH >= 50ns, tCL >= 50ns.
 * At 80MHz each __NOP = 12.5ns; 4 NOPs = 50ns + GPIO write latency ~25ns = ~75ns. */
#define SPI_DELAY()  do { __NOP(); __NOP(); __NOP(); __NOP(); } while(0)

static void spi_write_byte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (data & 0x80)
            PGA_SI_HIGH();
        else
            PGA_SI_LOW();
        SPI_DELAY();
        PGA_SCK_HIGH();
        SPI_DELAY();
        PGA_SCK_LOW();
        data <<= 1;
    }
}

void tsp_pga_init(void)
{
    DL_GPIO_initDigitalOutput(IOMUX_PINCM90);   /* PC25 = CS  */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM91);   /* PC26 = SCK */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM92);   /* PC27 = SI  */
    DL_GPIO_enableOutput(GPIOC,
        DL_GPIO_PIN_25 | DL_GPIO_PIN_26 | DL_GPIO_PIN_27);
    PGA_CS_HIGH();
    PGA_SCK_LOW();
    PGA_SI_LOW();
}

void tsp_pga_set(uint8_t data)
{
    PGA_CS_LOW();
    spi_write_byte(MCP41010_CMD_WRITE);
    spi_write_byte(data);
    PGA_CS_HIGH();
}

void tsp_pga_set_gain(uint16_t gain_x10)
{
    uint32_t g10 = gain_x10;
    uint32_t rg;
    uint32_t d;

    if (g10 <= 10) {
        tsp_pga_set(255);
        return;
    }

    /* AD620: G = 1 + 49400/Rg  =>  Rg = 49400 / (G - 1)
     * MCP41010: Rg = data * 391 + 52  =>  data = (Rg - 52) / 391 */
    rg = 494000 / (g10 - 10);
    if (rg < 52) {
        tsp_pga_set(1);
        return;
    }
    d = (rg - 52) / 391;
    if (d < 1) d = 1;
    if (d > 255) d = 255;
    tsp_pga_set((uint8_t)d);
}
