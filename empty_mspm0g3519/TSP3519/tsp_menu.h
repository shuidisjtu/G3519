#ifndef _TSP_MENU_H
#define _TSP_MENU_H

#include "tsp_common_headfile.h"
#include "TSP_TFT18.h"

typedef struct {
    const char *text;          /* display text (max ~18 chars for 8px font on 160px) */
    void (*action)(void);      /* callback on select, NULL = grayed / inactive */
} tsp_menu_item_t;

void tsp_menu_init(const char *title, tsp_menu_item_t *items, uint8_t count);
void tsp_menu_run(void);       /* call in main loop, handles keys and display */

#endif
