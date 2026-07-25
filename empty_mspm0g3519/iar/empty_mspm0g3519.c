#include "ti_msp_dl_config.h"
#include "tsp_isr.h"
#include "tsp_gpio.h"
#include "TSP_TFT18.h"
#include "tsp_key.h"
#include "tsp_menu.h"
#include "tsp_encoder.h"
#include "tsp_ad5933.h"
#include "tsp_dds.h"
#include "tsp_adc.h"
#include <math.h>

/* ===== Global state ===== */
extern volatile uint32_t sys_tick_counter;

/* ===== Boot Animation ===== */

static void boot_animation(void)
{
	/* Color test sequence (like teacher's tsp_tft18_test_color) */
	tsp_tft18_test_color();

	/* Show boot info */
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"NUEDC-2026 Signal", BLUE, YELLOW);
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

/* ===== Interactive DDS Test ===== */

static void action_dds_test(void)
{
	uint8_t i;
	uint8_t  wave_idx  = 0;          /* 0=Square, 1=Sine, 2=Triangle */
	uint32_t freq      = 1000;       /* start at 1 kHz */
	uint8_t  last_wave = 0xFF;
	uint32_t last_freq = 0;

	uint8_t  keep_output = 0;

	/* Draw static elements */
	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"AD9833 DDS Generator", YELLOW, BLUE);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"S0/S1:Wave  Enc:Freq", GRAY1, BLACK);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH:Exit  S2:Keep  ", GRAY1, BLACK);

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
				if (tsp_key_pressed(KEY_S2)) { keep_output = 1; break; }

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
	if (!keep_output)
		tsp_dds_stop();
	tsp_menu_request_redraw();
}

/* ===== ADC Voltage / Frequency Test ===== */

static void fmt_uint16(char *buf, uint8_t *p, uint16_t v, uint8_t digits)
{
	uint8_t d[5];
	uint8_t i;
	d[0] = (uint8_t)((v / 10000) % 10);
	d[1] = (uint8_t)((v / 1000) % 10);
	d[2] = (uint8_t)((v / 100) % 10);
	d[3] = (uint8_t)((v / 10) % 10);
	d[4] = (uint8_t)(v % 10);
	for (i = 5 - digits; i < 5; i++)
		buf[(*p)++] = (char)('0' + d[i]);
}

static void fmt_uint32(char *buf, uint8_t *p, uint32_t v, uint8_t digits)
{
	uint8_t d[7];
	uint8_t i;
	d[0] = (uint8_t)((v / 1000000) % 10);
	d[1] = (uint8_t)((v / 100000) % 10);
	d[2] = (uint8_t)((v / 10000) % 10);
	d[3] = (uint8_t)((v / 1000) % 10);
	d[4] = (uint8_t)((v / 100) % 10);
	d[5] = (uint8_t)((v / 10) % 10);
	d[6] = (uint8_t)(v % 10);
	for (i = 7 - digits; i < 7; i++)
		buf[(*p)++] = (char)('0' + d[i]);
}

static void action_adc_test(void)
{
	uint8_t  ch_idx     = 0;
	uint8_t  last_ch    = 0xFF;
	uint16_t last_vpp   = 0xFFFF;
	uint16_t last_dc    = 0xFFFF;
	uint32_t last_freq  = 0xFFFFFFFF;
	uint8_t  meas_tick  = 0;
	tsp_adc_meas_t meas;

	tsp_adc_init();
	tsp_adc_select_channel(0);

	tsp_tft18_clear(BLACK);
	tsp_tft18_draw_line_h(0, 16, 160, BLUE);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"S0/S1:CH  PUSH:Exit ", GRAY1, BLACK);

	while (1) {
		tsp_key_scan();
		if (tsp_key_pressed(KEY_PUSH)) break;

		if (tsp_key_pressed(KEY_S0)) {
			ch_idx = (ch_idx == 0) ? ADC_CH_COUNT - 1 : ch_idx - 1;
		}
		if (tsp_key_pressed(KEY_S1)) {
			ch_idx = (ch_idx + 1 >= ADC_CH_COUNT) ? 0 : ch_idx + 1;
		}

		/* Channel change */
		if (ch_idx != last_ch) {
			tsp_adc_select_channel(ch_idx);

			/* Row 0: title + channel label */
			{
				char buf[21]; uint8_t p = 0;
				const char *s;
				buf[p++] = 'A'; buf[p++] = 'D'; buf[p++] = 'C';
				buf[p++] = ' '; buf[p++] = 'T'; buf[p++] = 'e';
				buf[p++] = 's'; buf[p++] = 't'; buf[p++] = ' ';
				buf[p++] = ' '; buf[p++] = '[';
				for (s = tsp_adc_channels[ch_idx].label; *s; s++)
					buf[p++] = *s;
				buf[p++] = ']';
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 0, (uint8_t *)buf, YELLOW, BLUE);
			}

			/* Row 2: channel detail */
			{
				char buf[21]; uint8_t p = 0;
				const char *s;
				for (s = tsp_adc_channels[ch_idx].detail; *s; s++)
					buf[p++] = *s;
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);
			}

			last_ch   = ch_idx;
			last_vpp  = 0xFFFF;
			last_dc   = 0xFFFF;
			last_freq = 0xFFFFFFFF;
		}

		/* Measurement every ~200ms */
		meas_tick++;
		if (meas_tick >= 20) {
			meas_tick = 0;
			tsp_adc_measure(&meas);

			/* Row 3: Vpp */
			if (meas.vpp_mv != last_vpp) {
				char buf[21]; uint8_t p = 0;
				buf[p++] = 'V'; buf[p++] = 'p'; buf[p++] = 'p';
				buf[p++] = ':'; buf[p++] = ' '; buf[p++] = ' ';
				fmt_uint16(buf, &p, meas.vpp_mv, 4);
				buf[p++] = ' '; buf[p++] = 'm'; buf[p++] = 'V';
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 3, (uint8_t *)buf, GREEN, BLACK);
				last_vpp = meas.vpp_mv;
			}

			/* Row 4: DC offset */
			if (meas.dc_mv != last_dc) {
				char buf[21]; uint8_t p = 0;
				buf[p++] = 'D'; buf[p++] = 'C'; buf[p++] = ':';
				buf[p++] = ' '; buf[p++] = ' '; buf[p++] = ' ';
				fmt_uint16(buf, &p, meas.dc_mv, 4);
				buf[p++] = ' '; buf[p++] = 'm'; buf[p++] = 'V';
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 4, (uint8_t *)buf, WHITE, BLACK);
				last_dc = meas.dc_mv;
			}

			/* Row 5: Frequency */
			if (meas.freq_hz != last_freq) {
				char buf[21]; uint8_t p = 0;
				buf[p++] = 'F'; buf[p++] = 'r'; buf[p++] = 'e';
				buf[p++] = 'q'; buf[p++] = ':'; buf[p++] = ' ';
				if (meas.freq_hz == 0) {
					buf[p++] = '-'; buf[p++] = '-';
					buf[p++] = ' '; buf[p++] = 'D'; buf[p++] = 'C';
				} else {
					uint32_t f = meas.freq_hz;
					if (f > 9999999) f = 9999999;
					fmt_uint32(buf, &p, f, 7);
					buf[p++] = ' '; buf[p++] = 'H'; buf[p++] = 'z';
				}
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 5, (uint8_t *)buf, GREEN, BLACK);
				last_freq = meas.freq_hz;
			}
		}

		delay_1ms(10);
	}

	tsp_menu_request_redraw();
}

