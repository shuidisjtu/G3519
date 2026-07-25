#ifndef _TSP_CMD_H
#define _TSP_CMD_H

#include "tsp_common_headfile.h"

/* Text command protocol over UART.
 * Format: CMD[,PARAM1,PARAM2]\r\n  ->  OK[,DATA]\r\n  or  ERR,msg\r\n */

#define CMD_LINE_BUF_SIZE   128

typedef void     (*tsp_cmd_tx_fn)(const char *str);
typedef uint8_t  (*tsp_cmd_rx_fn)(void);
typedef uint16_t (*tsp_cmd_avail_fn)(void);

void tsp_cmd_init(tsp_cmd_tx_fn tx, tsp_cmd_rx_fn rx, tsp_cmd_avail_fn avail);
void tsp_cmd_poll(void);

#endif
