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

/* ===== AD9833 DDS Waveform Generator ===== */

/* AD9833 DDS constants */
#define DDS_MCLK    25000000UL          /* 25 MHz master clock (X1 on board) */
#define DDS_FREQ_REG(f_hz) \
    ((uint32_t)(((uint64_t)(f_hz) * 268435456ULL + (DDS_MCLK / 2)) / DDS_MCLK))

/* AD9833 control register bits */
#define AD9833_RESET     0x2100         /* B28=1, RESET=1 */
#define AD9833_FREQ0     0x4000         /* D15-D14=01 -> FREQ0 register select */
#define AD9833_SINE      0x2000         /* B28=1, OPBITEN=0, MODE=0 -> sine (DAC) */
#define AD9833_TRIANGLE  0x2002         /* B28=1, OPBITEN=0, MODE=1 -> triangle (DAC) */
#define AD9833_SQUARE    0x2028         /* B28=1, OPBITEN=1, DIV2=1 -> square (MSB) */

/* --- Send 16-bit word to AD9833, MSB first, falling-edge SCLK --- */
static void dds_write(uint16_t data)
{
	uint8_t i;
	/* AD9833 datasheet: SCLK idles HIGH (CPOL=1).
	 * FSYNC falling edge must occur while SCLK is HIGH.
	 * AD9833 samples SDATA on every SCLK falling edge. */
	DDS_FSYNC_HIGH();
	DDS_SCLK_HIGH();
	DDS_SDATA_LOW();
	DDS_FSYNC_LOW();
	for (i = 0; i < 16; i++) {
		if (data & 0x8000) {
			DDS_SDATA_HIGH();
		} else {
			DDS_SDATA_LOW();
		}
		DDS_SCLK_LOW();       /* Falling edge → AD9833 samples SDATA */
		DDS_SCLK_HIGH();      /* Rising  edge → prepare next bit */
		data <<= 1;
	}
	DDS_SDATA_LOW();
	DDS_FSYNC_HIGH();
	/* SCLK stays HIGH (idle per CPOL=1) */
}

/* --- Generic AD9833 init: load FREQ0, output selected waveform --- */
static void dds_set_output(uint32_t freq_hz, uint16_t waveform_ctrl)
{
	uint32_t reg;
	uint16_t lsb, msb;

	reg = DDS_FREQ_REG(freq_hz);
	lsb = AD9833_FREQ0 | ((uint16_t)(reg & 0x3FFF));
	msb = AD9833_FREQ0 | ((uint16_t)((reg >> 14) & 0x3FFF));

	dds_write(AD9833_RESET);
	dds_write(lsb);
	dds_write(msb);
	dds_write(waveform_ctrl);
}

/* --- Waveform metadata --- */
static const char * const dds_wave_names[] = {
	"Square", "Sine", "Triangle"
};
static const char * const dds_vout_info[] = {
	"Vout ~3.1V (MSB)",
	"Vout ~0.6Vpp (DAC)",
	"Vout ~0.6Vpp (DAC)",
};
static const uint16_t dds_wave_ctrl[] = {
	AD9833_SQUARE, AD9833_SINE, AD9833_TRIANGLE
};
#define DDS_WAVE_COUNT  (sizeof(dds_wave_ctrl) / sizeof(dds_wave_ctrl[0]))

/* --- Get adaptive frequency step --- */
static uint32_t dds_get_step(uint32_t freq, int32_t delta)
{
	uint32_t base;
	if      (freq < 1000)  base = 10;
	else if (freq < 10000) base = 100;
	else                   base = 1000;   /* 10 kHz ~ 50 kHz */

	/* Fast rotation: coarser step */
	if (delta >= 3 || delta <= -3) base *= 10;

	return base;
}

