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
static uint8_t           g_cursor;
static uint8_t           g_cursor_prev;
static uint8_t           g_needs_full;     /* 1 = full redraw (init/switch) */

/* Draw a single menu row in selected or normal style */
static void menu_draw_row(uint8_t idx)
{
    uint8_t row = MENU_ITEMS_START + idx;

    if (idx == g_cursor) {
        tsp_tft18_show_str_color(0, row, (uint8_t *)g_items[idx].text,
                                 MENU_SEL_FG_COLOR, MENU_SEL_BG_COLOR);
    } else {
        tsp_tft18_show_str_color(0, row, (uint8_t *)g_items[idx].text,
                                 MENU_FG_COLOR, MENU_BG_COLOR);
    }
}

void tsp_menu_init(const char *title, tsp_menu_item_t *items, uint8_t count)
{
    g_title        = title;
    g_items        = items;
    g_count        = count;
    g_cursor       = 0;
    g_cursor_prev  = 0;
    g_needs_full   = 1;
}

void tsp_menu_switch(const char *title, tsp_menu_item_t *items, uint8_t count)
{
    g_title        = title;
    g_items        = items;
    g_count        = count;
    g_cursor       = 0;
    g_cursor_prev  = 0;
    g_needs_full   = 1;
}

/*
 * Run one cycle of menu logic.
 * Returns 1 if back (KEY_PUSH) was pressed during this call.
 */
uint8_t tsp_menu_run(void)
{
    uint8_t i;
    uint8_t back = 0;

    /* Back/Exit: PUSH */
    if (tsp_key_pressed(KEY_PUSH)) {
        back = 1;
    }

    /* Handle key navigation */
    g_cursor_prev = g_cursor;

    if (tsp_key_pressed(KEY_S0)) {
        if (g_cursor > 0) {
            g_cursor--;
        } else {
            g_cursor = g_count - 1;
        }
    }

    if (tsp_key_pressed(KEY_S1)) {
        if (g_cursor < g_count - 1) {
            g_cursor++;
        } else {
            g_cursor = 0;
        }
    }

    /* Confirm: S2 */
    if (tsp_key_pressed(KEY_S2)) {
        if (g_items[g_cursor].action != NULL) {
            g_items[g_cursor].action();
        }
    }

    /* Full redraw on init/switch */
    if (g_needs_full) {
        g_needs_full = 0;

        tsp_tft18_show_str_color(0, MENU_TITLE_ROW,
                                 (uint8_t *)g_title, BLUE, YELLOW);
        tsp_tft18_draw_line_h(0, (MENU_TITLE_ROW + 1) * 16, 160, BLUE);

        for (i = 0; i < g_count && i < MENU_ITEMS_MAX; i++) {
            menu_draw_row(i);
        }
        return back;
    }

    /* Incremental update: only redraw rows whose selection state changed */
    if (g_cursor != g_cursor_prev) {
        menu_draw_row(g_cursor_prev);  /* old cursor -> unselected */
        menu_draw_row(g_cursor);       /* new cursor -> selected   */
    }

    return back;
}
