/**
 * @file    tsp_linefollow.c
 * @brief   CCD-based Line Following implementation
 *
 * Ported from HSP (HuashanPi) Project1_LineFollower.c.
 * Adapted for MSPM0G3519:
 *   - CCD 128-pixel 1D array replaces MT9V034 2D camera image
 *   - Differential drive replaces servo steering
 *   - PD control from tsp_pid module
 *
 * Line detection algorithm:
 *   Scan CCD pixels for left/right edges of dark region (tape).
 *   Validate width (TAPE_WIDTH_MIN..TAPE_WIDTH_MAX).
 *   Center = (left + right) / 2, error = center - LF_CCD_CENTER.
 *
 * Steering PD (position form):
 *   steer = Kp_steer * error + Kd_steer * delta_error
 *   IIR filtered: smooth = 0.7*smooth + 0.3*steer
 *
 * Speed PD (position form, from encoder feedback 20ms):
 *   dc = base_dc + Kp_spd * spd_error + Kd_spd * delta_spd
 *   Rate-limited to max ±5 per frame
 *
 * Differential drive:
 *   left_dc  = dc + steer_correct
 *   right_dc = dc - steer_correct
 *   Clamped to [-35, 60] with deadband <3%
 */

#include "tsp_linefollow.h"
#include "tsp_ccd.h"
#include "tsp_motor.h"
#include "tsp_encoder.h"
#include "tsp_wheel_enc.h"
#include "tsp_key.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"

/* =========================================================================
 * Module State
 * ========================================================================= */

static uint8_t  g_base_speed    = LF_DEFAULT_SPEED;
static float    g_kp_steer      = LF_KP_STEER_DEF;
static float    g_kd_steer      = LF_KD_STEER_DEF;
static float    g_kp_spd        = LF_KP_SPEED_DEF;
static float    g_kd_spd        = LF_KD_SPEED_DEF;

static pid_pos_t g_steer_pid;       /* Steering position PID (error in pixels) */
static pid_pos_t g_speed_pid;       /* Speed position PID (error in pulses/20ms) */

static uint8_t  g_initialized  = 0;

/* Smoothing state for steering output */
static float    g_smooth_steer = 0.0f;

/* Speed rate-limiter state */
static int16_t  g_last_dc      = 0;

/* Line loss tracking */
static uint8_t  g_lost_frames  = 0;
static float    g_smooth_mid   = (float)LF_CCD_CENTER;
static uint8_t  g_last_mid     = LF_CCD_CENTER;

/* =========================================================================
 * Initialization
 * ========================================================================= */

void tsp_linefollow_init(void)
{
    /* Initialize peripheral drivers */
    tsp_ccd_init();
    tsp_ccd_set_exposure(10);   /* 10ms integration (default) */

    /* Motor is initialized separately by caller (tsp_motor_init + SLEEP_HIGH) */

    /* Encoder is initialized separately by caller (tsp_encoder_enable) */

    /* Initialize steering PID (position form, PD-only: Ki=0) */
    tsp_pid_pos_init(&g_steer_pid, g_kp_steer, 0.0f, g_kd_steer,
                     -LF_STEER_MAX, LF_STEER_MAX, 0.0f);

    /* Initialize speed PID (position form, PD-only: Ki=0) */
    tsp_pid_pos_init(&g_speed_pid, g_kp_spd, 0.0f, g_kd_spd,
                     (float)LF_DC_MIN, (float)LF_DC_MAX, 0.0f);

    /* Reset smoothing state */
    g_smooth_steer = 0.0f;
    g_last_dc      = 0;
    g_lost_frames  = 0;
    g_smooth_mid   = (float)LF_CCD_CENTER;
    g_last_mid     = LF_CCD_CENTER;

    g_initialized = 1;
}

/* =========================================================================
 * Line Detection
 * ========================================================================= */

/**
 * @brief  Find black tape in CCD pixel array.
 *
 * Scans from pixel 2 to CCD_PIXELS-3, looking for a contiguous dark region
 * (ADC value < threshold). Validates width is within reasonable range.
 *
 * @param  pixels    128-pixel CCD data (uint16_t)
 * @param  left      Output: left edge pixel index
 * @param  right     Output: right edge pixel index
 * @return 1 if valid tape found, 0 otherwise
 */