/* --- Interactive DDS Test --- */
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
	dds_set_output(freq, dds_wave_ctrl[wave_idx]);

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
					uint32_t step = dds_get_step(freq, enc_acc);
					int32_t  new_freq = (int32_t)freq + enc_acc * (int32_t)step;

					if      (new_freq < 100)    freq = 100;
					else if (new_freq > 50000) freq = 50000;
					else                       freq = (uint32_t)new_freq;

					enc_acc = 0;
				}
			}

			/* --- Reconfigure DDS if needed --- */
			if (wave_idx != last_wave || freq != last_freq) {
				dds_set_output(freq, dds_wave_ctrl[wave_idx]);
			}

			/* --- Incremental LCD update: waveform row --- */
			if (wave_idx != last_wave) {
				char buf[21];
				uint8_t p = 0;
				buf[p++] = 'W'; buf[p++] = 'a'; buf[p++] = 'v'; buf[p++] = 'e';
				buf[p++] = ':'; buf[p++] = ' ';
				for (i = 0; dds_wave_names[wave_idx][i]; i++)
					buf[p++] = (char)dds_wave_names[wave_idx][i];
				while (p < 20) buf[p++] = ' ';
				buf[20] = '\0';
				tsp_tft18_show_str_color(0, 2, (uint8_t *)buf, CYAN, BLACK);
				tsp_tft18_show_str_color(0, 4, (uint8_t *)dds_vout_info[wave_idx],
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
	dds_write(AD9833_RESET);
	tsp_menu_request_redraw();
}

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

	tsp_tft18_clear(BLACK);
	tsp_tft18_show_str_color(0, 0, (uint8_t *)"AD5933 Impedance", YELLOW, BLUE);
	tsp_tft18_show_str_color(0, 7, (uint8_t *)"PUSH to exit", GRAY1, BLACK);

	/* --- Init via library API (verifies P2 fix) --- */
	tsp_ad5933_init();
	delay_1ms(10);

	/* --- Read back and display CTRL_H/CTRL_L --- */
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

	/* --- Temperature via library API (verifies P6 fix) --- */
	temp = tsp_ad5933_read_temperature();
	/* NaN check (NaN is the only float != itself) — no math.h needed */
	if (temp != temp) {
		/* Temperature read timed out — show error but keep going */
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

	/* --- Configure sweep and start (verifies P3 fix) --- */
	tsp_ad5933_set_sweep(1000, 0, 0, 100, AD5933_SETTLE_X1);
	tsp_ad5933_start_sweep();

	/* Display static labels for live data */
	tsp_tft18_show_str_color(0, 3, (uint8_t *)"Freq: 1000 Hz", WHITE, BLACK);
	tsp_tft18_show_str_color(0, 4, (uint8_t *)"Real:", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 5, (uint8_t *)"Imag:", CYAN, BLACK);
	tsp_tft18_show_str_color(0, 6, (uint8_t *)"Mag :", GREEN, BLACK);

	/* --- Live display loop: Real/Imag/Magnitude + periodic temperature --- */
	{
		uint8_t  temp_tick = 0;
		int16_t  last_temp_x10 = (int16_t)(temp * 10.0f + 0.5f); /* avoid flicker */

		while (1) {
			tsp_key_scan();
			if (tsp_key_pressed(KEY_PUSH)) goto exit_ad5933;

			/* Refresh temperature every ~1s (20 * 50ms).
			   read_temperature() blocks ~30ms — acceptable at 1Hz. */
			temp_tick++;
			if (temp_tick >= 20) {
				temp_tick = 0;
				temp = tsp_ad5933_read_temperature();
				if (temp == temp) {       /* not NaN */
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
			/* Display Magnitude */
			{
				char buf[14]; uint8_t p = 0;
				uint16_t m = (uint16_t)mag;
				buf[p++] = '0' + (m / 10000) % 10;
				buf[p++] = '0' + (m / 1000) % 10;
				buf[p++] = '0' + (m / 100) % 10;
				buf[p++] = '0' + (m / 10) % 10;
				buf[p++] = '0' + (m % 10);
				buf[p] = '\0';
				tsp_tft18_show_str_color(40, 6, (uint8_t *)buf, GREEN, BLACK);
			}

			delay_1ms(50);
		}
	}

exit_ad5933:
	/* Stop DDS output */
	tsp_ad5933_write_reg(AD5933_REG_CTRL_H,
		(uint8_t)(AD5933_CTRL_POWER_DOWN >> 8));
	tsp_menu_request_redraw();
}

/* ===== Main Menu ===== */

static tsp_menu_item_t main_menu[] = {
	{"K230 Test",      action_k230_test},
	{"DDS Test",       action_dds_test},
	{"AD5933 Test",    action_ad5933_test},
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
