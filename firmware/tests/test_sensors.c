/**
 * @file    test_sensors.c
 * @brief   TEST 3 - HC-SR501 PIR (PA1) and SW-420 vibration (PA2), polled with debounce.
 * Wiring: PIR OUT --> PA1 ; SW-420 DO --> PA2 ; grounds common.
 * Expect: 30 s PIR warm-up (LED blinks slowly). Then
 *           - PIR motion        -> ONE long LED blink  (600 ms)
 *           - Vibration/tap     -> TWO short LED blinks
 * Debugger tip: watch pir_events / vib_events / pir_state / vib_state.
 * Tune sensitivity with the blue trimmer pots on each module (see docs/assembly_guide.md).
 * Files to build: test_sensors.c + common/board.c
 */
#include "board.h"

#define PIR_WARMUP_MS   30000u
#define VIB_DEBOUNCE_MS 100u

volatile uint8_t  pir_state = 0, vib_state = 0;
volatile uint32_t pir_events = 0, vib_events = 0;

static uint8_t pir_active(void)
{
    uint8_t v = gpio_read(PIR_PORT, PIR_PIN);
    return PIR_ACTIVE_HIGH ? v : !v;
}

static uint8_t vib_active(void)
{
    uint8_t v = gpio_read(VIB_PORT, VIB_PIN);
    return VIB_ACTIVE_HIGH ? v : !v;
}

int main(void)
{
    uint8_t  pir_prev = 0, vib_prev = 0;
    uint32_t last_vib_ms = 0;

    board_init();
    led_init();
    gpio_mode(PIR_PORT, PIR_PIN, BRD_MODE_IN);   /* HC-SR501 drives its own output (3.3 V) */
    gpio_mode(VIB_PORT, VIB_PIN, BRD_MODE_IN);

    /* PIR needs ~30-60 s to stabilise after power-up */
    uint32_t t0 = millis();
    while ((millis() - t0) < PIR_WARMUP_MS) {
        led_blink(1, 100, 900);
    }
    led_set(0);

    for (;;) {
        pir_state = pir_active();
        vib_state = vib_active();

        if (pir_state && !pir_prev) {            /* rising edge = new motion */
            pir_events++;
            led_set(1); delay_ms(600); led_set(0);
        }
        pir_prev = pir_state;

        if (vib_state && !vib_prev && (millis() - last_vib_ms) > VIB_DEBOUNCE_MS) {
            last_vib_ms = millis();
            vib_events++;
            led_blink(2, 80, 80);
        }
        vib_prev = vib_state;

        delay_ms(10);                            /* 100 Hz polling is plenty */
    }
}
