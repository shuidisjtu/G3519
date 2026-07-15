#include "tsp_menu.h"
#include "tsp_key.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"

/* Layout constants */
#define MENU_TITLE_ROW      0      /* row index for title (16px per row) */
#define MENU_ITEMS_START    2      /* first item row */
#define MENU_ITEMS_MAX      6      /* max visible items per page */
#define MENU_FG_COLOR       WHITE
#define MENU_BG_COLOR       BLACK
#define MENU_SEL_FG_COLOR   BLACK
#define MENU_SEL_BG_COLOR   WHITE

/* Internal state */
static const char       *g_title;
static tsp_menu_item_t  *g_items;
static uint8_t           g_count;
static uint8_t           g_cursor;        /* currently selected index */
static uint8_t           g_redraw;        /* 1 = full redraw needed */

void tsp_menu_init(const char *title, tsp_menu_item_t *items, uint8_t count)
{
    g_title  = title;
    g_items  = items;
    g_count  = count;
    g_cursor = 0;
    g_redraw = 1;
}

void tsp_menu_run(void)
{
    uint8_t i;
    uint8_t row;

    /* Handle key navigation */
    if (tsp_key_pressed(KEY_S0)) {
        if (g_cursor > 0) {
            g_cursor--;
        } else {
            g_cursor = g_count - 1;  /* wrap to bottom */
        }
        g_redraw = 1;
    }

    if (tsp_key_pressed(KEY_S1)) {
        if (g_cursor < g_count - 1) {
            g_cursor++;
        } else {
            g_cursor = 0;  /* wrap to top */
        }
        g_redraw = 1;
    }

    if (tsp_key_pressed(KEY_PUSH)) {
        if (g_items[g_cursor].action != NULL) {
            g_items[g_cursor].action();
        }
    }

    /* S2: reserved for back, no action yet */

    if (!g_redraw) return;
    g_redraw = 0;

    /* Clear and redraw title */
    tsp_tft18_show_str_color(0, MENU_TITLE_ROW, (uint8_t *)g_title, BLUE, YELLOW);

    /* Draw separator line under title */
    tsp_tft18_draw_line_h(0, (MENU_TITLE_ROW + 1) * 16, 160, BLUE);

    /* Draw menu items */
    for (i = 0; i < g_count && i < MENU_ITEMS_MAX; i++) {
        row = MENU_ITEMS_START + i;

        if (i == g_cursor) {
            /* Selected: inverted colors */
            tsp_tft18_show_str_color(0, row, (uint8_t *)g_items[i].text,
                                     MENU_SEL_FG_COLOR, MENU_SEL_BG_COLOR);
        } else {
            /* Normal */
            tsp_tft18_show_str_color(0, row, (uint8_t *)g_items[i].text,
                                     MENU_FG_COLOR, MENU_BG_COLOR);
        }
    }
}
