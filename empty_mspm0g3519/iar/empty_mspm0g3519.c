#include "ti_msp_dl_config.h"
#include <math.h>
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_wheel_enc.h"
#include "tsp_uart_k230.h"
#include "tsp_k230.h"
#include "tsp_motor.h"
#include "tsp_ccd.h"
#include "tsp_mpu6050.h"
#include "tsp_pid.h"
#include "tsp_odometer.h"
#include "tsp_linefollow.h"

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;

/* ===== Boot Animation ===== */

static void boot_animation(void)
{
	/* Color test sequence (like teacher's tsp_tft18_test_color) */
	tsp_tft18_test_color();

	/* Show boot info */
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"NUEDC-2026 Control", BLUE, YELLOW);
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"MSPM0G3519  80MHz", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"LCD 160x128  SPI1", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Initializing...", GREEN, BLACK);

	/* LED blink during boot */
	LED_ON();
	delay_1ms(200);
	LED_OFF();
	delay_1ms(200);
	LED_ON();
	delay_1ms(200);
	LED_OFF();

	/* Startup beep */
	BUZZ_ON();
	delay_1ms(50);
	BUZZ_OFF();

	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Ready!          ", GREEN, BLACK);
	delay_1ms(500);

	/* Clear screen before entering menu */
	tsp_tft18_clear(BLACK);
}

/* ===== K230 Vision Test (multi-function: color/QR/AprilTag/face tracking) ===== */

static void action_k230_test(void)
{
	k230_target_t tgt;
	int16_t lcd_x, lcd_y, lcd_w, lcd_h;
	int16_t old_x = 0, old_y = 0, old_w = 0, old_h = 0;
	int16_t last_disp_x = -1, last_disp_y = -1;
	uint32_t last_fc = 0xFFFFFFFF, last_ec = 0xFFFFFFFF;
	uint8_t has_old = 0, showed_x = 0, toggle_cnt = 0;

	/* Full-screen tracking view */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"K230 Track", YELLOW, BLACK);
	tsp_tft18_draw_line_h(0, 96, 160, BLUE);    /* divider y=96 */

	/* Initial diagnostic: show we're waiting */
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Waiting for K230...", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"F:   0 E:   0", WHITE, BLACK);

	tsp_k230_init();
	tsp_uart_k230_flush_rx();
	tsp_uart_k230_rx_enable();

	while (1) {
		/* Scan keys at 1ms intervals */
		{
			uint8_t i;
			for (i = 0; i < 5; i++) {
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) goto exit_k230;
				/* S0: Send toggle command to K230 over UART6 TX */
				if (tsp_key_pressed(KEY_S0)) {
					tsp_uart_k230_send_string("$SWITCH#\n");
					toggle_cnt++;
					tsp_tft18_show_str_color(90, 0,
					    (uint8_t *)"SW", GREEN, BLACK);
					/* Show toggle count on title bar */
					{
						char tb[4];
						tb[0] = 'T'; tb[1] = ':';
						tb[2] = '0' + (toggle_cnt % 10);
						tb[3] = '\0';
						tsp_tft18_show_str_color(128, 0, (uint8_t *)tb, WHITE, BLACK);
					}
				}
				/* S1: Show current detected function ID on title bar */
				if (tsp_key_pressed(KEY_S1)) {
					tsp_tft18_show_str_color(90, 0,
					    (uint8_t *)"FN", YELLOW, BLACK);
				}
				delay_1ms(1);
			}
		}

		tsp_k230_task();

		if (tsp_k230_get_target(&tgt)) {
			/* Map K230 640x480 -> LCD canvas 160x80 (y=16..95):
			 *   X: /4 (0..639 -> 0..159),  Y: /6+16 (0..479 -> 16..95) */
			lcd_x = tgt.x / 4;
			lcd_y = 16 + tgt.y / 6;
			lcd_w = tgt.w / 4;
			lcd_h = tgt.h / 6;

			/* Clamp to canvas (title y=0..15, bottom divider y=104, x <= 159) */
			if (lcd_w < 2) lcd_w = 2;
			if (lcd_h < 2) lcd_h = 2;
			if (lcd_x < 0) lcd_x = 0;
			if (lcd_x + lcd_w > 159) lcd_x = 159 - lcd_w;
			if (lcd_x < 0) lcd_x = 0;

			/* Protect title row: push top edge below row 0 (16 px) */
			if (lcd_y < 16) {
				lcd_h -= (16 - lcd_y);
				lcd_y = 16;
			}
			if (lcd_y + lcd_h > 95) lcd_y = 95 - lcd_h;
			if (lcd_y < 16) lcd_y = 16;
			if (lcd_h < 2) lcd_h = 2;

			/* Redraw rect only when position/size changes (prevents flicker) */
			if (!has_old || lcd_x != old_x || lcd_y != old_y ||
			    lcd_w != old_w || lcd_h != old_h) {

				if (has_old) {
					/* Erase old rect at previous position */
					tsp_tft18_draw_frame((uint8_t)old_x, (uint8_t)old_y,
					                     (uint8_t)old_w, (uint8_t)old_h, BLACK);
				}

				/* Draw new rect at current position */
				tsp_tft18_draw_frame((uint8_t)lcd_x, (uint8_t)lcd_y,
				                     (uint8_t)lcd_w, (uint8_t)lcd_h, GREEN);

				old_x = lcd_x; old_y = lcd_y;
				old_w = lcd_w; old_h = lcd_h;
				has_old = 1;
			}

			/* Update X/Y display (row 6) only when values change */
			if (!showed_x || tgt.x != last_disp_x || tgt.y != last_disp_y) {
				if (!showed_x) {
					/* First frame: clear "Waiting" hint */
					tsp_tft18_show_str_color(0, 6,
					    (uint8_t *)"                    ", WHITE, BLACK);
					showed_x = 1;
				}
				/* uint16 (5 digits, 40 px) -- fits within allocated space */
				tsp_tft18_show_str_color(0, 6, (uint8_t *)"X:", WHITE, BLACK);
				{
					int16_t disp_x = tgt.x / 4;
					if (disp_x < 0) disp_x = 0;
					tsp_tft18_show_uint16(16, 6, (uint16_t)disp_x);
				}
				tsp_tft18_show_str_color(56, 6, (uint8_t *)" Y:", WHITE, BLACK);
				tsp_tft18_show_uint16(80, 6, (uint16_t)tgt.y);
				last_disp_x = tgt.x;
				last_disp_y = tgt.y;
			}
		}

		/* Refresh F/E counters (row 7) only when values change */
		{
			uint32_t fc = tsp_k230_frame_count();
			uint32_t ec = tsp_k230_error_count();

			if (fc != last_fc || ec != last_ec) {
				char buf[21];
				uint8_t p = 0;
				uint16_t fc4 = (uint16_t)(fc % 10000);
				uint16_t ec4 = (uint16_t)(ec % 10000);

				buf[p++] = 'F'; buf[p++] = ':';
				buf[p++] = '0' + (fc4 / 1000) % 10;
				buf[p++] = '0' + (fc4 / 100) % 10;
				buf[p++] = '0' + (fc4 / 10) % 10;
				buf[p++] = '0' + (fc4 % 10);
				buf[p++] = ' ';
				buf[p++] = 'E'; buf[p++] = ':';
				buf[p++] = '0' + (ec4 / 1000) % 10;
				buf[p++] = '0' + (ec4 / 100) % 10;
				buf[p++] = '0' + (ec4 / 10) % 10;
				buf[p++] = '0' + (ec4 % 10);
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';

				tsp_tft18_show_str_color(0, 7, (uint8_t *)buf, WHITE, BLACK);
				last_fc = fc;
				last_ec = ec;
			}
		}

	}

