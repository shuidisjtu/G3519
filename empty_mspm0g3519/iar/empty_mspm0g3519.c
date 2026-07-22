#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_uart_k230.h"
#include "tsp_k230.h"
#include "tsp_ad5933.h"
#include "tsp_motor.h"
/* [ARCHIVED] DDS — PC2/PC3 conflict with CCD analog inputs. Keep tsp_dds.c/h. */
// #include "tsp_dds.h"
#include "tsp_ccd.h"
#include <math.h>

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;

/* ===== Boot Animation ===== */

static void boot_animation(void)
{
	/* Color test sequence (like teacher's tsp_tft18_test_color) */
	tsp_tft18_test_color();

	/* Show boot info */
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"NUEDC-2026 SAIS@SJTU", BLUE, YELLOW);
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

/* ===== K230 Vision Test (color tracking with LCD overlay) ===== */

static void action_k230_test(void)
{
	k230_target_t tgt;
	int16_t lcd_x, lcd_y, lcd_w, lcd_h;
	int16_t old_x = 0, old_y = 0, old_w = 0, old_h = 0;
	int16_t last_disp_x = -1, last_disp_y = -1;
	uint16_t last_fc = 0xFFFF, last_ec = 0xFFFF;
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
				if (tsp_key_pressed(KEY_S0)) {
					/* Send toggle command to K230 over UART6 TX */
					tsp_uart_k230_send_string("$SWITCH#\n");
					toggle_cnt++;
					/* Show toggle count on row 5 */
					{
						char tb[4];
						tb[0] = 'T'; tb[1] = ':';
						tb[2] = '0' + (toggle_cnt % 10);
						tb[3] = '\0';
						tsp_tft18_show_str_color(90, 0, (uint8_t *)tb, WHITE, BLACK);
					}
				}
				delay_1ms(1);
			}
		}

		tsp_k230_task();

		if (tsp_k230_get_target(&tgt)) {
			/* Map K230 640x480 -> LCD canvas 160x80 (y=16..95):
			 *   X: /4 (0..639 → 0..159),  Y: /6+16 (0..479 → 16..95) */
			lcd_x = tgt.x / 4;
			lcd_y = 16 + tgt.y / 6;
			lcd_w = tgt.w / 4;
			lcd_h = tgt.h / 6;

			/* Clamp to canvas (title y=0..15, bottom divider y=104, x <= 159) */
			if (lcd_x < 0) lcd_x = 0;
			if (lcd_x + lcd_w > 159) lcd_w = 159 - lcd_x;

			/* Protect title row: push top edge below row 0 (16 px) */
			if (lcd_y < 16) {
				lcd_h -= (16 - lcd_y);
				lcd_y = 16;
			}
			if (lcd_y + lcd_h > 95) lcd_h = 95 - lcd_y;
			if (lcd_w < 2) lcd_w = 2;
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
				/* uint16 (5 digits, 40 px) — fits within allocated space */
				tsp_tft18_show_str_color(0, 6, (uint8_t *)"X:", WHITE, BLACK);
				tsp_tft18_show_uint16(16, 6, (uint16_t)(tgt.x / 4));
				tsp_tft18_show_str_color(56, 6, (uint8_t *)" Y:", WHITE, BLACK);
				tsp_tft18_show_uint16(80, 6, (uint16_t)tgt.y);
				last_disp_x = tgt.x;
				last_disp_y = tgt.y;
			}
		}

		/* Refresh F/E counters (row 7) only when values change */
		{
			uint16_t fc = (uint16_t)tsp_k230_frame_count();
			uint16_t ec = (uint16_t)tsp_k230_error_count();

			if (fc != last_fc || ec != last_ec) {
				char buf[21];
				uint8_t p = 0;

				buf[p++] = 'F'; buf[p++] = ':';
				buf[p++] = '0' + (fc / 1000) % 10;
				buf[p++] = '0' + (fc / 100) % 10;
				buf[p++] = '0' + (fc / 10) % 10;
				buf[p++] = '0' + (fc % 10);
				buf[p++] = ' ';
				buf[p++] = 'E'; buf[p++] = ':';
				buf[p++] = '0' + (ec / 1000) % 10;
				buf[p++] = '0' + (ec / 100) % 10;
				buf[p++] = '0' + (ec / 10) % 10;
				buf[p++] = '0' + (ec % 10);
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

/* --- Interactive DDS Test [ARCHIVED] --- */
#if 0
static void action_dds_test(void)
{
	uint8_t i;
	uint8_t  wave_idx  = 0;          /* 0=Square, 1=Sine, 2=Triangle */
	uint32_t freq      = 1000;       /* start at 1 kHz */
	uint8_t  last_wave = 0xFF;
	uint32_t last_freq = 0;

	/* Draw static elements */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"AD9833 DDS Generator", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0/S1:Wave  Enc:Freq", GRAY1, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", GRAY1, BLACK);

	/* Start DDS (polled encoder — no ISR needed) */
	tsp_dds_set_output(freq, tsp_dds_wave_ctrl[wave_idx]);

	{
		uint8_t  last_a   = (PHA0() != 0) ? 1 : 0;
		int32_t  enc_acc  = 0;       /* accumulated encoder pulses */
		uint8_t  tick     = 0;       /* 1ms tick counter */

		while (1) {
			/* --- Poll encoder at 1ms rate (natural debounce) --- */
			{
				uint8_t a = (PHA0() != 0) ? 1 : 0;
				uint8_t b = (PHB0() != 0) ? 1 : 0;
				if (a != last_a) {
					last_a = a;
					if (a == b) enc_acc++;
					else        enc_acc--;
				}
			}

			tick++;

			/* --- Apply accumulated pulses every ~10ms --- */
			if (tick >= 10) {
				tick = 0;

				/* Keys */
				tsp_key_scan();
				if (tsp_key_pressed(KEY_PUSH)) break;

				if (tsp_key_pressed(KEY_S0)) {
					wave_idx = (wave_idx == 0) ? DDS_WAVE_COUNT - 1 : wave_idx - 1;
				}
				if (tsp_key_pressed(KEY_S1)) {
					wave_idx = (wave_idx == DDS_WAVE_COUNT - 1) ? 0 : wave_idx + 1;
				}

				/* Apply encoder accumulation */
				if (enc_acc != 0) {
					uint32_t step = tsp_dds_get_step(freq, enc_acc);
					int32_t  new_freq = (int32_t)freq + enc_acc * (int32_t)step;

					if      (new_freq < 100)    freq = 100;
					else if (new_freq > 50000) freq = 50000;
					else                       freq = (uint32_t)new_freq;

					enc_acc = 0;
				}
			}

			/* --- Reconfigure DDS if needed --- */
			if (wave_idx != last_wave || freq != last_freq) {
				tsp_dds_set_output(freq, tsp_dds_wave_ctrl[wave_idx]);
			}

			/* --- Incremental LCD update: waveform row --- */
			if (wave_idx != last_wave) {
				char buf[21];
				uint8_t p = 0;
				buf[p++] = 'W'; buf[p++] = 'a'; buf[p++] = 'v'; buf[p++] = 'e';
				buf[p++] = ':'; buf[p++] = ' ';
				for (i = 0; tsp_dds_wave_names[wave_idx][i]; i++)
					buf[p++] = (char)tsp_dds_wave_names[wave_idx][i];
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);
				tsp_tft18_show_str_color(0, 4, (uint8_t *)tsp_dds_vout_info[wave_idx],
				                         WHITE, BLACK);
				last_wave = wave_idx;
			}

			/* --- Incremental LCD update: frequency row --- */
			if (freq != last_freq) {
				char buf[21];
				uint8_t digits[7];
				uint8_t nd = 0, p = 0;
				uint32_t v = freq;

				if (v == 0) { digits[nd++] = 0; }
				else { while (v > 0) { digits[nd++] = (uint8_t)(v % 10); v /= 10; } }

				buf[p++] = 'F'; buf[p++] = 'r'; buf[p++] = 'e'; buf[p++] = 'q';
				buf[p++] = ':'; buf[p++] = ' ';
				for (i = nd; i < 7; i++) buf[p++] = ' ';
				for (i = nd; i > 0; i--)  buf[p++] = (char)('0' + digits[i - 1]);
				buf[p++] = ' '; buf[p++] = 'H'; buf[p++] = 'z';
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';

				tsp_tft18_show_str_color(0, 3, (uint8_t *)buf, WHITE, BLACK);
				last_freq = freq;
			}

			delay_1ms(1);
		}
	}

	/* Cleanup */
	tsp_dds_stop();
	tsp_menu_request_redraw();
}
#endif /* [ARCHIVED] DDS action_dds_test */

