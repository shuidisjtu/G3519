#ifndef _TSP_K230_H
#define _TSP_K230_H

#include "tsp_common_headfile.h"

/* ===== K230 YbProtocol Parser (Yahboom AIMOTION2 vision module) =====
 * Parses ASCII frames reported by the stock Yahboom GUI firmware:
 *
 *     $LL,FF,xxx,yyy,www,hhh[,MSG[,VVV]]#\n
 *
 *   $ / #  : frame start / end (0x24 / 0x23), trailing \n ignored
 *   LL     : frame length (NOT trusted — framing is done by $...#)
 *   FF     : function ID (see K230_ID_* below)
 *   x,y,w,h: target box in screen coordinates
 *   MSG    : optional text (QR content, class name, tag_id, ...)
 *
 * Architecture: ISR enqueues bytes (tsp_uart_k230.c); tsp_k230_task()
 * consumes the ring buffer and runs the frame state machine in the
 * MAIN LOOP — never parse inside the ISR (bad practice in the vendor
 * reference example, deliberately fixed here).
 *
 * Usage:
 *   tsp_uart_k230_init();            // once, after SYSCFG_DL_init()
 *   tsp_k230_init();
 *   tsp_uart_k230_rx_enable();       // when ready to receive
 *   while (1) {
 *       tsp_k230_task();             // call every loop iteration
 *       if (tsp_k230_get_target(&t)) { ... use t.x/t.y/t.w/t.h ... }
 *   }
 */

/* YbProtocol function IDs (full list: docs/k230_firmware_ref/YbProtocol.py) */
#define K230_ID_COLOR          1
#define K230_ID_QRCODE         3
#define K230_ID_APRILTAG       4
#define K230_ID_FACE_DETECT    6
#define K230_ID_PERSON_DETECT  9
#define K230_ID_HAND_DETECT    11
#define K230_ID_OBJECT_DETECT  14
#define K230_ID_NANO_TRACKER   15

#define K230_FRAME_LEN_MAX     64   /* max raw frame length ($...#) */
#define K230_MSG_LEN           32   /* max stored MSG text length */

typedef struct {
    uint8_t func_id;                /* YbProtocol function ID */
    int16_t x;                      /* target box: top-left x */
    int16_t y;                      /* target box: top-left y */
    int16_t w;                      /* target box: width */
    int16_t h;                      /* target box: height */
    char    msg[K230_MSG_LEN];      /* optional text field, "" if none */
} k230_target_t;

/* Reset parser state and statistics */
void tsp_k230_init(void);

/* Consume ring buffer + run frame state machine. Call from main loop. */
void tsp_k230_task(void);

/* Copy latest target into *out. Returns 1 if a NEW frame arrived since
 * the previous call (fresh flag auto-clears), 0 otherwise. */
uint8_t tsp_k230_get_target(k230_target_t *out);

/* Statistics (for menu display / debugging) */
uint32_t tsp_k230_frame_count(void);   /* frames parsed OK */
uint32_t tsp_k230_error_count(void);   /* malformed frames dropped */

#endif