exit_k230:
	tsp_uart_k230_rx_disable();
	tsp_menu_request_redraw();
}

/* ===== CCD Test (128-pixel Linear CCD with LCD waveform + AD2 debugging) ===== */

static void action_ccd_test(void)
{
	/* Waveform drawing area: 128px wide, 63px tall (y=49..111) */
#define CCD_WF_X     0
#define CCD_WF_Y0    49U
#define CCD_WF_H     63U
#define CCD_WF_Y1    (CCD_WF_Y0 + CCD_WF_H - 1)   /* 111 */

	ccd_data_t ccd_raw;
	uint8_t    ccd_ch    = CCD1;
	uint8_t    exp_ms    = 10;
	uint8_t    redraw    = 1;
	uint16_t   max_v = 0, min_v = 0;
	uint32_t   sum_v = 0;
	uint8_t    i;
	uint8_t    tick      = 0;
	uint8_t    cont_mode = 1;   /* 1=continuous capture, 0=single-shot */
	uint8_t    captured  = 0;

	tsp_ccd_init();
	tsp_ccd_set_exposure(exp_ms);

	/* Clear previous frame buffer */
	for (i = 0; i < CCD_PIXELS; i++)
		ccd_raw[i] = 0;

	/* Draw static UI */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"CCD Test 128px", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);

	/* Waveform bounding box */
	tsp_tft18_draw_frame(CCD_WF_X, CCD_WF_Y0 - 1,
		CCD_PIXELS, CCD_WF_H + 1, DARKGREY);

	/* Bottom hints */
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:Exp S2:Ch", GRAY1, BLACK);
	tsp_tft18_show_str_color(128, 7, (uint8_t *)"PUSH", GRAY1, BLACK);

	tsp_encoder_enable();

	while (1) {
		tsp_key_scan();

		/* Key handling (capture edge-triggered states first) */
		{
			uint8_t k_s0 = tsp_key_pressed(KEY_S0);
			uint8_t k_s1 = tsp_key_pressed(KEY_S1);
			uint8_t k_s2 = tsp_key_pressed(KEY_S2);

			if (tsp_key_pressed(KEY_PUSH)) goto exit_ccd;

			/* Encoder: toggle continuous / single-shot */
			{
				int32_t enc = tsp_encoder_get_count();
				if (enc != 0) {
					cont_mode = !cont_mode;
					redraw = 1;
					tsp_encoder_reset();
				}
			}

			/* S0: exposure -1ms (min 1ms) */
			if (k_s0) { if (exp_ms > 1) { exp_ms--; tsp_ccd_set_exposure(exp_ms); redraw = 1; } }
			/* S1: exposure +1ms (max 100ms) */
			if (k_s1) { if (exp_ms < 100) { exp_ms++; tsp_ccd_set_exposure(exp_ms); redraw = 1; } }
			/* S2: toggle CCD channel (1<->2) */
			if (k_s2) {
				ccd_ch = (ccd_ch == CCD1) ? CCD2 : CCD1;
				redraw = 1;
			}

			/* Capture:
			 * CONT mode: capture every loop for live waveform.
			 * SNGL mode: capture once on S0/S1/S2 key press only. */
			captured = 0;
			if (cont_mode) {
				tsp_ccd_snapshot(ccd_ch, ccd_raw);
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) goto exit_ccd;
				captured = 1;
			} else if (k_s0 || k_s1 || k_s2) {
				tsp_ccd_snapshot(ccd_ch, ccd_raw);
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) goto exit_ccd;
				captured = 1;
			}
		}

		/* Compute statistics if we have fresh data */
		if (captured) {
			max_v = 0;
			min_v = 4095;
			sum_v = 0;
			for (i = 0; i < CCD_PIXELS; i++) {
				if (ccd_raw[i] > max_v) max_v = ccd_raw[i];
				if (ccd_raw[i] < min_v) min_v = ccd_raw[i];
				sum_v += ccd_raw[i];
			}
		}

		/* Incremental LCD text update */
		if (redraw) {
			char buf[21];
			uint8_t p = 0;

			/* Row 1: channel, exposure */
			buf[p++] = 'C'; buf[p++] = 'H'; buf[p++] = (ccd_ch == CCD1) ? '1' : '2';
			buf[p++] = ' '; buf[p++] = 'E'; buf[p++] = ':';
			if (exp_ms >= 100) { buf[p++] = '1'; buf[p++] = '0'; buf[p++] = '0'; }
			else if (exp_ms >= 10) { buf[p++] = '0' + (exp_ms/10); buf[p++] = '0' + (exp_ms%10); }
			else { buf[p++] = ' '; buf[p++] = '0' + exp_ms; }
			buf[p++] = 'm'; buf[p++] = 's';
			while (p < 16) buf[p++] = ' ';
			buf[16] = '\0';
			tsp_tft18_show_str_color(0, 1, (uint8_t *)buf, CYAN, BLACK);

			if (cont_mode)
				tsp_tft18_show_str_color(128, 1, (uint8_t *)"CONT", GREEN, BLACK);
			else
				tsp_tft18_show_str_color(128, 1, (uint8_t *)"SNGL", YELLOW, BLACK);

			/* Row 2: max, min, avg */
			p = 0;
			buf[p++] = 'M'; buf[p++] = ':';
			buf[p++] = '0' + (max_v / 1000) % 10;
			buf[p++] = '0' + (max_v / 100) % 10;
			buf[p++] = '0' + (max_v / 10) % 10;
			buf[p++] = '0' + (max_v % 10);
			buf[p++] = ' ';
			buf[p++] = 'm'; buf[p++] = ':';
			buf[p++] = '0' + (min_v / 1000) % 10;
			buf[p++] = '0' + (min_v / 100) % 10;
			buf[p++] = '0' + (min_v / 10) % 10;
			buf[p++] = '0' + (min_v % 10);
			buf[p++] = ' ';
			buf[p++] = 'A'; buf[p++] = ':';
			{
				uint16_t avg = (uint16_t)(sum_v / CCD_PIXELS);
				buf[p++] = '0' + (avg / 1000) % 10;
				buf[p++] = '0' + (avg / 100) % 10;
				buf[p++] = '0' + (avg / 10) % 10;
				buf[p++] = '0' + (avg % 10);
			}
			buf[p++] = ' ';
			buf[p] = '\0';
			tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);

			redraw = 0;
		}

		/* Draw waveform: batch-clear area then draw connected segments */
		if (captured) {
			uint8_t prev_y;
			uint16_t n;

			/* Fast clear: set_region once, stream BLACK pixels */
			tsp_tft18_set_region(CCD_WF_X, CCD_WF_Y0,
				CCD_WF_X + CCD_PIXELS - 1, CCD_WF_Y1);
			for (n = 0; n < (uint16_t)CCD_PIXELS * CCD_WF_H; n++)
				tsp_tft18_write_2byte(BLACK);

			/* Draw connected waveform on clean background */
			prev_y = 0;
			for (i = 0; i < CCD_PIXELS; i++) {
				uint8_t y_new = CCD_WF_Y1 -
					(uint8_t)(((uint32_t)ccd_raw[i] * CCD_WF_H) / 4096U);
				if (y_new < CCD_WF_Y0) y_new = CCD_WF_Y0;
				if (y_new > CCD_WF_Y1) y_new = CCD_WF_Y1;

				if (i == 0) {
					tsp_tft18_draw_pixel(CCD_WF_X, y_new, CYAN);
				} else {
					uint8_t ya = (prev_y < y_new) ? prev_y : y_new;
					uint8_t yb = (prev_y > y_new) ? prev_y : y_new;
					tsp_tft18_set_region(CCD_WF_X + i, ya,
						CCD_WF_X + i, yb);
					{
						uint8_t y;
						for (y = ya; y <= yb; y++)
							tsp_tft18_write_2byte(CYAN);
					}
				}
				prev_y = y_new;
			}
		}

		/* Periodic: refresh status info every ~500ms */
		tick++;
		if (tick >= 50) {
			tick = 0;
			redraw = 1;
		}

		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_ccd;

		delay_1ms(10);
	}

