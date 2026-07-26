/**
 * @file    tsp_odometer.c
 * @brief   Odometer implementation — encoder-based distance & angle measurement
 *
 * Ported from HSP Odometer.c (GD32F4xx). Adapted for MSPM0G3519.
 *
 * Uses tsp_encoder driver (PHA0/PHB0 quadrature decode via EXTI interrupt).
 * Encoder speed is updated every 20ms by tsp_encoder_update_speed() from
 * SysTick_Handler. Pulse count is read atomically via tsp_encoder_get_count().
 *
 * Display: incremental refresh — only redraw fields that changed.
 *
 * Key distribution:
 *   S0 (PA18): Straight mode   S1 (PC0): Rotate mode
 *   S2 (PA16): Reset counts    PUSH (PA12): Exit
 */

#include "tsp_odometer.h"
#include "tsp_encoder.h"
#include "tsp_wheel_enc.h"
#include "tsp_key.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"

/* Display layout constants */
#define ODOM_VAL_COL   72        /* pixel column for numeric values */

/* =========================================================================
 * Static Helpers
 * ========================================================================= */

/**
 * @brief  Draw static frame elements (title, labels, divider, hints).
 */
static void odom_draw_frame(odometer_mode_t mode)
{
    tsp_tft18_clear(BLACK);
    tsp_tft18_show_str_color(0, 0, (uint8_t *)"== Odometer ==", WHITE, DARKBLUE);

    /* Row 1: mode label */
    tsp_tft18_show_str_color(0, 1, (uint8_t *)"Mode:", CYAN, BLACK);

    /* Row 2-3: distance & angle labels */
    tsp_tft18_show_str_color(0, 2, (uint8_t *)"Dist:", CYAN, BLACK);
    tsp_tft18_show_str_color(0, 3, (uint8_t *)"Angle:", CYAN, BLACK);

    /* Row 4: divider */
    tsp_tft18_draw_line_h(1, 64, 158, GRAY2);

    /* Row 5-6: raw encoder & speed */
    tsp_tft18_show_str_color(0, 5, (uint8_t *)"Pulse:", CYAN, BLACK);
    tsp_tft18_show_str_color(0, 6, (uint8_t *)"Speed:", CYAN, BLACK);

    /* Row 7: hint bar */
    tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0:Dis S1:Ang S2:Rst", BLACK, GREEN);
}

/**
 * @brief  Update mode indicator on LCD (incremental).
 */
