#include "tsp_k230.h"
#include "tsp_uart_k230.h"
#include <stdlib.h>

#define K230_PTO_HEAD   0x24    /* '$' */
#define K230_PTO_TAIL   0x23    /* '#' */

#define K230_MAX_FIELDS 12      /* $LL,FF,x,y,w,h,MSG,VVV,... */

/* ─── Frame collector state (main-loop context only) ─── */
static char     g_frame[K230_FRAME_LEN_MAX];
static uint16_t g_frame_idx;
static uint8_t  g_collecting;           /* 0 = waiting for '$', 1 = in frame */

/* ─── Latest parsed target ─── */
static k230_target_t g_latest;
static uint8_t       g_fresh;           /* set on parse OK, cleared by get_target */
static uint32_t      g_frame_count;
static uint32_t      g_error_count;

void tsp_k230_init(void)
{
    g_frame_idx   = 0;
    g_collecting  = 0;
    g_fresh       = 0;
    g_frame_count = 0;
    g_error_count = 0;
    memset(&g_latest, 0, sizeof(g_latest));
}

/* All chars in str are decimal digits (str non-empty) */
static uint8_t is_number(const char *str)
{
    if (*str == '\0') {
        return 0;
    }
    while (*str) {
        if (*str < '0' || *str > '9') {
            return 0;
        }
        str++;
    }
    return 1;
}

/* Parse one complete frame "$...#" (NUL-terminated, '#' already stripped).
 * Layout after '$': "LL,FF,..." — split on ',' then interpret.
 * Returns 1 on success (g_latest updated), 0 on malformed frame. */
static uint8_t k230_parse_frame(char *body)
{
    char   *fields[K230_MAX_FIELDS];
    uint8_t nf = 0;
    char   *p  = body;

    /* Split on ',' in place */
    fields[nf++] = p;
    while (*p && nf < K230_MAX_FIELDS) {
        if (*p == ',') {
            *p = '\0';
            fields[nf++] = p + 1;
        }
        p++;
    }

    /* Minimum: LL, FF, one payload field. LL/FF must be 2-digit numbers.
     * LL value itself is NOT trusted for framing (vendor inconsistency). */
    if (nf < 3 || !is_number(fields[0]) || !is_number(fields[1])) {
        return 0;
    }

    g_latest.func_id = (uint8_t)atoi(fields[1]);
    g_latest.msg[0]  = '\0';

    if (nf >= 6 && is_number(fields[2]) && is_number(fields[3]) &&
        is_number(fields[4]) && is_number(fields[5])) {
        /* Coordinate frame: $LL,FF,x,y,w,h[,MSG[,VVV]]# */
        g_latest.x = (int16_t)atoi(fields[2]);
        g_latest.y = (int16_t)atoi(fields[3]);
        g_latest.w = (int16_t)atoi(fields[4]);
        g_latest.h = (int16_t)atoi(fields[5]);
        if (nf >= 7) {
            /* Re-join remaining fields into msg ("tag,deg" / "name,score") */
            uint8_t i;
            uint16_t pos = 0;
            for (i = 6; i < nf; i++) {
                uint16_t len = (uint16_t)strlen(fields[i]);
                if (pos + len + 2 > K230_MSG_LEN) {
                    break;
                }
                if (pos > 0) {
                    g_latest.msg[pos++] = ',';
                }
                memcpy(&g_latest.msg[pos], fields[i], len);
                pos += len;
            }
            g_latest.msg[pos] = '\0';
        }
    } else {
        /* Message frame: $LL,FF,MSG[,VVV]# (gesture, OCR, ...) */
        g_latest.x = 0;
        g_latest.y = 0;
        g_latest.w = 0;
        g_latest.h = 0;
        strncpy(g_latest.msg, fields[2], K230_MSG_LEN - 1);
        g_latest.msg[K230_MSG_LEN - 1] = '\0';
    }

    return 1;
}

/* Feed one byte into the frame state machine */
static void k230_feed_byte(uint8_t ch)
{
    if (!g_collecting) {
        /* Waiting for frame head; '\n' and noise are skipped here */
        if (ch == K230_PTO_HEAD) {
            g_collecting = 1;
            g_frame_idx  = 0;
        }
        return;
    }

    if (ch == K230_PTO_TAIL) {
        /* Frame complete: terminate and parse */
        g_frame[g_frame_idx] = '\0';
        if (k230_parse_frame(g_frame)) {
            g_frame_count++;
            g_fresh = 1;
        } else {
            g_error_count++;
        }
        g_collecting = 0;
        g_frame_idx  = 0;
    } else if (ch == K230_PTO_HEAD) {
        /* Unexpected new head: drop partial frame, restart */
        g_error_count++;
        g_frame_idx = 0;
    } else if (g_frame_idx >= K230_FRAME_LEN_MAX - 1) {
        /* Overlong frame: drop and resync */
        g_error_count++;
        g_collecting = 0;
        g_frame_idx  = 0;
    } else {
        g_frame[g_frame_idx++] = (char)ch;
    }
}

void tsp_k230_task(void)
{
    while (tsp_uart_k230_available()) {
        k230_feed_byte(tsp_uart_k230_read_byte());
    }
}

uint8_t tsp_k230_get_target(k230_target_t *out)
{
    if (out != NULL) {
        *out = g_latest;
    }
    if (g_fresh) {
        g_fresh = 0;
        return 1;
    }
    return 0;
}

uint32_t tsp_k230_frame_count(void)
{
    return g_frame_count;
}

uint32_t tsp_k230_error_count(void)
{
    return g_error_count;
}