exit_ccd:
	tsp_encoder_disable();
	tsp_menu_request_redraw();
}

/* ===== MPU6050 IMU Test (6-axis raw data + Yaw angle) ===== */

static void action_mpu6050_test(void)
{
	mpu6050_raw_t  mpu;
	uint8_t  page     = 0;   /* 0=raw data, 1=yaw */
	uint8_t  redraw   = 1;
	uint8_t  tick     = 0;
	uint16_t i2c_err  = 0;
	int8_t   init_ok;

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"MPU6050 Test", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);

	/* Init MPU6050 */
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"Init MPU6050...", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Keep board still!", YELLOW, BLACK);
	init_ok = tsp_mpu6050_init();

	if (init_ok != 0) {
		uint8_t id = tsp_mpu6050_who_am_i();
		tsp_tft18_show_str_color(0, 3, (uint8_t *)"FAIL! WHO_AM_I:", RED, BLACK);
		tsp_tft18_show_uint16(0, 4, (uint16_t)id);
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"Check J16 cable", WHITE, BLACK);
		tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", GRAY1, BLACK);
		while (1) {
			tsp_key_scan();
			if (tsp_key_pressed(KEY_PUSH)) goto exit_mpu;
			delay_1ms(10);
		}
	}

	tsp_tft18_show_str_color(0, 2, (uint8_t *)"WHO_AM_I: 0x68 OK  ", GREEN, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"GZ offset:", WHITE, BLACK);
	tsp_tft18_show_int16(80, 3, tsp_mpu6050_get_gz_offset());
	delay_1ms(800);

	tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0:Yaw=0 S2:Page", GRAY1, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH:exit       ", GRAY1, BLACK);

	tsp_mpu6050_reset_yaw();
	tsp_mpu6050_yaw_enable();

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_mpu;

		if (tsp_key_pressed(KEY_S0)) {
			tsp_mpu6050_reset_yaw();
			redraw = 1;
		}
		if (tsp_key_pressed(KEY_S2)) {
			page = !page;
			tsp_tft18_clear(BLACK);
			tsp_tft18_show_str_color(0, 0, (uint8_t *)"MPU6050 Test", YELLOW, BLUE);
			tsp_tft18_draw_line_h(0, 16, 160, BLUE);
			tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0:Yaw=0 S2:Page", GRAY1, BLACK);
			tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH:exit       ", GRAY1, BLACK);
			redraw = 1;
		}

		tsp_mpu6050_update_yaw();

		tick++;
		if (tick >= 10) {
			tick = 0;
			redraw = 1;
		}

		if (redraw) {
			if (tsp_mpu6050_read_raw(&mpu) == 0) {
				if (page == 0) {

					tsp_tft18_show_str_color(0, 2, (uint8_t *)"AX:", CYAN, BLACK);
					tsp_tft18_show_int16(24, 2, mpu.ax);
					tsp_tft18_show_str_color(80, 2, (uint8_t *)"AY:", CYAN, BLACK);
					tsp_tft18_show_int16(104, 2, mpu.ay);

					tsp_tft18_show_str_color(0, 3, (uint8_t *)"AZ:", CYAN, BLACK);
					tsp_tft18_show_int16(24, 3, mpu.az);
					tsp_tft18_show_str_color(80, 3, (uint8_t *)"T :", CYAN, BLACK);
					tsp_tft18_show_int16(104, 3, mpu.temp);

					tsp_tft18_show_str_color(0, 4, (uint8_t *)"GX:", GREEN, BLACK);
					tsp_tft18_show_int16(24, 4, mpu.gx);
					tsp_tft18_show_str_color(80, 4, (uint8_t *)"GY:", GREEN, BLACK);
					tsp_tft18_show_int16(104, 4, mpu.gy);

					tsp_tft18_show_str_color(0, 5, (uint8_t *)"GZ:", GREEN, BLACK);
					tsp_tft18_show_int16(24, 5, mpu.gz);
				} else {
					/* Yaw page */
					float yaw = tsp_mpu6050_get_yaw();
					float yaw_mod = fmodf(yaw, 360.0f);
					if (yaw_mod < 0.0f) yaw_mod += 360.0f;
					int16_t yaw_i = (int16_t)yaw_mod;

					tsp_tft18_show_str_color(0, 1, (uint8_t *)"[Yaw Heading]   ", WHITE, BLACK);

					tsp_tft18_show_str_color(0, 2, (uint8_t *)"Yaw:", CYAN, BLACK);
					tsp_tft18_show_int16(32, 2, yaw_i);
					tsp_tft18_show_str_color(80, 2, (uint8_t *)"deg ", CYAN, BLACK);

					tsp_tft18_show_str_color(0, 3, (uint8_t *)"GZ:", GREEN, BLACK);
					tsp_tft18_show_int16(24, 3, mpu.gz);

					tsp_tft18_show_str_color(0, 4, (uint8_t *)"Ofs:", GRAY1, BLACK);
					tsp_tft18_show_int16(32, 4, tsp_mpu6050_get_gz_offset());

					tsp_tft18_show_str_color(0, 5, (uint8_t *)"S0 to reset Yaw ", YELLOW, BLACK);
				}
			} else {
				i2c_err++;
				tsp_tft18_show_str_color(120, 0, (uint8_t *)"E", RED, BLUE);
				tsp_tft18_show_uint16(128, 0, i2c_err);
			}
			redraw = 0;
		}

		delay_1ms(10);
	}

