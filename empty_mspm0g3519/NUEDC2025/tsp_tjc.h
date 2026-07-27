#ifndef _TSP_TJC_H
#define _TSP_TJC_H

#include <stdint.h>

/* TJC serial screen driver (TJC4827X543_011C, X5 series, 480x272).
 * Protocol layer on top of tsp_uart6.  UART6 on PC11(TX)/PC10(RX), J11.
 * Screen TX is ~5V — PC10 needs 5V→3.3V level shifting (10k/20k divider). */

typedef struct {
	uint8_t page_id;
	uint8_t comp_id;
	uint8_t event;		/* 0x01=press, 0x00=release */
} tjc_event_t;

/* Init UART6 at given baudrate and enable RX */
void tsp_tjc_init(uint32_t baudrate);

/* === Send commands (auto-append FF FF FF terminator) === */
void tsp_tjc_page(uint8_t page_id);
void tsp_tjc_set_val(const char *obj, int32_t val);
void tsp_tjc_set_txt(const char *obj, const char *txt);
void tsp_tjc_vis(const char *obj, uint8_t visible);
void tsp_tjc_cmd(const char *raw_cmd);

/* === Waveform bulk transfer ===
 * Returns 1 on success, 0 on timeout (no 0xFE ready signal). */
uint8_t tsp_tjc_addt(const char *obj, uint8_t ch,
                     const uint8_t *data, uint16_t len);

/* === Event polling ===
 * Call from main loop.  Parses 0x65 button events from UART6 RX buffer.
 * Returns 1 and fills evt on event, 0 if no complete event available. */
uint8_t tsp_tjc_poll(tjc_event_t *evt);

#endif
