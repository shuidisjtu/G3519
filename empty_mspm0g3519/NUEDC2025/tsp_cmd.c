#include "tsp_cmd.h"
#include "tsp_adc.h"
#include "tsp_dds.h"
#include <string.h>

/* ─── Transport callbacks ─── */
static tsp_cmd_tx_fn    g_tx;
static tsp_cmd_rx_fn    g_rx;
static tsp_cmd_avail_fn g_avail;

/* ─── Line buffer ─── */
static char     g_line[CMD_LINE_BUF_SIZE];
static uint16_t g_line_pos;

/* ─── Response helpers ─── */

static void tx_ok(const char *data)
{
    g_tx("OK");
    if (data && data[0]) {
        g_tx(",");
        g_tx(data);
    }
    g_tx("\r\n");
}

static void tx_err(const char *msg)
{
    g_tx("ERR,");
    g_tx(msg);
    g_tx("\r\n");
}

/* itoa for uint32_t (no stdlib needed) */
static void u32_to_str(uint32_t val, char *buf, uint8_t buflen)
{
    uint8_t i = 0;
    char tmp[11];
    if (val == 0) { tmp[i++] = '0'; }
    while (val > 0 && i < 10) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    uint8_t j;
    for (j = 0; j < i && j < buflen - 1; j++) {
        buf[j] = tmp[i - 1 - j];
    }
    buf[j] = '\0';
}

/* simple atoi (unsigned) */
static uint32_t str_to_u32(const char *s)
{
    uint32_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

/* case-insensitive compare */
static int streqi(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

/* ─── Parse comma-separated tokens ─── */

#define MAX_TOKENS  6

static uint8_t tokenize(char *line, char *tokens[], uint8_t max_tok)
{
    uint8_t count = 0;
    char *p = line;
    while (*p && count < max_tok) {
        tokens[count++] = p;
        while (*p && *p != ',') p++;
        if (*p == ',') { *p = '\0'; p++; }
    }
    return count;
}

/* ─── Command handlers ─── */

static void cmd_ver(void)
{
    tx_ok("G3519-Signal,v1.0");
}

static void cmd_adc(char *tokens[], uint8_t count)
{
    char buf[12];

    if (count < 2) { tx_err("usage: ADC,<ch 1-5>"); return; }

    uint32_t ch = str_to_u32(tokens[1]);
    if (ch < 1 || ch > 5) { tx_err("ch 1-5"); return; }

    tsp_adc_select_channel((uint8_t)(ch - 1));
    uint16_t mv = tsp_adc_read_avg_mv(8);
    u32_to_str(mv, buf, sizeof(buf));
    tx_ok(buf);
}

static void cmd_freq(char *tokens[], uint8_t count)
{
    char buf[12];
    tsp_adc_meas_t meas;

    if (count < 2) { tx_err("usage: FREQ,<ch 1-5>"); return; }

    uint32_t ch = str_to_u32(tokens[1]);
    if (ch < 1 || ch > 5) { tx_err("ch 1-5"); return; }

    tsp_adc_select_channel((uint8_t)(ch - 1));
    tsp_adc_measure(&meas);
    u32_to_str(meas.freq_hz, buf, sizeof(buf));
    tx_ok(buf);
}

static void cmd_dds(char *tokens[], uint8_t count)
{
    if (count < 2) { tx_err("usage: DDS,<freq>,<SINE|SQR|TRI> or DDS,STOP"); return; }

    if (streqi(tokens[1], "STOP")) {
        tsp_dds_stop();
        tx_ok(NULL);
        return;
    }

    if (count < 3) { tx_err("usage: DDS,<freq>,<SINE|SQR|TRI>"); return; }

    uint32_t freq = str_to_u32(tokens[1]);
    if (freq == 0) { tx_err("freq>0"); return; }

    uint16_t wave;
    if      (streqi(tokens[2], "SINE")) wave = AD9833_SINE;
    else if (streqi(tokens[2], "SQR"))  wave = AD9833_SQUARE;
    else if (streqi(tokens[2], "TRI"))  wave = AD9833_TRIANGLE;
    else { tx_err("wave: SINE|SQR|TRI"); return; }

    tsp_dds_set_output(freq, wave);
    tx_ok(NULL);
}

/* ─── Command dispatch ─── */

static void process_line(char *line)
{
    char *tokens[MAX_TOKENS];
    uint8_t count = tokenize(line, tokens, MAX_TOKENS);

    if (count == 0) return;

    if      (streqi(tokens[0], "VER?")) cmd_ver();
    else if (streqi(tokens[0], "ADC"))  cmd_adc(tokens, count);
    else if (streqi(tokens[0], "FREQ")) cmd_freq(tokens, count);
    else if (streqi(tokens[0], "DDS"))  cmd_dds(tokens, count);
    else    tx_err("unknown cmd");
}

/* ─── Public API ─── */

void tsp_cmd_init(tsp_cmd_tx_fn tx, tsp_cmd_rx_fn rx, tsp_cmd_avail_fn avail)
{
    g_tx    = tx;
    g_rx    = rx;
    g_avail = avail;
    g_line_pos = 0;
}

void tsp_cmd_poll(void)
{
    while (g_avail()) {
        uint8_t ch = g_rx();
        if (ch == '\r' || ch == '\n') {
            if (g_line_pos > 0) {
                g_line[g_line_pos] = '\0';
                process_line(g_line);
                g_line_pos = 0;
            }
        } else if (g_line_pos < CMD_LINE_BUF_SIZE - 1) {
            g_line[g_line_pos++] = (char)ch;
        }
    }
}
