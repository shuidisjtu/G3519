#include "tsp_tjc.h"
#include "tsp_uart3.h"
#include "tsp_isr.h"

#define TJC_CMD_BUF_SIZE	64
#define TJC_ADDT_TIMEOUT_MS	200

extern volatile uint32_t sys_tick_counter;

/* ─── RX state machine (used by addt + poll) ─── */

enum {
	TJC_RX_IDLE = 0,
	TJC_RX_HEAD,
	TJC_RX_BODY,
	TJC_RX_TAIL
};

static uint8_t  rx_state;
static uint8_t  rx_head;
static uint8_t  rx_body[3];
static uint8_t  rx_body_idx;
static uint8_t  rx_tail_idx;
static uint8_t  rx_skip_count;

/* ─── Terminator ─── */

static void tjc_send_end(void)
{
	tsp_uart3_send_byte(0xFF);
	tsp_uart3_send_byte(0xFF);
	tsp_uart3_send_byte(0xFF);
}

/* ─── Integer to ASCII (no sprintf) ─── */

static uint8_t tjc_itoa(char *buf, int32_t val)
{
	uint8_t p = 0;
	uint32_t u;

	if (val < 0) {
		buf[p++] = '-';
		u = (uint32_t)(-(val + 1)) + 1;
	} else {
		u = (uint32_t)val;
	}

	if (u == 0) {
		buf[p++] = '0';
		return p;
	}

	char tmp[10];
	uint8_t n = 0;
	while (u > 0) {
		tmp[n++] = (char)('0' + (u % 10));
		u /= 10;
	}
	while (n > 0)
		buf[p++] = tmp[--n];

	return p;
}

static uint8_t tjc_utoa16(char *buf, uint16_t val)
{
	uint8_t p = 0;

	if (val == 0) {
		buf[p++] = '0';
		return p;
	}

	char tmp[5];
	uint8_t n = 0;
	while (val > 0) {
		tmp[n++] = (char)('0' + (val % 10));
		val /= 10;
	}
	while (n > 0)
		buf[p++] = tmp[--n];

	return p;
}

/* ─── Init ─── */

void tsp_tjc_init(uint32_t baudrate)
{
	tsp_uart3_init(baudrate);
	tsp_uart3_rx_enable();
}

/* ─── Send commands ─── */

void tsp_tjc_cmd(const char *raw_cmd)
{
	tsp_uart3_send_string(raw_cmd);
	tjc_send_end();
}

void tsp_tjc_page(uint8_t page_id)
{
	char buf[12];
	uint8_t p = 0;
	buf[p++] = 'p'; buf[p++] = 'a'; buf[p++] = 'g'; buf[p++] = 'e';
	buf[p++] = ' ';
	p += tjc_utoa16(&buf[p], page_id);
	buf[p] = '\0';
	tsp_tjc_cmd(buf);
}

void tsp_tjc_set_val(const char *obj, int32_t val)
{
	char buf[TJC_CMD_BUF_SIZE];
	uint8_t p = 0;

	while (*obj && p < TJC_CMD_BUF_SIZE - 20)
		buf[p++] = *obj++;
	buf[p++] = '.'; buf[p++] = 'v'; buf[p++] = 'a'; buf[p++] = 'l';
	buf[p++] = '=';
	p += tjc_itoa(&buf[p], val);
	buf[p] = '\0';

	tsp_tjc_cmd(buf);
}

void tsp_tjc_set_txt(const char *obj, const char *txt)
{
	char buf[TJC_CMD_BUF_SIZE];
	uint8_t p = 0;

	while (*obj && p < TJC_CMD_BUF_SIZE - 7)
		buf[p++] = *obj++;
	buf[p++] = '.'; buf[p++] = 't'; buf[p++] = 'x'; buf[p++] = 't';
	buf[p++] = '='; buf[p++] = '"';
	buf[p] = '\0';

	tsp_uart3_send_string(buf);
	tsp_uart3_send_string(txt);
	tsp_uart3_send_byte('"');
	tjc_send_end();
}