/* ===== AD5933 Impedance Test ===== */

/* Helper: format uint8_t as 2-char hex string (null-terminated) */
static void hex2(uint8_t val, char *out)
{
	out[0] = "0123456789ABCDEF"[(val >> 4) & 0xF];
	out[1] = "0123456789ABCDEF"[ val       & 0xF];
	out[2] = '\0';
}

static void action_ad5933_test(void)
{
	int16_t real, imag;
	float mag;
	float gain_factor = 0.0f;
	float cal_mag_saved = 0.0f;

	#define CAL_RESISTANCE  220.0f

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

	/* ===== Calibration phase ===== */
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Cal: 220 Ohm  ", CYAN, BLACK);
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
			cal_mag_saved = cal_mag;
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
	tsp_tft18_show_str_color(0, 2, (uint8_t *)"cM:       M:        ", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Freq: 1000 Hz       ", WHITE, BLACK);

	/* Show cal_mag once */
	{
		char buf[6]; uint8_t p = 0;
		uint16_t cm = (uint16_t)(cal_mag_saved + 0.5f);
		buf[p++] = '0' + (cm / 10000) % 10;
		buf[p++] = '0' + (cm / 1000) % 10;
		buf[p++] = '0' + (cm / 100) % 10;
		buf[p++] = '0' + (cm / 10) % 10;
		buf[p++] = '0' + (cm % 10);
		buf[p] = '\0';
		tsp_tft18_show_str_color(24, 2, (uint8_t *)buf, WHITE, BLACK);
	}
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Real:               ", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Imag:               ", CYAN, BLACK);
	if (gain_factor > 0.0f) {
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"Z   :               ", GREEN, BLACK);
	} else {
		tsp_tft18_show_str_color(0, 6, (uint8_t *)"Mag :               ", GREEN, BLACK);
	}

	/* --- Live display loop --- */
	{
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

			/* Display current Mag on Row 2 (after "M:" at x=96) */
			{
				char buf[6]; uint8_t p = 0;
				uint16_t m = (uint16_t)(mag + 0.5f);
				buf[p++] = '0' + (m / 10000) % 10;
				buf[p++] = '0' + (m / 1000) % 10;
				buf[p++] = '0' + (m / 100) % 10;
				buf[p++] = '0' + (m / 10) % 10;
				buf[p++] = '0' + (m % 10);
				buf[p] = '\0';
				tsp_tft18_show_str_color(96, 2, (uint8_t *)buf, WHITE, BLACK);
			}

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
				(uint8_t)((AD5933_CTRL_REPEAT_FREQ | AD5933_VOLT_200MV | AD5933_PGA_X1) >> 8));
			tsp_ad5933_write_reg(AD5933_REG_CTRL_L, AD5933_CLK_EXTERNAL);
		}
	}

exit_ad5933:
	tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
		(uint8_t)(AD5933_CTRL_POWER_DOWN >> 8));
	tsp_menu_request_redraw();
}

/* ===== Main Menu ===== */

static tsp_menu_item_t main_menu[] = {
	{"AD5933 Test",    action_ad5933_test},
	{"DDS Test",       action_dds_test},
	{"ADC Test",       action_adc_test},
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

	tsp_key_init();
	tsp_menu_init("== Signal 2026 ==", main_menu, MAIN_MENU_COUNT);

	while (1) {
		tsp_key_scan();
		tsp_menu_run();
		delay_1ms(10);
	}
}
