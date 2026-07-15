#include "tsp_encoder.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"

/* Speed calculation interval: 20ms (matching HSP's encoder read period) */
#define ENC_SPEED_INTERVAL_MS   20

static volatile int32_t  g_enc_count;       /* accumulated pulse count */
static volatile int16_t  g_enc_speed;       /* latest speed measurement */
static          uint32_t g_enc_last_tick;   /* last speed update tick */

/* PHA0 pin IIDX — matches SysConfig generated value */
#define ENC_PHA0_IIDX   (DL_GPIO_IIDX_DIO14)   /* PA14 */

void tsp_encoder_init(void)
{
    g_enc_count      = 0;
    g_enc_speed      = 0;
    g_enc_last_tick  = sys_tick_counter;

    /* PHA0 interrupt is already enabled by SYSCFG_DL_init() via SysConfig.
     * No additional init needed — GPIOA IRQ is routed through GROUP1_IRQHandler. */
}

/*
 * tsp_encoder_isr — called from GROUP1_IRQHandler when PHA0 (PA14) triggers.
 * On each edge of PHA0, read PHB0 to determine direction:
 *   PHB0 HIGH on PHA0 rising  → forward  (+1)
 *   PHB0 LOW  on PHA0 rising  → reverse (−1)
 * (Or equivalently: count on all edges, direction = PHB0 ^ PHA0)
 */
void tsp_encoder_isr(uint8_t dio_index)
{
    if (dio_index != ENC_PHA0_IIDX) return;

    /* Read current levels */
    uint8_t pha0 = (PHA0() != 0) ? 1 : 0;
    uint8_t phb0 = (PHB0() != 0) ? 1 : 0;

    /* Quadrature decode: XNOR of PHA0 and PHB0 gives direction.
     * PHA0 edge + PHB0=HIGH → CW/forward;  PHA0 edge + PHB0=LOW → CCW/reverse.
     * Equivalent: if PHA0 == PHB0 → forward (+1), else reverse (−1). */
    if (pha0 == phb0) {
        g_enc_count++;
    } else {
        g_enc_count--;
    }
}

/*
 * tsp_encoder_update_speed — call every ENC_SPEED_INTERVAL_MS in main loop
 * or SysTick handler. Computes pulse delta since last call.
 */
void tsp_encoder_update_speed(void)
{
    uint32_t now     = sys_tick_counter;
    uint32_t elapsed = now - g_enc_last_tick;

    if (elapsed >= ENC_SPEED_INTERVAL_MS) {
        static int32_t last_count;
        int32_t current = g_enc_count;
        g_enc_speed      = (int16_t)(current - last_count);
        last_count       = current;
        g_enc_last_tick  = now;
    }
}

int32_t tsp_encoder_get_count(void)
{
    /* Disable GROUP1 interrupts briefly for atomic 32-bit read on M0+ */
    uint32_t primask;
    int32_t  val;
    primask = __get_PRIMASK();
    __disable_irq();
    val = g_enc_count;
    __set_PRIMASK(primask);
    return val;
}

int16_t tsp_encoder_get_speed(void)
{
    return g_enc_speed;
}

void tsp_encoder_reset(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    g_enc_count     = 0;
    g_enc_speed     = 0;
    g_enc_last_tick = sys_tick_counter;
    __set_PRIMASK(primask);
}