/* ===== AD5933 Impedance Test =====
 * Uses library API (tsp_ad5933_init/read_temperature/set_sweep/start_sweep)
 * to verify P2 (CTRL_H write), P3 (frequency code), P6 (temp mask) fixes.
 * LCD shows live Real/Imag/Magnitude for calibration with known resistor. */

/* Helper: format uint8_t as 2-char hex string (null-terminated) */
static void hex2(uint8_t val, char *out)
{
	out[0] = "0123456789ABCDEF"[(val >> 4) & 0xF];
	out[1] = "0123456789ABCDEF"[ val       & 0xF];
	out[2] = '\0';
}

static void action_ad5933_test(void)
{
	float temp;
	int16_t real, imag;
	float mag;
	float gain_factor = 0.0f;

	#define CAL_RESISTANCE  18000.0f

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"AD5933 Impedance", YELLOW, BLUE);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", GRAY1, BLACK);

	/* --- Init --- */
	tsp_ad5933_init();
	delay_1ms(10);

	/* --- Display CTRL_H/CTRL_L --- */
	{
		uint8_t vh = tsp_ad5933_read_reg(AD5933_REG_CTRL_H);
		uint8_t vl = tsp_ad5933_read_reg(AD5933_REG_CTRL_L);
		char hx[3]; char buf[21]; uint8_t p = 0;
		buf[p++] = 'C'; buf[p++] = ':';
		hex2(vh, hx); buf[p++] = hx[0]; buf[p++] = hx[1]; buf[p++] = ' ';
		hex2(vl, hx); buf[p++] = hx[0]; buf[p++] = hx[1];
		while (p < 20) buf[p++] = ' ';
		buf[20] = '\0';
		tsp_tft18_show_str_color(0, 1, (uint8_t *)buf, WHITE, BLACK);
	}

	/* --- Temperature --- */
	temp = tsp_ad5933_read_temperature();
	if (temp != temp) {
		tsp_tft18_show_str_color(0, 2,
			(uint8_t *)"T: --.-C (no resp)", YELLOW, BLACK);
	} else {
		char buf[21]; uint8_t p = 0;
		int16_t ti = (int16_t)temp;
		uint8_t td = (uint8_t)((temp - (float)ti) * 10.0f + 0.5f);
		buf[p++] = 'T'; buf[p++] = ':';
		if (ti < 0) { buf[p++] = '-'; ti = -ti; }
		if (ti >= 10) buf[p++] = '0' + (ti / 10) % 10;
		buf[p++] = '0' + (ti % 10);
		buf[p++] = '.'; buf[p++] = '0' + td; buf[p++] = 'C';
		while (p < 20) buf[p++] = ' ';
		buf[20] = '\0';
		tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, WHITE, BLACK);
	}

	/* ===== Calibration phase ===== */
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Cal: 18000 Ohm", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Connect Rcal to J15", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"S2=Calibrate", GRAY1, BLACK);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_ad5933;
		if (tsp_key_pressed(KEY_S2)) break;
		delay_1ms(10);
	}

	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Calibrating...      ", YELLOW, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"                    ", BLACK, BLACK);

	tsp_ad5933_set_sweep(1000, 0, 0, 100, AD5933_SETTLE_X1);
	tsp_ad5933_start_sweep();

	/* Wait for DFT data valid */
	{
		uint32_t timeout = 50000;
		while (!(tsp_ad5933_read_status() & AD5933_STATUS_DATA_VALID) && --timeout)
			;
	}

	{
		int16_t cal_real = tsp_ad5933_read_real();
		int16_t cal_imag = tsp_ad5933_read_imag();
		float cal_mag = sqrtf((float)cal_real * cal_real +
		                      (float)cal_imag * cal_imag);

		if (cal_mag > 1.0f) {
			gain_factor = 1.0f / (CAL_RESISTANCE * cal_mag);
		}
	}

	if (gain_factor > 0.0f) {
		tsp_tft18_show_str_color(0, 4, (uint8_t *)"Cal OK! Replace DUT ", GREEN, BLACK);
	} else {
		tsp_tft18_show_str_color(0, 4, (uint8_t *)"Cal FAIL (Mag=0)    ", RED, BLACK);
	}
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"S2=Start measuring  ", GRAY1, BLACK);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_ad5933;
		if (tsp_key_pressed(KEY_S2)) break;
		delay_1ms(10);
	}

	/* ===== Measurement phase ===== */
	/* Restart sweep for measurement */
	tsp_ad5933_start_sweep();

	/* Redraw static labels (pad to 20 chars to clear calibration text) */
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Freq: 1000 Hz       ", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Real:               ", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Imag:               ", CYAN, BLACK);
	if (gain_factor > 0.0f) {
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"Z   :               ", GREEN, BLACK);
	} else {
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"Mag :               ", GREEN, BLACK);
	}

	/* --- Live display loop --- */
	{
		uint8_t  temp_tick = 0;
		int16_t  last_temp_x10 = (int16_t)(temp * 10.0f + 0.5f);

		while (1) {
			/* Wait for DATA_VALID, polling keys every 10ms */
			{
				uint8_t wait;
				for (wait = 0; wait < 50; wait++) {
					delay_1ms(10);
					tsp_key_scan();
					if (tsp_key_pressed(KEY_PUSH)) goto exit_ad5933;
					if (tsp_ad5933_read_status() & AD5933_STATUS_DATA_VALID)
						break;
				}
				if (wait >= 50) continue;
			}

			/* Read DFT data */
			real = tsp_ad5933_read_real();
			imag = tsp_ad5933_read_imag();
			mag  = sqrtf((float)real * (float)real + (float)imag * (float)imag);

			/* Display Real */
			{
				char buf[14]; uint8_t p = 0;
				int16_t abs_v = (real < 0) ? -real : real;
				if (real < 0) buf[p++] = '-';
				buf[p++] = '0' + (abs_v / 10000) % 10;
				buf[p++] = '0' + (abs_v / 1000) % 10;
				buf[p++] = '0' + (abs_v / 100) % 10;
				buf[p++] = '0' + (abs_v / 10) % 10;
				buf[p++] = '0' + (abs_v % 10);
				buf[p] = '\0';
				tsp_tft18_show_str_color(40, 4, (uint8_t *)buf, WHITE, BLACK);
			}
			/* Display Imag */
			{
				char buf[14]; uint8_t p = 0;
				int16_t abs_v = (imag < 0) ? -imag : imag;
				if (imag < 0) buf[p++] = '-';
				buf[p++] = '0' + (abs_v / 10000) % 10;
				buf[p++] = '0' + (abs_v / 1000) % 10;
				buf[p++] = '0' + (abs_v / 100) % 10;
				buf[p++] = '0' + (abs_v / 10) % 10;
				buf[p++] = '0' + (abs_v % 10);
				buf[p] = '\0';
				tsp_tft18_show_str_color(40, 5, (uint8_t *)buf, WHITE, BLACK);
			}
			/* Display Z (Ohm) or raw Mag */
			{
				char buf[21]; uint8_t p = 0;
				if (gain_factor > 0.0f && mag > 1.0f) {
					float z_ohm = 1.0f / (gain_factor * mag);
					uint32_t z = (uint32_t)(z_ohm + 0.5f);
					if (z > 99999) z = 99999;
					buf[p++] = '0' + (z / 10000) % 10;
					buf[p++] = '0' + (z / 1000) % 10;
					buf[p++] = '0' + (z / 100) % 10;
					buf[p++] = '0' + (z / 10) % 10;
					buf[p++] = '0' + (z % 10);
					buf[p++] = ' '; buf[p++] = 'O'; buf[p++] = 'h';
					buf[p++] = 'm'; buf[p++] = ' ';
				} else {
					uint16_t m = (uint16_t)mag;
					buf[p++] = '0' + (m / 10000) % 10;
					buf[p++] = '0' + (m / 1000) % 10;
					buf[p++] = '0' + (m / 100) % 10;
					buf[p++] = '0' + (m / 10) % 10;
					buf[p++] = '0' + (m % 10);
					buf[p++] = ' '; buf[p++] = ' '; buf[p++] = ' ';
					buf[p++] = ' '; buf[p++] = ' ';
				}
				buf[p] = '\0';
				tsp_tft18_show_str_color(40, 6, (uint8_t *)buf, GREEN, BLACK);
			}

			/* Trigger next measurement at same frequency.
			 * REPEAT_FREQ re-runs DFT without re-initializing the DDS,
			 * per AD5933 datasheet Figure 27. */
			tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
				(uint8_t)((AD5933_CTRL_REPEAT_FREQ | AD5933_VOLT_2000MV | AD5933_PGA_X1) >> 8));
			tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);

			/* Periodic temperature refresh (~5s to reduce sweep interruption) */
			temp_tick++;
			if (temp_tick >= 20) {
				temp_tick = 0;
				temp = tsp_ad5933_read_temperature();
				if (temp == temp) {
					int16_t tx = (int16_t)(temp * 10.0f + 0.5f);
					if (tx != last_temp_x10) {
						char buf[21]; uint8_t p = 0;
						int16_t ti = (int16_t)temp;
						uint8_t td = (uint8_t)((temp - (float)ti) * 10.0f + 0.5f);
						buf[p++] = 'T'; buf[p++] = ':';
						if (ti < 0) { buf[p++] = '-'; ti = -ti; }
						if (ti >= 10) buf[p++] = '0' + (ti / 10) % 10;
						buf[p++] = '0' + (ti % 10);
						buf[p++] = '.'; buf[p++] = '0' + td; buf[p++] = 'C';
						while (p < 20) buf[p++] = ' ';
						buf[20] = '\0';
						tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, WHITE, BLACK);
						last_temp_x10 = tx;
					}
				}
			}
		}
	}

