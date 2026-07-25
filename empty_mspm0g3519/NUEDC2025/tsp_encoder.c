#include "tsp_encoder.h"
#include "tsp_gpio.h"
#include "tsp_isr.h"

/* Speed calculation interval: 20ms (matching HSP's encoder read period) */
#define ENC_SPEED_INTERVAL_MS   20

static volatile int32_t  g_enc_count;       /* accumulated pulse count */
static volatile int16_t  g_enc_speed;       /* latest speed measurement */
static          uint32_t g_enc_last_tick;   /* last speed update tick */
static          int32_t  g_enc_last_count;  /* last count for speed calc */
static          uint8_t  g_enc_first_run = 1;

/* PHA0 pin IIDX — matches SysConfig generated value */
#define ENC_PHA0_IIDX   (DL_GPIO_IIDX_DIO14)   /* PA14 */

void tsp_encoder_init(void)
{
    g_enc_count      = 0;
    g_enc_speed      = 0;
    g_enc_last_tick  = sys_tick_counter;

    /* Disable PHA0 interrupt on startup to prevent spurious triggers
     * from floating pin when no encoder is connected.
     * Re-enable via DL_GPIO_enableInterrupt(GPIOA, DL_GPIO_PIN_14)
     * once a physical encoder is attached. */
    DL_GPIO_disableInterrupt(GPIOA, DL_GPIO_PIN_14);
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
        int32_t current = g_enc_count;

        if (g_enc_first_run) {
            g_enc_last_count = current;
            g_enc_first_run  = 0;
        }

        {
            int32_t delta = current - g_enc_last_count;
            if (delta > INT16_MAX)  delta = INT16_MAX;
            if (delta < INT16_MIN)  delta = INT16_MIN;
            g_enc_speed = (int16_t)delta;
        }
        g_enc_last_count = current;
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
    g_enc_count      = 0;
    g_enc_speed      = 0;
    g_enc_last_tick  = sys_tick_counter;
    g_enc_last_count = 0;
    g_enc_first_run  = 1;
    __set_PRIMASK(primask);
}

/*
 * tsp_encoder_enable — enable PHA0 (PA14) interrupt and reset count.
 * Call when entering a mode that uses the encoder (e.g. DDS interactive).
 */
void tsp_encoder_enable(void)
{
    DL_GPIO_clearInterruptStatus(GPIOA, DL_GPIO_PIN_14);
    DL_GPIO_enableInterrupt(GPIOA, DL_GPIO_PIN_14);

    /* Enable GROUP1 interrupt for GPIOA at CPUSS level */
    CPUSS->INT_GROUP[1].ISET = DL_INTERRUPT_GROUP1_GPIOA;

    /* Enable GROUP1 in NVIC */
    NVIC_EnableIRQ(PORTA_INT_IRQN);

    tsp_encoder_reset();
}

/*
 * tsp_encoder_disable — disable PHA0 interrupt and reset count.
 * Call when leaving encoder-dependent mode to prevent spurious interrupts.
 */
void tsp_encoder_disable(void)
{
    DL_GPIO_disableInterrupt(GPIOA, DL_GPIO_PIN_14);
    CPUSS->INT_GROUP[1].ICLR = DL_INTERRUPT_GROUP1_GPIOA;
    NVIC_DisableIRQ(PORTA_INT_IRQN);
    tsp_encoder_reset();
}
