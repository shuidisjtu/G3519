#include "tsp_scope.h"
#include "TSP_TFT18.h"

/* Previous frame y-values for differential update */
static uint8_t prev_y[SCOPE_WIDTH];
static uint8_t has_prev = 0;

/* Fast vertical line: set_region once + bulk pixel write */
void tsp_scope_vline(uint16_t x, uint16_t y0, uint16_t y1, uint16_t color)
{
    uint16_t ymin, ymax, i;

    if (x >= SCOPE_WIDTH) return;

    ymin = (y0 < y1) ? y0 : y1;
    ymax = (y0 > y1) ? y0 : y1;

    if (ymin < SCOPE_GRAPH_Y0) ymin = SCOPE_GRAPH_Y0;
    if (ymax > SCOPE_GRAPH_Y1) ymax = SCOPE_GRAPH_Y1;
    if (ymin > ymax) return;

    tsp_tft18_set_region(x, ymin, x, ymax);
    for (i = ymin; i <= ymax; i++) {
        tsp_tft18_write_2byte(color);
    }
}

void tsp_scope_clear(void)
{
    uint16_t x;

    /* Bulk clear graph area using set_region + fill */
    tsp_tft18_set_region(0, SCOPE_GRAPH_Y0, SCOPE_WIDTH - 1, SCOPE_GRAPH_Y1);
    for (x = 0; x < (uint16_t)SCOPE_WIDTH * SCOPE_GRAPH_H; x++) {
        tsp_tft18_write_2byte(BLACK);
    }

    has_prev = 0;
}

void tsp_scope_draw_grid(void)
{
    uint16_t x;
    uint16_t y_mid = SCOPE_GRAPH_Y0 + SCOPE_GRAPH_H / 2;
    uint16_t y_q1  = SCOPE_GRAPH_Y0 + SCOPE_GRAPH_H / 4;
    uint16_t y_q3  = SCOPE_GRAPH_Y0 + 3 * SCOPE_GRAPH_H / 4;

    /* Dotted horizontal lines (every 4 pixels) */
    for (x = 0; x < SCOPE_WIDTH; x += 4) {
        tsp_tft18_draw_pixel(x, y_mid, GRAY2);
        tsp_tft18_draw_pixel(x, y_q1,  GRAY2);
        tsp_tft18_draw_pixel(x, y_q3,  GRAY2);
    }

    /* Dotted vertical center line */
    {
        uint16_t y;
        for (y = SCOPE_GRAPH_Y0; y <= SCOPE_GRAPH_Y1; y += 4) {
            tsp_tft18_draw_pixel(SCOPE_WIDTH / 2, y, GRAY2);
        }
    }
}

void tsp_scope_draw_wave(uint16_t *samples, uint16_t count)
{
    uint8_t cur_y[SCOPE_WIDTH];
    uint16_t i;
    uint8_t y_min_old, y_max_old, y_min_new, y_max_new;
    uint16_t vmin = 4095, vmax = 0;
    uint16_t range, margin;

    if (count > SCOPE_WIDTH) count = SCOPE_WIDTH;

    /* Auto-scale: find min/max of displayed samples */
    for (i = 0; i < count; i++) {
        if (samples[i] < vmin) vmin = samples[i];
        if (samples[i] > vmax) vmax = samples[i];
    }

    range = vmax - vmin;

    if (range < 200) {
        /* Noise floor: use fixed 0~3.3V scale */
        vmin = 0;
        vmax = 4095;
    } else {
        /* Auto-scale with 5% margin */
        margin = range / 20;
        if (margin < 10) margin = 10;
        vmin = (vmin > margin) ? vmin - margin : 0;
        vmax = (vmax + margin < 4095) ? vmax + margin : 4095;
    }
    range = vmax - vmin;

    /* Map ADC values to screen Y coordinates (auto-scaled) */
    for (i = 0; i < count; i++) {
        uint16_t val = samples[i];
        if (val < vmin) val = vmin;
        if (val > vmax) val = vmax;
        cur_y[i] = (uint8_t)(SCOPE_GRAPH_Y1 -
                   (uint32_t)(val - vmin) * (SCOPE_GRAPH_H - 1) / range);
    }
    for (; i < SCOPE_WIDTH; i++) {
        cur_y[i] = SCOPE_GRAPH_Y1;
    }

    /* Differential update: erase old, draw new */
    for (i = 0; i < SCOPE_WIDTH - 1; i++) {
        /* Erase old segment */
        if (has_prev) {
            if (prev_y[i] <= prev_y[i + 1]) {
                y_min_old = prev_y[i];
                y_max_old = prev_y[i + 1];
            } else {
                y_min_old = prev_y[i + 1];
                y_max_old = prev_y[i];
            }
            tsp_scope_vline(i, y_min_old, y_max_old, BLACK);
        }

        /* Draw new segment */
        if (cur_y[i] <= cur_y[i + 1]) {
            y_min_new = cur_y[i];
            y_max_new = cur_y[i + 1];
        } else {
            y_min_new = cur_y[i + 1];
            y_max_new = cur_y[i];
        }
        tsp_scope_vline(i, y_min_new, y_max_new, GREEN);
    }

    /* Last column: single pixel */
    if (has_prev) {
        tsp_tft18_draw_pixel(SCOPE_WIDTH - 1, prev_y[SCOPE_WIDTH - 1], BLACK);
    }
    tsp_tft18_draw_pixel(SCOPE_WIDTH - 1, cur_y[SCOPE_WIDTH - 1], GREEN);

    /* Save for next frame */
    for (i = 0; i < SCOPE_WIDTH; i++) {
        prev_y[i] = cur_y[i];
    }
    has_prev = 1;
}