exit_ad5933:
	tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
		(uint8_t)(AD5933_CTRL_POWER_DOWN >> 8));
	tsp_menu_request_redraw();
}

/* ===== Motor Test (AD2-friendly interactive PWM control) ===== */

static void action_motor_test(void)
{
	uint8_t  motor      = MOTOR1;
	uint8_t  dir[2]     = {MOTOR_FORWARD, MOTOR_FORWARD};
	uint16_t duty[2]    = {0, 0};
	uint8_t  redraw     = 1;
	uint8_t  tick       = 0;

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"Motor Test TIMA0", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"PWM: 20kHz (50us)  ", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0/S1:Duty S2:Dir", GRAY1, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"M1/M2:Enc PUSH:exit", GRAY1, BLACK);

	tsp_motor_init();
	SLEEP_HIGH();
	tsp_encoder_enable();

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) goto exit_motor;

		/* Encoder: switch motor — left=M2, right=M1 */
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

		/* S0: duty -5, S1: duty +5 for selected motor */
		if (tsp_key_pressed(KEY_S0)) {
			if (duty[motor] >= 5) duty[motor] -= 5; else duty[motor] = 0;
			tsp_motor_set(motor, dir[motor], duty[motor]);
			redraw = 1;
		}
		if (tsp_key_pressed(KEY_S1)) {
			if (duty[motor] <= 95) duty[motor] += 5; else duty[motor] = 100;
			tsp_motor_set(motor, dir[motor], duty[motor]);
			redraw = 1;
		}

		/* Incremental LCD update */
		if (redraw) {
			char buf[21];
			uint8_t p = 0, i;
			char dname[4] = "FWD";

			if (dir[motor] == MOTOR_BACKWARD) { dname[0]='R'; dname[1]='E'; dname[2]='V'; }

			buf[p++] = 'M'; buf[p++] = (motor == MOTOR1) ? '1' : '2';
			buf[p++] = ':'; buf[p++] = ' ';
			buf[p++] = dname[0]; buf[p++] = dname[1]; buf[p++] = dname[2];
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

		/* nFAULT: poll every ~200ms, always refresh */
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

exit_motor:
	tsp_encoder_disable();
	tsp_motor_stop_all();
	SLEEP_LOW();
	tsp_menu_request_redraw();
}