exit_mpu:
	tsp_mpu6050_yaw_disable();
	tsp_menu_request_redraw();
}

/* ===== Motor Open-Loop Test (encoder-based duty adjust, distributed keys) ===== */

static void action_motor_openloop(void)
{
	uint8_t  motor      = MOTOR1;
	uint8_t  dir[2]     = {MOTOR_FORWARD, MOTOR_FORWARD};
	uint16_t duty[2]    = {0, 0};
	uint8_t  redraw     = 1;
	uint8_t  tick       = 0;

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"Motor OpenLoop", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"PWM: 20kHz (50us)  ", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0:-5% S1:+5% Dir:S2", GRAY1, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"Enc:M1/M2 PUSH:exit", GRAY1, BLACK);

	tsp_motor_init();
	/* Push a known-stopped state to hardware before enabling the H-bridge --
	 * otherwise CC/DIR keep whatever they held on entry and the motor spins. */
	tsp_motor_stop_all();
	SLEEP_HIGH();
	tsp_encoder_enable();

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_openloop;

		/* Encoder: switch motor -- left=CCW=M2, right=CW=M1 */
		{
			int32_t enc = tsp_encoder_get_count();
			if (enc != 0) {
				uint8_t new_m = (enc < 0) ? MOTOR2 : MOTOR1;
				if (new_m != motor) { motor = new_m; redraw = 1; }
				tsp_encoder_reset();
			}
		}

		/* S2: toggle direction for selected motor */
		if (tsp_key_pressed(KEY_S2)) {
			dir[motor] = (dir[motor] == MOTOR_FORWARD) ? MOTOR_BACKWARD : MOTOR_FORWARD;
			tsp_motor_set(motor, dir[motor], duty[motor]);
			redraw = 1;
		}

		/* S0: duty -5 for selected motor */
		if (tsp_key_pressed(KEY_S0)) {
			if (duty[motor] >= 5) duty[motor] -= 5; else duty[motor] = 0;
			tsp_motor_set(motor, dir[motor], duty[motor]);
			redraw = 1;
		}
		/* S1: duty +5 for selected motor */
		if (tsp_key_pressed(KEY_S1)) {
			if (duty[motor] <= 95) duty[motor] += 5; else duty[motor] = 100;
			tsp_motor_set(motor, dir[motor], duty[motor]);
			redraw = 1;
		}

		/* Incremental LCD update */
		if (redraw) {
			char buf[21];
			uint8_t p = 0, i;

			buf[p++] = 'M'; buf[p++] = (motor == MOTOR1) ? '1' : '2';
			buf[p++] = ':'; buf[p++] = ' ';
			if (dir[motor] == MOTOR_FORWARD) {
				buf[p++] = 'F'; buf[p++] = 'W'; buf[p++] = 'D';
			} else {
				buf[p++] = 'R'; buf[p++] = 'E'; buf[p++] = 'V';
			}
			buf[p++] = ' ';
			if (duty[motor] >= 100) { buf[p++] = '1'; buf[p++] = '0'; buf[p++] = '0'; }
			else if (duty[motor] >= 10) { buf[p++] = '0' + (duty[motor]/10)%10; buf[p++] = '0' + duty[motor]%10; }
			else { buf[p++] = ' '; buf[p++] = '0' + duty[motor]%10; }
			buf[p++] = '%';
			while (p < 20) buf[p++] = ' ';
			buf[20] = '\0';
			tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);

			/* Duty bar (row 3) */
			{
				uint8_t bar_fill = (uint8_t)(duty[motor] / 10);
				buf[0] = 'D'; buf[1] = 'u'; buf[2] = 't'; buf[3] = 'y';
				buf[4] = ':'; buf[5] = '[';
				for (i = 0; i < 10; i++) {
					buf[6+i] = (i < bar_fill) ? '#' : ' ';
				}
				buf[16] = ']'; buf[17] = '\0';
				tsp_tft18_show_str_color(0, 3, (uint8_t *)buf, WHITE, BLACK);
			}
			redraw = 0;
		}

		/* nFAULT: poll every ~200ms */
		tick++;
		if (tick >= 20) {
			tick = 0;
			if (tsp_motor_fault())
				tsp_tft18_show_str_color(0, 5, (uint8_t *)"nFAULT: FAULT!      ", RED, BLACK);
			else
				tsp_tft18_show_str_color(0, 5, (uint8_t *)"nFAULT: OK          ", GREEN, BLACK);
		}

		delay_1ms(10);
	}