static void odom_update_mode_lcd(odometer_mode_t mode)
{
    if (mode == ODOM_MODE_STRAIGHT) {
        tsp_tft18_show_str_color(ODOM_VAL_COL, 1,
            (uint8_t *)"STRAIGHT", BLACK, YELLOW);
    } else {
        tsp_tft18_show_str_color(ODOM_VAL_COL, 1,
            (uint8_t *)"ROTATE  ", BLACK, CYAN);
    }
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void tsp_odometer_demo(void)
{
    odometer_mode_t mode = ODOM_MODE_STRAIGHT;

    /* Encoder accumulator (independent of tsp_encoder's internal count) */
    int32_t  pulse_total  = 0;
    int32_t  last_count   = 0;
    uint8_t  first_read   = 1;    /* skip first read (anchoring) */

    /* Display cache for incremental refresh */
    float    last_disp_dist  = -1.0f;
    float    last_disp_ang   = -1.0f;
    int32_t  last_disp_pulse = -1;
    int16_t  last_disp_speed = -999;

    float    distance = 0.0f;
    float    angle    = 0.0f;

    char     line[20];

    /* Initialize encoder (already enabled by caller) */
    tsp_wheel_enc_reset();

    /* Draw initial frame */
    odom_draw_frame(mode);
    odom_update_mode_lcd(mode);

    /* Show initial zeros */
    tsp_tft18_show_str_color(ODOM_VAL_COL, 2, (uint8_t *)"0.0 cm   ", BLACK, WHITE);
    tsp_tft18_show_str_color(ODOM_VAL_COL, 3, (uint8_t *)"0.0 deg  ", BLACK, WHITE);
    tsp_tft18_show_str_color(ODOM_VAL_COL, 5, (uint8_t *)"0         ", BLACK, WHITE);
    tsp_tft18_show_str_color(ODOM_VAL_COL, 6, (uint8_t *)"0         ", BLACK, WHITE);

    while (1)
    {
        /*
         * Read encoder (atomic on M0+ via PRIMASK in tsp_encoder_get_count)
         */
        int32_t cur_count = tsp_wheel_enc_count(MOTOR1);

        if (first_read) {
            /* Anchor: skip delta on first read */
            last_count  = cur_count;
            pulse_total = 0;
            first_read  = 0;
        } else {
            int32_t delta = cur_count - last_count;
            if (delta != 0) {
                /*
                 * Encoder polarity:
                 *   Forward  = positive count (CW on rotary knob)
                 *   Backward = negative count
                 *
                 * For displacement, we accumulate signed pulses.
                 * Driving backward decreases pulse_total.
                 * In STRAIGHT mode: distance = pulses / COUNTS_PER_CM
                 * In ROTATE mode:   angle    = pulses / COUNTS_PER_DEGREE
                 */
                {
                    int32_t new_total = pulse_total + delta;
                    if ((delta > 0 && new_total < pulse_total) ||
                        (delta < 0 && new_total > pulse_total)) {
                        /* saturate on overflow */
                    } else {
                        pulse_total = new_total;
                    }
                }
                last_count   = cur_count;

                /* Compute derived values */
                if (mode == ODOM_MODE_STRAIGHT) {
                    distance = (float)pulse_total / (float)ODOM_COUNTS_PER_CM;
                    angle    = 0.0f;
                } else {
                    distance = 0.0f;
                    angle    = (float)pulse_total / (float)ODOM_COUNTS_PER_DEGREE;
                }
            }
        }

        /*
         * Incremental LCD update (only redraw changed fields)
         */
        /* Distance */
        {
            float d = (distance < 0.0f) ? -distance : distance;
            float ld = (last_disp_dist < 0.0f) ? -last_disp_dist : last_disp_dist;
            float diff = d - ld;
            if (diff < 0.0f) diff = -diff;
            if (diff >= 0.05f || last_disp_dist < 0.0f) {
                snprintf(line, sizeof(line), "%0.1f cm ", distance);
                tsp_tft18_show_str_color(ODOM_VAL_COL, 2,
                    (uint8_t *)line, BLACK, WHITE);
                last_disp_dist = distance;
            }
        }

        /* Angle */
        {
            float a = (angle < 0.0f) ? -angle : angle;
            float la = (last_disp_ang < 0.0f) ? -last_disp_ang : last_disp_ang;
            float diff = a - la;
            if (diff < 0.0f) diff = -diff;
            if (diff >= 0.05f || last_disp_ang < 0.0f) {
                snprintf(line, sizeof(line), "%0.1f deg", angle);
                tsp_tft18_show_str_color(ODOM_VAL_COL, 3,
                    (uint8_t *)line, BLACK, WHITE);
                last_disp_ang = angle;
            }
        }

        /* Raw pulse count */
        {
            uint32_t p = (pulse_total < 0)
                ? (uint32_t)(-(pulse_total + 1)) + 1U
                : (uint32_t)pulse_total;
            if ((int32_t)p != last_disp_pulse || last_disp_pulse < 0) {
                snprintf(line, sizeof(line), "%lu     ", (unsigned long)p);
                tsp_tft18_show_str_color(ODOM_VAL_COL, 5,
                    (uint8_t *)line, BLACK, WHITE);
                last_disp_pulse = (int32_t)(p > (uint32_t)INT32_MAX ? INT32_MAX : p);
            }
        }

        /* Speed */
        {
            int16_t spd = tsp_wheel_enc_speed(MOTOR1);
            if (spd != last_disp_speed || last_disp_speed == -999) {
                tsp_tft18_show_int16(ODOM_VAL_COL, 6, spd);
                last_disp_speed = spd;
            }
        }

        /*
         * Key handling (each button has a DISTINCT function)
         */
        tsp_key_scan();

        /* S0: Straight mode (distance tracking) */
        if (tsp_key_pressed(KEY_S0)) {
            if (mode != ODOM_MODE_STRAIGHT) {
                mode = ODOM_MODE_STRAIGHT;
                pulse_total = 0;
                distance    = 0.0f;
                angle       = 0.0f;
                first_read  = 1;
                tsp_wheel_enc_reset();
                odom_update_mode_lcd(mode);
                /* Force display refresh */
                last_disp_dist  = -1.0f;
                last_disp_ang   = -1.0f;
                last_disp_pulse = -1;
                last_disp_speed = -999;
            }
        }

        /* S1: Rotate mode (angle tracking) */
        if (tsp_key_pressed(KEY_S1)) {
            if (mode != ODOM_MODE_ROTATE) {
                mode = ODOM_MODE_ROTATE;
                pulse_total = 0;
                distance    = 0.0f;
                angle       = 0.0f;
                first_read  = 1;
                tsp_wheel_enc_reset();
                odom_update_mode_lcd(mode);
                last_disp_dist  = -1.0f;
                last_disp_ang   = -1.0f;
                last_disp_pulse = -1;
                last_disp_speed = -999;
            }
        }

        /* S2: Reset counts */
        if (tsp_key_pressed(KEY_S2)) {
            pulse_total = 0;
            distance    = 0.0f;
            angle       = 0.0f;
            first_read  = 1;
            tsp_encoder_reset();
            last_disp_dist  = -1.0f;
            last_disp_ang   = -1.0f;
            last_disp_pulse = -1;
            last_disp_speed = -999;
        }

        /* PUSH: Exit */
        if (tsp_key_pressed(KEY_PUSH)) {
            break;
        }

        delay_1ms(10);
    }

    /* Wait for PUSH release */
    while (tsp_key_state(KEY_PUSH)) {
        tsp_key_scan();
        delay_1ms(10);
    }
}