static uint8_t find_tape(const uint16_t *pixels, uint8_t *left, uint8_t *right)
{
    uint8_t i;
    uint8_t found_left  = 0;
    uint8_t found_right = 0;
    uint8_t l = 0, r = 0;

    for (i = 2; i < CCD_PIXELS - 2; i++) {
        /* Rising edge: background -> tape (light -> dark) */
        if (!found_left && pixels[i] < LF_TAPE_THRESHOLD
            && pixels[i - 1] >= LF_TAPE_THRESHOLD) {
            found_left = 1;
            l = i;
        }
        /* Falling edge: tape -> background (dark -> light) */
        if (found_left && !found_right && pixels[i] < LF_TAPE_THRESHOLD
            && pixels[i + 1] >= LF_TAPE_THRESHOLD) {
            found_right = 1;
            r = i;
        }
        /* Validate width once both edges found */
        if (found_left && found_right) {
            uint8_t w = r - l + 1;
            if (w >= LF_TAPE_WIDTH_MIN && w <= LF_TAPE_WIDTH_MAX) {
                *left  = l;
                *right = r;
                return 1;
            }
            /* Reset and keep scanning */
            found_left  = 0;
            found_right = 0;
        }
    }

    return 0;
}

/* =========================================================================
 * Process
 * ========================================================================= */

void tsp_linefollow_process(uint8_t ccd_ch, lf_result_t *result)
{
    uint16_t pixels[CCD_PIXELS];     /* 256 bytes on stack — watch M0+ stack size */
    uint8_t  left, right, center;
    uint8_t  tape_found;
    int16_t  steer_raw;
    float    steer_filtered;
    int16_t  current_speed;
    int16_t  dc;

    /* 1. Capture CCD frame */
    tsp_ccd_snapshot(ccd_ch, pixels);

    /* 2. Find tape */
    tape_found = find_tape(pixels, &left, &right);

    /* 3. Compute line center and error */
    if (tape_found) {
        center = (left + right) / 2;
        g_lost_frames = 0;
        g_smooth_mid  = 0.65f * g_smooth_mid + 0.35f * (float)center;
        result->line_valid  = 1;
        result->line_center = center;
        result->tape_width  = right - left + 1;
    } else {
        if (g_lost_frames < 255U) g_lost_frames++;
        if (g_lost_frames <= 3) {
            /* Brief dropout: hold last position */
            center = g_last_mid;
            result->line_valid  = 1;
            result->line_center = center;
            result->tape_width  = 0;
        } else {
            /* Sustained loss: decay toward center and reduce speed */
            g_smooth_mid = 0.98f * g_smooth_mid + 0.02f * (float)LF_CCD_CENTER;
            center = (uint8_t)g_smooth_mid;
            result->line_valid  = 0;
            result->line_center = center;
            result->tape_width  = 0;
        }
    }

    /* 4. PD Steering Control */
    {
        float steer_out = tsp_pid_pos_step(&g_steer_pid,
                                           (float)LF_CCD_CENTER,
                                           (float)center);
        steer_raw = (int16_t)steer_out;

        /* IIR filter for smooth actuation (no phase lag from feedback) */
        g_smooth_steer = 0.7f * g_smooth_steer + 0.3f * (float)steer_raw;
        steer_filtered = g_smooth_steer;

        result->steer_correct = (int16_t)steer_filtered;
    }

    /* 5. Speed PD Control */
    current_speed = tsp_wheel_enc_speed(MOTOR1);   /* pulses per 20ms */
    {
        float target_spd = (float)g_base_speed * LF_SPEED_SCALE;
        float dc_f = tsp_pid_pos_step(&g_speed_pid, target_spd,
                                      (float)current_speed);
        dc = (int16_t)dc_f;

        /* Rate-limit frame-to-frame DC change */
        {
            int16_t delta = dc - g_last_dc;
            if (delta > LF_DC_RATE_LIMIT)   dc = g_last_dc + LF_DC_RATE_LIMIT;
            if (delta < -LF_DC_RATE_LIMIT)  dc = g_last_dc - LF_DC_RATE_LIMIT;
        }

        /* Clamp to motor limits */
        if (dc > LF_DC_MAX)  dc = LF_DC_MAX;
        if (dc < LF_DC_MIN)  dc = LF_DC_MIN;

        /* Reduce speed when line is lost for extended period */
        if (g_lost_frames > 3) {
            int16_t scale = 10 - (int16_t)(g_lost_frames - 3);
            if (scale < 0) scale = 0;
            dc = (int16_t)((int32_t)dc * scale / 10);
        }

        g_last_dc = dc;
        result->dc_base = dc;
    }

    /* Update position hold */
    g_last_mid = center;
}