exit_openloop:
	tsp_encoder_disable();
	tsp_motor_stop_all();
	SLEEP_LOW();
	tsp_menu_request_redraw();
}

/* ===== Motor Closed-Loop PID Speed Control (incremental PID, D-on-PV) ===== */
/* Ported from HSP Ex6_pwm.c hsp_demo_motor_closeloop() */

static void action_motor_closeloop(void)
{
	uint8_t  motor_run   = 0;     /* 0=stopped, 1=running, S2 to toggle */
	uint16_t target_speed = 200;  /* target encoder pulses/20ms */
	int16_t  current_speed;
	int16_t  dc          = 0;     /* PWM duty cycle output (signed for bidirectional) */
	uint16_t tick        = 0;
	char     buf[32];

	/* PID controller (incremental, D-on-PV) */
	pid_inc_t spd_pid;
	tsp_pid_inc_init(&spd_pid, 0.2f, 0.02f, 0.06f, 0.0f, 99.0f);

	/* Init subsystems */
	tsp_motor_init();
	/* Push a known-stopped state to hardware before enabling the H-bridge --
	 * otherwise CC/DIR keep whatever they held on entry and the motor spins. */
	tsp_motor_stop_all();
	SLEEP_HIGH();
	tsp_encoder_enable();
	tsp_wheel_enc_start();

	/* Draw UI */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"Motor ClsLoop PID", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);

	tsp_tft18_show_str_color(0, 2, (uint8_t *)"TgtSpd:", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"FdbkSpd:", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Duty:", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Status:", CYAN, BLACK);

	tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:Spd S2:Run/Stp", BLACK, GREEN);

	/* Initial display */
	tsp_tft18_show_uint16(72, 2, target_speed);
	tsp_tft18_show_int16(72, 3, 0);
	tsp_tft18_show_uint16(72, 4, 0);
	tsp_tft18_show_str_color(72, 5, (uint8_t *)"STOP", BLACK, GREEN);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_closedloop;

		/* S2: Toggle run/stop */
		if (tsp_key_pressed(KEY_S2)) {
			motor_run = !motor_run;
			if (!motor_run) {
				dc = 0;
				tsp_pid_inc_reset(&spd_pid);
				tsp_motor_stop_all();
				tsp_tft18_show_str_color(72, 5, (uint8_t *)"STOP", BLACK, GREEN);
				tsp_tft18_show_uint16(72, 4, 0);
			} else {
				tsp_tft18_show_str_color(72, 5, (uint8_t *)"RUN ", BLACK, YELLOW);
			}
		}

		/* S0: target speed -10 */
		if (tsp_key_pressed(KEY_S0)) {
			if (target_speed >= 10) target_speed -= 10;
			else target_speed = 0;
			tsp_tft18_show_uint16(72, 2, target_speed);
		}
		/* S1: target speed +10 */
		if (tsp_key_pressed(KEY_S1)) {
			if (target_speed <= 490) target_speed += 10;
			else target_speed = 500;
			tsp_tft18_show_uint16(72, 2, target_speed);
		}

		/* Encoder: fine speed adjust ±1 (distributed from S0/S1) */
		{
			int32_t enc = tsp_encoder_get_count();
			if (enc > 0) {
				if (target_speed < 500) target_speed++;
				tsp_tft18_show_uint16(72, 2, target_speed);
				tsp_encoder_reset();
			} else if (enc < 0) {
				if (target_speed > 0) target_speed--;
				tsp_tft18_show_uint16(72, 2, target_speed);
				tsp_encoder_reset();
			}
		}

		/* PID control: apply PWM every loop */
		if (motor_run) {
			current_speed = tsp_wheel_enc_speed(MOTOR1);
			dc = (int16_t)tsp_pid_inc_step(&spd_pid,
			                                (float)target_speed,
			                                (float)current_speed,
			                                1.0f);

			/* Dead-zone: force output to 0 when target is 0 */
			if (target_speed == 0) {
				dc = 0;
				tsp_pid_inc_reset(&spd_pid);
			}

			/* Apply to motors */
			if (dc > 0) {
				tsp_motor_set(MOTOR1, MOTOR_FORWARD, (uint16_t)dc);
				tsp_motor_set(MOTOR2, MOTOR_FORWARD, (uint16_t)dc);
			} else if (dc < 0) {
				tsp_motor_set(MOTOR1, MOTOR_BACKWARD, (uint16_t)(-dc));
				tsp_motor_set(MOTOR2, MOTOR_BACKWARD, (uint16_t)(-dc));
			} else {
				tsp_motor_stop_all();
			}
		}

		/* Periodic display update (~100ms) */
		tick++;
		if (tick >= 10) {
			tick = 0;
			current_speed = tsp_wheel_enc_speed(MOTOR1);
			tsp_tft18_show_int16(72, 3, current_speed);
			if (motor_run) tsp_tft18_show_uint16(72, 4, (uint16_t)(dc >= 0 ? dc : -dc));
			/* Kp/Ki/Kd display — use snprintf to prevent overflow */
			snprintf(buf, sizeof(buf), "P%.1f I%.2f D%.2f", spd_pid.kp, spd_pid.ki, spd_pid.kd);
			buf[20] = '\0';
			tsp_tft18_show_str_color(0, 6, (uint8_t *)buf, GRAY1, BLACK);
		}

		delay_1ms(10);
	}

