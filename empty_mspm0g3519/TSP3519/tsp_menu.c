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

/* Blank line for clearing unused menu rows */
#define MENU_BLANK_STR      "                    "

/* Internal state */
static const char       *g_title;
static tsp_menu_item_t  *g_items;
static uint8_t           g_count;
static uint8_t           g_cursor;
static uint8_t           g_cursor_prev;
static uint8_t           g_needs_full;     /* 1 = full redraw (init/switch/after-action) */

/* Draw a single menu row in selected or normal style.
 * Pads text to 20 characters (160px at 8px font) so the background
 * color covers the full row — prevents highlight residue from
 * previous selection when menu items have different text lengths. */
static void menu_draw_row(uint8_t idx)
{
    uint8_t     row = MENU_ITEMS_START + idx;
    const char *src = g_items[idx].text;
    uint8_t     buf[21];  /* 20 chars + null */
    uint8_t     j;

    /* Copy text, then pad with spaces to full width */
    for (j = 0; j < 20 && src[j]; j++) {
        buf[j] = (uint8_t)src[j];
    }
    for (; j < 20; j++) {
        buf[j] = ' ';
    }
    buf[20] = '\0';

    if (idx == g_cursor) {
        tsp_tft18_show_str_color(0, row, buf,
                                 MENU_SEL_FG_COLOR, MENU_SEL_BG_COLOR);
    } else {
        tsp_tft18_show_str_color(0, row, buf,
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

    /* Confirm: S2 — execute action, then force full redraw */
    if (tsp_key_pressed(KEY_S2)) {
        if (g_items[g_cursor].action != NULL) {
            /* Clear full screen so action starts with a clean workspace */
            for (i = 0; i < 8; i++) {
                tsp_tft18_show_str_color(0, i,
                                         (uint8_t *)MENU_BLANK_STR,
                                         MENU_FG_COLOR, MENU_BG_COLOR);
            }
            g_items[g_cursor].action();
            /* Action may have overwritten LCD rows — need full redraw */
            g_needs_full = 1;
        }
    }

    /* Full redraw on init/switch/after-action */
    if (g_needs_full) {
        g_needs_full = 0;

        /* Draw title and separator */
        tsp_tft18_show_str_color(0, MENU_TITLE_ROW,
                                 (uint8_t *)g_title, BLUE, YELLOW);
        tsp_tft18_draw_line_h(0, (MENU_TITLE_ROW + 1) * 16, 160, BLUE);

        /* Draw new menu items */
        for (i = 0; i < g_count && i < MENU_ITEMS_MAX; i++) {
            menu_draw_row(i);
        }

        /* Clear any remaining rows left over from previous menu
         * (e.g. main menu has 6 items, sub-menu has only 2) */
        for (; i < MENU_ITEMS_MAX; i++) {
            tsp_tft18_show_str_color(0, MENU_ITEMS_START + i,
                                     (uint8_t *)MENU_BLANK_STR,
                                     MENU_FG_COLOR, MENU_BG_COLOR);
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