void tsp_tjc_vis(const char *obj, uint8_t visible)
{
	char buf[TJC_CMD_BUF_SIZE];
	uint8_t p = 0;
	buf[p++] = 'v'; buf[p++] = 'i'; buf[p++] = 's'; buf[p++] = ' ';
	while (*obj && p < TJC_CMD_BUF_SIZE - 4)
		buf[p++] = *obj++;
	buf[p++] = ',';
	buf[p++] = visible ? '1' : '0';
	buf[p] = '\0';
	tsp_tjc_cmd(buf);
}

/* ─── Waveform bulk transfer (addt) ─── */

static uint8_t tjc_wait_byte(uint8_t target, uint32_t timeout_ms)
{
	uint32_t start = sys_tick_counter;
	while ((sys_tick_counter - start) < timeout_ms) {
		if (tsp_uart3_available() > 0) {
			uint8_t b = tsp_uart3_read_byte();
			if (b == target)
				return 1;
		}
	}
	return 0;
}

uint8_t tsp_tjc_addt(const char *obj, uint8_t ch,
                     const uint8_t *data, uint16_t len)
{
	char buf[TJC_CMD_BUF_SIZE];
	uint8_t p = 0;

	buf[p++] = 'a'; buf[p++] = 'd'; buf[p++] = 'd'; buf[p++] = 't';
	buf[p++] = ' ';
	while (*obj && p < TJC_CMD_BUF_SIZE - 16)
		buf[p++] = *obj++;
	buf[p++] = '.'; buf[p++] = 'i'; buf[p++] = 'd';
	buf[p++] = ',';
	p += tjc_utoa16(&buf[p], ch);
	buf[p++] = ',';
	p += tjc_utoa16(&buf[p], len);
	buf[p] = '\0';

	tsp_uart3_flush_rx();
	rx_state = TJC_RX_IDLE;
	tsp_tjc_cmd(buf);

	if (!tjc_wait_byte(0xFE, TJC_ADDT_TIMEOUT_MS))
		return 0;

	tsp_uart3_send_bytes(data, len);

	return tjc_wait_byte(0xFD, TJC_ADDT_TIMEOUT_MS);
}

/* ─── Event polling (0x65 frame state machine) ─── */

static const uint8_t frame_body_len[] = {
	/* 0x00 */ 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x10 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x20 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x30 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x40 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x50 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x60 */ 0, 0, 0, 0, 0, 3, 1, 5, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x70 */ 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x80 */ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

uint8_t tsp_tjc_poll(tjc_event_t *evt)
{
	while (tsp_uart3_available() > 0) {
		uint8_t b = tsp_uart3_read_byte();

		switch (rx_state) {
		case TJC_RX_IDLE:
			if (b == 0xFF)
				break;
			rx_head = b;
			if (b < 0x90 && frame_body_len[b] > 0) {
				rx_body_idx = 0;
				rx_skip_count = frame_body_len[b];
				rx_state = TJC_RX_BODY;
			} else {
				rx_state = TJC_RX_TAIL;
				rx_tail_idx = 0;
			}
			break;

		case TJC_RX_BODY:
			rx_body[rx_body_idx < 3 ? rx_body_idx : 2] = b;
			rx_body_idx++;
			if (rx_body_idx >= rx_skip_count) {
				rx_state = TJC_RX_TAIL;
				rx_tail_idx = 0;
			}
			break;

		case TJC_RX_TAIL:
			if (b == 0xFF) {
				rx_tail_idx++;
				if (rx_tail_idx >= 3) {
					rx_state = TJC_RX_IDLE;
					if (rx_head == 0x65 && evt) {
						evt->page_id = rx_body[0];
						evt->comp_id = rx_body[1];
						evt->event   = rx_body[2];
						return 1;
					}
				}
			} else {
				rx_head = b;
				if (b < 0x90 && frame_body_len[b] > 0) {
					rx_body_idx = 0;
					rx_skip_count = frame_body_len[b];
					rx_state = TJC_RX_BODY;
				} else {
					rx_state = TJC_RX_TAIL;
					rx_tail_idx = 0;
				}
			}
			break;

		default:
			rx_state = TJC_RX_IDLE;
			break;
		}
	}

	return 0;
}