exit_closedloop:
	tsp_motor_stop_all();
	SLEEP_LOW();
	tsp_wheel_enc_stop();
	tsp_encoder_disable();
	tsp_menu_request_redraw();
}

/* ===== Odometer (里程计) — encoder-based distance/angle measurement ===== */
/* Ported from HSP Odometer.c */

static void action_odometer(void)
{
	tsp_encoder_enable();
	tsp_wheel_enc_start();
	tsp_odometer_demo();
	tsp_wheel_enc_stop();
	tsp_encoder_disable();
	tsp_menu_request_redraw();
}

/* ===== Line Following (巡线) — CCD-based PD steering + speed control ===== */
/* Ported from HSP Project1_LineFollower.c */

static void action_linefollow(void)
{
	tsp_linefollow_demo();
	tsp_menu_request_redraw();
}

/* ===== Speed/PID Parameter Setting (encoder-based parameter tuning) ===== */
/* Ported from HSP project_para_set() / hsp_speed_setting() */

static void action_speed_setting(void)
{
	uint8_t  item       = 1;       /* 1=Kp_steer, 2=Kd_steer, 3=Kp_spd, 4=Kd_spd, 5=BaseSpd */
	uint8_t  redraw     = 1;
	float    kp_s, kd_s, kp_v, kd_v;
	uint8_t  base_spd;
	char     buf[21];
	int32_t  enc;

	/* Read current values */
	tsp_linefollow_get_steer_gains(&kp_s, &kd_s);
	tsp_linefollow_get_speed_gains(&kp_v, &kd_v);
	base_spd = tsp_linefollow_get_speed_val();

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"PID Parameter Set", RED, YELLOW);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);

	tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:item Enc:val  ", BLACK, GREEN);

	tsp_encoder_enable();

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_param;

		/* S0: Previous item */
		if (tsp_key_pressed(KEY_S0)) {
			if (item > 1) item--;
			redraw = 1;
		}
		/* S1: Next item */
		if (tsp_key_pressed(KEY_S1)) {
			if (item < 5) item++;
			redraw = 1;
		}

		/* Encoder: Adjust selected parameter (fine control) */
		enc = tsp_encoder_get_count();
		if (enc != 0) {
			float step  = 0.1f;
			float *pval = &kp_s;
			float vmin  = 0.0f;
			float vmax  = 15.0f;

			switch (item) {
			case 1:  pval = &kp_s;  step = 0.1f;  vmin = 0.0f;  vmax = 9.0f;   break;
			case 2:  pval = &kd_s;  step = 0.5f;  vmin = 0.0f;  vmax = 15.0f;  break;
			case 3:  pval = &kp_v;  step = 0.1f;  vmin = 0.0f;  vmax = 5.0f;   break;
			case 4:  pval = &kd_v;  step = 0.01f; vmin = 0.0f;  vmax = 2.0f;   break;
			case 5:
				if (enc > 0)      { if (base_spd < 100) base_spd += 5; }
				else if (enc < 0) { if (base_spd >= 5)  base_spd -= 5; }
				redraw = 1;
				tsp_encoder_reset();
				continue;
			}

			if (enc > 0)       { *pval += step; if (*pval > vmax) *pval = vmax; }
			else if (enc < 0)  { *pval -= step; if (*pval < vmin) *pval = vmin; }
			redraw = 1;
			tsp_encoder_reset();
		}

		/* Update LCD (incremental) */
		if (redraw) {
			/* Clear all cursor markers */
			tsp_tft18_show_str_color(0, 2, (uint8_t *)"  ", WHITE, BLACK);
			tsp_tft18_show_str_color(0, 3, (uint8_t *)"  ", WHITE, BLACK);
			tsp_tft18_show_str_color(0, 4, (uint8_t *)"  ", WHITE, BLACK);
			tsp_tft18_show_str_color(0, 5, (uint8_t *)"  ", WHITE, BLACK);
			tsp_tft18_show_str_color(0, 6, (uint8_t *)"  ", WHITE, BLACK);

			/* Show cursor (->) for selected item */
			tsp_tft18_show_str_color(0, item + 1, (uint8_t *)"->", WHITE, BLACK);

			/* Row 2: Kp_steer */
			snprintf(buf, sizeof(buf), "KpStr: %0.1f", kp_s);
			tsp_tft18_show_str_color(16, 2, (uint8_t *)buf, CYAN, BLACK);

			/* Row 3: Kd_steer */
			snprintf(buf, sizeof(buf), "KdStr: %0.1f", kd_s);
			tsp_tft18_show_str_color(16, 3, (uint8_t *)buf, CYAN, BLACK);

			/* Row 4: Kp_spd */
			snprintf(buf, sizeof(buf), "KpSpd: %0.2f", kp_v);
			tsp_tft18_show_str_color(16, 4, (uint8_t *)buf, CYAN, BLACK);

			/* Row 5: Kd_spd */
			snprintf(buf, sizeof(buf), "KdSpd: %0.3f", kd_v);
			tsp_tft18_show_str_color(16, 5, (uint8_t *)buf, CYAN, BLACK);

			/* Row 6: BaseSpeed */
			snprintf(buf, sizeof(buf), "Spd: %03d%%", base_spd);
			tsp_tft18_show_str_color(16, 6, (uint8_t *)buf, CYAN, BLACK);

			redraw = 0;
		}

		delay_1ms(10);
	}