/* =========================================================================
 * Drive
 * ========================================================================= */

void tsp_linefollow_drive(const lf_result_t *result)
{
    int16_t dc       = result->dc_base;
    int16_t steer    = result->steer_correct;
    int16_t dc_left  = dc + steer;
    int16_t dc_right = dc - steer;

    /* Independent motor clamping */
    if (dc_left > LF_DC_MAX)   dc_left = LF_DC_MAX;
    if (dc_left < LF_DC_MIN)   dc_left = LF_DC_MIN;
    if (dc_right > LF_DC_MAX)  dc_right = LF_DC_MAX;
    if (dc_right < LF_DC_MIN)  dc_right = LF_DC_MIN;

    /* Deadband: prevent 0<->3% PWM oscillation */
    if (dc_left > 0 && dc_left < 3)    dc_left = 0;
    if (dc_left < 0 && dc_left > -3)   dc_left = 0;
    if (dc_right > 0 && dc_right < 3)  dc_right = 0;
    if (dc_right < 0 && dc_right > -3) dc_right = 0;

    /* Left motor (Motor1) */
    if (dc_left > 0) {
        tsp_motor_set(MOTOR1, MOTOR_FORWARD, (uint16_t)dc_left);
    } else if (dc_left < 0) {
        tsp_motor_set(MOTOR1, MOTOR_BACKWARD, (uint16_t)(-dc_left));
    } else {
        tsp_motor_stop(MOTOR1);
    }

    /* Right motor (Motor2) */
    if (dc_right > 0) {
        tsp_motor_set(MOTOR2, MOTOR_FORWARD, (uint16_t)dc_right);
    } else if (dc_right < 0) {
        tsp_motor_set(MOTOR2, MOTOR_BACKWARD, (uint16_t)(-dc_right));
    } else {
        tsp_motor_stop(MOTOR2);
    }
}

/* =========================================================================
 * Getters / Setters
 * ========================================================================= */

int16_t tsp_linefollow_get_speed(void)
{
    return tsp_wheel_enc_speed(MOTOR1);
}

void tsp_linefollow_set_speed(uint8_t speed)
{
    if (speed > 100) speed = 100;
    g_base_speed = speed;
}

uint8_t tsp_linefollow_get_speed_val(void)
{
    return g_base_speed;
}

void tsp_linefollow_set_steer_gains(float kp, float kd)
{
    g_kp_steer = kp;
    g_kd_steer = kd;
    g_steer_pid.kp = kp;
    g_steer_pid.kd = kd;
}

void tsp_linefollow_set_speed_gains(float kp, float kd)
{
    g_kp_spd = kp;
    g_kd_spd = kd;
    g_speed_pid.kp = kp;
    g_speed_pid.kd = kd;
}

void tsp_linefollow_get_steer_gains(float *kp, float *kd)
{
    *kp = g_kp_steer;
    *kd = g_kd_steer;
}

void tsp_linefollow_get_speed_gains(float *kp, float *kd)
{
    *kp = g_kp_spd;
    *kd = g_kd_spd;
}

/* =========================================================================
 * Demo (LCD + interactive)
 * ========================================================================= */

#define LF_VAL_COL   80    /* pixel column for numeric values */

