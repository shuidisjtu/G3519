#ifndef _TSP_MCP41010_H
#define _TSP_MCP41010_H

#include <stdint.h>

/* MCP41010 100k digital potentiometer — controls AD620 Rg for PGA.
 * GPIO bit-bang SPI on PC25(CS) / PC26(SCK) / PC27(SI), J2 pin6-8. */

void tsp_pga_init(void);
void tsp_pga_set(uint8_t data);
void tsp_pga_set_gain(uint16_t gain_x10);

#endif