exit_param:
	/* Save parameters back */
	tsp_linefollow_set_steer_gains(kp_s, kd_s);
	tsp_linefollow_set_speed_gains(kp_v, kd_v);
	tsp_linefollow_set_speed(base_spd);
	tsp_encoder_disable();
	tsp_menu_request_redraw();
}

/* ===== Main Menu ===== */

static tsp_menu_item_t main_menu[] = {
	{"K230 Test",       action_k230_test},
	{"CCD Test",        action_ccd_test},
	{"Motor OpenLoop",  action_motor_openloop},
	{"Motor ClsLoop",   action_motor_closeloop},
	{"Odometer",        action_odometer},
	{"Line Follow",     action_linefollow},
	{"MPU6050 Test",    action_mpu6050_test},
	{"Speed Setting",   action_speed_setting},
};

#define MAIN_MENU_COUNT  (sizeof(main_menu) / sizeof(main_menu[0]))

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Boot animation: color test + info display */
	boot_animation();

	/* Init encoder (uses SysConfig PHA0 interrupt) */
	tsp_encoder_init();

	/* Init UART6 for K230 vision module (J11, BUSCLK 80MHz, 115200 preset).
	 * RX interrupt stays off until K230 Test enables it on demand. */
	tsp_uart_k230_init();
	tsp_k230_init();

	tsp_key_init();
	tsp_menu_init("== Control 2026 ==", main_menu, MAIN_MENU_COUNT);

	while (1) {
		tsp_key_scan();
		tsp_menu_run();
		delay_1ms(10);
	}
}