/* ===== CCD Test (128-pixel Linear CCD with LCD waveform + AD2 debugging) ===== */

static void action_ccd_test(void)
{
	/* Waveform drawing area: 128px wide (x=0..127), 80px tall (y=32..111) */
#define CCD_WF_X     0
#define CCD_WF_Y0    32U
#define CCD_WF_H     80U
#define CCD_WF_Y1    (CCD_WF_Y0 + CCD_WF_H - 1)   /* 111 */

	ccd_data_t ccd_raw, ccd_prev;
	uint8_t    ccd_ch    = CCD1;
	uint8_t    exp_ms    = 10;
	uint8_t    redraw    = 1;
	uint16_t   max_v, min_v;
	uint32_t   sum_v;
	uint8_t    i;
	uint8_t    tick      = 0;
	uint8_t    cont_mode = 1;   /* 1=continuous capture, 0=single-shot */
	uint8_t    captured;        /* set when new data is read this loop */

	tsp_ccd_init();
	tsp_ccd_set_exposure(exp_ms);

	/* Clear previous frame buffer */
	for (i = 0; i < CCD_PIXELS; i++) {
		ccd_prev[i] = 0xFFFF;  /* impossible value → force first draw */
		ccd_raw[i]  = 0;
	}

	/* ─── Draw static UI ─── */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"CCD Test 128px", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);

	/* Waveform bounding box: left edge at x=0, pixels at x=0..127 inside.
	 * Frame width=128 covers all 128 CCD pixels, height=81 covers y=31..111. */
	tsp_tft18_draw_frame(CCD_WF_X, CCD_WF_Y0 - 1,
		CCD_PIXELS, CCD_WF_H + 1, DARKGREY);

	/* Bottom hints */
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:Exp S2:Ch", GRAY1, BLACK);
	tsp_tft18_show_str_color(108, 7, (uint8_t *)"PUSH:exit", GRAY1, BLACK);

	tsp_encoder_enable();

	while (1) {
		tsp_key_scan();

		/* ─── Key handling (capture edge-triggered states first) ─── */
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
					if (!cont_mode) {
						/* Switching to SNGL: invalidate prev to force full redraw on next capture */
						for (i = 0; i < CCD_PIXELS; i++) ccd_prev[i] = 0xFFFF;
					}
					tsp_encoder_reset();
				}
			}

			/* S0: exposure -1ms (min 1ms) */
			if (k_s0) { if (exp_ms > 1) { exp_ms--; tsp_ccd_set_exposure(exp_ms); redraw = 1; } }
			/* S1: exposure +1ms (max 100ms) */
			if (k_s1) { if (exp_ms < 100) { exp_ms++; tsp_ccd_set_exposure(exp_ms); redraw = 1; } }
			/* S2: toggle CCD channel (1↔2) */
			if (k_s2) {
				ccd_ch = (ccd_ch == CCD1) ? CCD2 : CCD1;
				for (i = 0; i < CCD_PIXELS; i++) ccd_prev[i] = 0xFFFF;
				redraw = 1;
			}

			/* ─── Capture ───
			 * CONT mode: capture every loop for live waveform.
			 * SNGL mode: capture once on S0/S1/S2 key press only. */
			captured = 0;
			if (cont_mode) {
				tsp_ccd_snapshot(ccd_ch, ccd_raw);
				captured = 1;
			} else if (k_s0 || k_s1 || k_s2) {
				/* Single-shot: user key triggers one capture */
				tsp_ccd_snapshot(ccd_ch, ccd_raw);
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

		/* ─── Incremental LCD text update ─── */
		if (redraw) {
			char buf[21];
			uint8_t p = 0;

			/* Row 1: channel, exposure, mode (C/S) */
			buf[p++] = 'C'; buf[p++] = 'H'; buf[p++] = (ccd_ch == CCD1) ? '1' : '2';
			buf[p++] = ' '; buf[p++] = 'E'; buf[p++] = ':';
			if (exp_ms >= 100) { buf[p++] = '1'; buf[p++] = '0'; buf[p++] = '0'; }
			else if (exp_ms >= 10) { buf[p++] = '0' + (exp_ms/10); buf[p++] = '0' + (exp_ms%10); }
			else { buf[p++] = ' '; buf[p++] = '0' + exp_ms; }
			buf[p++] = 'm'; buf[p++] = 's';
			buf[p++] = ' ';
			buf[p++] = (cont_mode) ? 'C' : 'S';  /* Continuous / Single */
			buf[p++] = ' '; buf[p++] = 'M'; buf[p++] = ':';
			buf[p++] = '0' + (max_v / 1000) % 10;
			buf[p++] = '0' + (max_v / 100) % 10;
			buf[p++] = '0' + (max_v / 10) % 10;
			buf[p++] = '0' + (max_v % 10);
			buf[p] = '\0';
			tsp_tft18_show_str_color(0, 1, (uint8_t *)buf, CYAN, BLACK);

			/* Row 2: min, avg */
			p = 0;
			buf[p++] = 'm'; buf[p++] = ':';
			buf[p++] = '0' + (min_v / 1000) % 10;
			buf[p++] = '0' + (min_v / 100) % 10;
			buf[p++] = '0' + (min_v / 10) % 10;
			buf[p++] = '0' + (min_v % 10);
			buf[p++] = ' '; buf[p++] = 'A'; buf[p++] = 'v'; buf[p++] = 'g'; buf[p++] = ':';
			{
				uint16_t avg = (uint16_t)(sum_v / CCD_PIXELS);
				buf[p++] = '0' + (avg / 1000) % 10;
				buf[p++] = '0' + (avg / 100) % 10;
				buf[p++] = '0' + (avg / 10) % 10;
				buf[p++] = '0' + (avg % 10);
			}
			while (p < 20) buf[p++] = ' ';
			buf[20] = '\0';
			tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);

			/* Mode indicator at row 2 right side */
			if (cont_mode)
				tsp_tft18_show_str_color(120, 2, (uint8_t *)"CONT", GREEN, BLACK);
			else
				tsp_tft18_show_str_color(120, 2, (uint8_t *)"SNGL", YELLOW, BLACK);

			redraw = 0;
		}

		/* ─── Draw waveform (only when new data captured) ─── */
		if (captured) {
			for (i = 0; i < CCD_PIXELS; i++) {
				uint8_t y_new = CCD_WF_Y1 -
					(uint8_t)(((uint32_t)ccd_raw[i] * CCD_WF_H) / 4096U);
				uint8_t y_old;

				if (ccd_prev[i] == 0xFFFF) {
					/* First draw after channel switch: no erasure needed */
					tsp_tft18_draw_pixel(CCD_WF_X + i, y_new, CYAN);
				} else {
					y_old = CCD_WF_Y1 -
						(uint8_t)(((uint32_t)ccd_prev[i] * CCD_WF_H) / 4096U);
					if (y_new != y_old) {
						/* Erase old pixel, draw new */
						tsp_tft18_draw_pixel(CCD_WF_X + i, y_old, BLACK);
						tsp_tft18_draw_pixel(CCD_WF_X + i, y_new, CYAN);
					}
					/* else: pixel at same position, no change needed */
				}
				ccd_prev[i] = ccd_raw[i];
			}
		}

		/* ─── Periodic: refresh status info every ~500ms ─── */
		tick++;
		if (tick >= 50) {
			tick = 0;
			/* Force info line refresh */
			redraw = 1;
		}

		delay_1ms(10);
	}

exit_ccd:
	tsp_encoder_disable();
	tsp_menu_request_redraw();
}

/* ===== Main Menu ===== */

static tsp_menu_item_t main_menu[] = {
	{"K230 Test",      action_k230_test},
	{"CCD Test",       action_ccd_test},
	{"AD5933 Test",    action_ad5933_test},
	{"Motor Test",     action_motor_test},
};

#define MAIN_MENU_COUNT  (sizeof(main_menu) / sizeof(main_menu[0]))

/* ===== Main ===== */

int main(void)
{
	SYSCFG_DL_init();

	tsp_tft18_init();

	/* Boot animation: color test + info display (like teacher's example) */
	boot_animation();

	/* Init encoder (uses SysConfig PHA0 interrupt) */
	tsp_encoder_init();

	/* Init UART6 for K230 vision module (J11, BUSCLK 80MHz, 115200 preset).
	 * RX interrupt stays off until K230 Test enables it on demand. */
	tsp_uart_k230_init();
	tsp_k230_init();

	tsp_key_init();
	tsp_menu_init("=== NUEDC 2026 ===", main_menu, MAIN_MENU_COUNT);

	while (1) {
		tsp_key_scan();
		tsp_menu_run();
		delay_1ms(10);
	}
}
