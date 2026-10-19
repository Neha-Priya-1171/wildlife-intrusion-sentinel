/**
 * @file    test_pir.c
 * @brief   PIR-ONLY diagnostic for PA1. Isolates the "PA1 always reads HIGH" fault.
 *
 * This is deliberately NOT a feature test - it is an instrument. It reports what the
 * PA1 input pin is physically doing and nothing else: no vibration sensor, no LCD,
 * no buzzer, no blocking 30 s warm-up that hides the first half-minute of behaviour.
 *
 * Wiring: PIR OUT --> PA1 ; PIR GND --> common GND. See docs/assembly_guide.md Stage 3.
 *         READ THE WARNING THERE FIRST: PA1 is an ADC-capable (TTa) pin and is NOT
 *         5 V tolerant. Do not connect an output measured above 3.3 V to it.
 *
 * The key variable is `pir_low_seen`. A PIR that is merely held HIGH by its own
 * hold-time pot will still drop LOW eventually, so pir_low_seen becomes 1. If
 * pir_low_seen stays 0 for minutes, the pin is genuinely stuck and the fault is
 * electrical, not in this firmware.
 *
 * Files to build: test_pir.c + common/board.c
 */
#include "board.h"

/* ---------------------------------------------------------------------------
 * Internal pull selection. Change this one value and re-flash to run the three
 * diagnostic passes described in docs/assembly_guide.md Stage 3.
 *   BRD_PULL_NONE - normal operation: the PIR drives the line, MCU stays passive.
 *   BRD_PULL_DOWN - with the PIR UNPLUGGED, PA1 must read 0. If it still reads 1
 *                   the pin itself is damaged or something else is driving it.
 *   BRD_PULL_UP   - with the PIR UNPLUGGED, PA1 must read 1. Confirms the pin and
 *                   the Watch plumbing work at all.
 * ------------------------------------------------------------------------- */
#define PIR_PULL_MODE   BRD_PULL_DOWN

/* The datasheet warm-up is 30-60 s. We do NOT block for it: we sample from t = 0
 * and simply flag when the window has elapsed, so warm-up behaviour is visible. */
#define PIR_WARMUP_MS   60000u

/* ---- Watch these in Keil ---- */
volatile uint8_t  pir_state     = 0;   /* live level of PA1, 0 or 1 (after polarity) */
volatile uint8_t  pir_raw       = 0;   /* live RAW bit 1 of GPIOA->IDR, no polarity  */
volatile uint8_t  pir_low_seen  = 0;   /* sticky: set to 1 the first time PA1 reads 0 */
volatile uint8_t  pir_high_seen = 0;   /* sticky: set to 1 the first time PA1 reads 1 */
volatile uint8_t  pir_warm      = 0;   /* 0 while inside the warm-up window           */
volatile uint32_t pir_events    = 0;   /* count of LOW->HIGH transitions only         */
volatile uint32_t pir_high_ms   = 0;   /* cumulative ms observed HIGH                 */
volatile uint32_t pir_low_ms    = 0;   /* cumulative ms observed LOW                  */
volatile uint32_t pir_last_edge_ms = 0;/* millis() of the most recent edge, any direction */

/* ---- Register snapshots, so the Watch window shows the real configuration ---- */
volatile uint32_t dbg_moder = 0;       /* GPIOA->MODER : bits [3:2] are PA1, expect 00 */
volatile uint32_t dbg_pupdr = 0;       /* GPIOA->PUPDR : bits [3:2] are PA1            */
volatile uint32_t dbg_idr   = 0;       /* GPIOA->IDR   : bit 1 is PA1                  */
volatile uint32_t dbg_ahb1enr = 0;     /* RCC->AHB1ENR : bit 0 is GPIOAEN, expect 1     */

#define PIR_POLL_MS   5u

int main(void)
{
    uint8_t  prev;
    uint32_t last_sample_ms;

    board_init();                 /* SysTick 1 ms + GPIOA/GPIOB clocks */
    led_init();

    /* PA1 as a plain digital input. MODER bits [3:2] = 00. gpio_mode() does a
     * read-modify-write, so it cannot disturb the other pins on port A. */
    gpio_mode(PIR_PORT, PIR_PIN, BRD_MODE_IN);
    gpio_pull(PIR_PORT, PIR_PIN, PIR_PULL_MODE);

    /* Snapshot the configuration once, after it has been applied. */
    dbg_ahb1enr = RCC->AHB1ENR;
    dbg_moder   = GPIOA->MODER;
    dbg_pupdr   = GPIOA->PUPDR;

    /* Prime the edge detector with the level that is ALREADY present, so a line
     * that is high at power-up does not fake a motion event on the first pass.
     * (This is the bug that makes the old test report pir_events = 1 instantly.) */
    prev = (uint8_t)(PIR_ACTIVE_HIGH ? gpio_read(PIR_PORT, PIR_PIN)
                                     : !gpio_read(PIR_PORT, PIR_PIN));
    pir_state = prev;
    pir_raw   = gpio_read(PIR_PORT, PIR_PIN);
    if (prev) pir_high_seen = 1; else pir_low_seen = 1;

    last_sample_ms = millis();

    for (;;) {
        uint32_t now = millis();
        uint8_t  raw = gpio_read(PIR_PORT, PIR_PIN);          /* bit 1 of GPIOA->IDR */
        uint8_t  lvl = (uint8_t)(PIR_ACTIVE_HIGH ? raw : !raw);

        dbg_idr   = GPIOA->IDR;
        pir_raw   = raw;
        pir_state = lvl;

        /* Sticky witnesses: these are the variables that actually settle the
         * "is it stuck, or is it just held?" question. */
        if (lvl) pir_high_seen = 1;
        else     pir_low_seen  = 1;

        /* Time accounting, so a long hold time is visible as a duration rather
         * than being mistaken for a permanent stuck level. */
        if (lvl) pir_high_ms += (now - last_sample_ms);
        else     pir_low_ms  += (now - last_sample_ms);
        last_sample_ms = now;

        if (now >= PIR_WARMUP_MS) pir_warm = 1;

        /* Count only a genuine LOW->HIGH transition, never the standing level. */
        if (lvl && !prev) {
            pir_events++;
            pir_last_edge_ms = now;
        } else if (!lvl && prev) {
            pir_last_edge_ms = now;
        }
        prev = lvl;

        /* The LED mirrors the pin continuously - no blocking blink, so the LED
         * cannot lag behind or mask a fast pulse on PA1. */
        led_set(lvl);

        delay_ms(PIR_POLL_MS);
    }
}