void tsp_linefollow_demo(void)
{
    lf_result_t  lf;
    uint8_t      run         = 0;     /* 0=stopped, 1=running */
    uint8_t      redraw      = 1;
    uint8_t      ccd_ch      = CCD1;
    uint16_t     tick        = 0;
    char         buf[21];

    /* Initialize subsystems */
    tsp_motor_init();
    SLEEP_HIGH();
    tsp_encoder_enable();
    tsp_wheel_enc_start();
    tsp_linefollow_init();

    /* Draw static UI */
    tsp_tft18_clear(BLACK);
    tsp_tft18_show_str_color(0, 0, (uint8_t *)"Line Follow CCD", YELLOW, BLUE);
    tsp_tft18_draw_line_h(0, 16, 160, BLUE);

    /* Row 2-5: data labels */
    tsp_tft18_show_str_color(0, 2, (uint8_t *)"Line:", CYAN, BLACK);
    tsp_tft18_show_str_color(0, 3, (uint8_t *)"Width:", CYAN, BLACK);
    tsp_tft18_show_str_color(0, 4, (uint8_t *)"Speed:", CYAN, BLACK);
    tsp_tft18_show_str_color(0, 5, (uint8_t *)"DC:", CYAN, BLACK);

    /* Status */
    tsp_tft18_show_str_color(0, 6, (uint8_t *)"Status:", CYAN, BLACK);
    tsp_tft18_show_str_color(LF_VAL_COL, 6, (uint8_t *)"STOP", BLACK, GREEN);

    /* Hint bar */
    tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:Spd S2:Run", BLACK, GREEN);

    while (1)
    {
        /*
         * Key handling (distributed across buttons)
         */
        tsp_key_scan();

        /* PUSH: Exit */
        if (tsp_key_pressed(KEY_PUSH)) {
            break;
        }

        /* S0: Decrease speed */
        if (tsp_key_pressed(KEY_S0)) {
            if (g_base_speed >= LF_SPEED_STEP) {
                g_base_speed -= LF_SPEED_STEP;
            } else {
                g_base_speed = 0;
            }
            tsp_linefollow_set_speed(g_base_speed);
            redraw = 1;
        }

        /* S1: Increase speed */
        if (tsp_key_pressed(KEY_S1)) {
            if (g_base_speed <= 95) {
                g_base_speed += LF_SPEED_STEP;
            } else {
                g_base_speed = 100;
            }
            tsp_linefollow_set_speed(g_base_speed);
            redraw = 1;
        }

        /* S2: Toggle run/stop */
        if (tsp_key_pressed(KEY_S2)) {
            run = !run;
            if (!run) {
                tsp_motor_stop_all();
            }
            redraw = 1;
        }

        /*
         * Process one CCD frame
         */
        if (run) {
            tsp_linefollow_process(ccd_ch, &lf);
            tsp_linefollow_drive(&lf);
        } else {
            /* Even when stopped, keep reading CCD for display */
            tsp_linefollow_process(ccd_ch, &lf);
            /* Don't drive motors */
        }

        /*
         * LCD incremental update
         */
        if (redraw || tick >= 5) {   /* refresh every ~50ms */
            tick = 0;

            /* Line center */
            if (lf.line_valid) {
                tsp_tft18_show_str_color(LF_VAL_COL, 2,
                    (uint8_t *)"OK      ", GREEN, BLACK);
            } else {
                tsp_tft18_show_str_color(LF_VAL_COL, 2,
                    (uint8_t *)"LOST    ", RED, BLACK);
            }

            /* Line position (pixel index) */
            {
                uint8_t p = 0;
                buf[p++] = 'P'; buf[p++] = ':';
                buf[p++] = '0' + (lf.line_center / 100) % 10;
                buf[p++] = '0' + (lf.line_center / 10) % 10;
                buf[p++] = '0' + (lf.line_center % 10);
                while (p < 8) buf[p++] = ' ';
                buf[8] = '\0';
                tsp_tft18_show_str_color(LF_VAL_COL + 32, 2,
                    (uint8_t *)buf, WHITE, BLACK);
            }

            /* Tape width */
            tsp_tft18_show_uint16(LF_VAL_COL, 3, (uint16_t)lf.tape_width);

            /* Speed (pulses/20ms) */
            tsp_tft18_show_int16(LF_VAL_COL, 4, tsp_wheel_enc_speed(MOTOR1));

            /* DC command */
            tsp_tft18_show_int16(LF_VAL_COL, 5, lf.dc_base);

            /* Status + set speed */
            {
                uint8_t p = 0;
                buf[p++] = 'S'; buf[p++] = 'e'; buf[p++] = 't'; buf[p++] = ':';
                buf[p++] = '0' + (g_base_speed / 100) % 10;
                buf[p++] = '0' + (g_base_speed / 10) % 10;
                buf[p++] = '0' + (g_base_speed % 10);
                buf[p++] = '%';
                buf[p++] = ' ';
                if (run) {
                    buf[p++] = 'R'; buf[p++] = 'U'; buf[p++] = 'N';
                } else {
                    buf[p++] = 'S'; buf[p++] = 'T'; buf[p++] = 'O'; buf[p++] = 'P';
                }
                while (p < 20) buf[p++] = ' ';
                buf[20] = '\0';
                tsp_tft18_show_str_color(0, 6, (uint8_t *)buf, WHITE, BLACK);
            }

            redraw = 0;
        }

        tick++;
        delay_1ms(10);
    }

    /* Cleanup */
    tsp_motor_stop_all();
    SLEEP_LOW();
    tsp_wheel_enc_stop();
    tsp_encoder_disable();
}
