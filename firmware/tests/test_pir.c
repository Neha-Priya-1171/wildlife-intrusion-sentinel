/**
 * @file    test_pir.c
 * @brief   Simple PIR test on PB5. Run this one test, watch three variables, done.
 *
 * WHY PB5 AND NOT PA1: PA1 read permanently HIGH and never changed. PB5 is a free pin,
 * and unlike PA1 it tolerates up to 5 V, so the PIR's ~4.3 V output cannot damage it.
 *
 * WIRING (three wires, nothing else):
 *     PIR VCC  -> 5 V
 *     PIR GND  -> GND   (same ground as the Black Pill)
 *     PIR OUT  -> PB5
 *
 * WHAT HAPPENS: PA5 blinks slowly for 30 s (warm-up - keep still), then copies the
 * PIR output directly. Wave your hand in
 * front of the sensor and the LED should light. Stand still and it should go out after
 * a few seconds. The PIR needs about 30-60 s after power-up before it behaves, so wait
 * before judging it.
 *
 * WHAT TO WATCH IN KEIL (right-click each name -> Add to Watch 1):
 *     pir_state      what the pin reads RIGHT NOW.  0 = low, 1 = high.
 *     pir_events     how many times motion has started. Goes up by 1 per wave.
 *     pir_low_seen   0 until the pin has been low at least once, then stays 1.
 *     pir_high_seen  0 until the pin has been high at least once, then stays 1.
 *     pir_warm       0 during the 30 s warm-up blink, 1 once armed.
 *
 * READING A STUCK-LOW PIN: pir_low_seen = 1 with pir_high_seen = 0 and pir_events = 0
 * means PB5 never once went high. The MCU side is working (it is reading a real low,
 * not floating, because of the pull-down); nothing is arriving from the sensor. Check
 * the OUT wire, then measure PIR OUT to GND with the module powered and a hand waving:
 * if it never rises above about 0.5 V, the module is not producing an output at all.
 *
 * HOW TO READ IT:
 *     pir_low_seen becomes 1 and pir_events counts your waves  -> the PIR works.
 *     pir_low_seen stays 0 for several minutes                  -> the pin is stuck high.
 *         That is a wiring or sensor fault, not a firmware one. Unplug the OUT wire from
 *         PB5 and look again: with nothing connected, pir_state must read 0. If it still
 *         reads 1 with the wire off, the problem is on the board side, not the sensor.
 *
 * Files to build: test_pir.c + common/board.c
 */
#include "board.h"

/* The HC-SR501 needs 30-60 s after power-up before its output means anything.
 * PA5 blinks slowly for the whole window so you can see the system is alive and
 * deliberately waiting, rather than broken. */
#define PIR_WARMUP_MS   30000u

/* ---- the variables to watch ---- */
volatile uint8_t  pir_state    = 0;   /* live level of PB5: 0 or 1          */
volatile uint32_t pir_events   = 0;   /* number of times motion started     */
volatile uint8_t  pir_low_seen = 0;   /* 1 once the pin has ever read low   */
volatile uint8_t  pir_high_seen= 0;   /* 1 once the pin has ever read high  */
volatile uint8_t  pir_warm     = 0;   /* 0 during warm-up, 1 once armed     */

int main(void)
{
    uint8_t prev;

    board_init();                                  /* clocks + 1 ms timer */
    led_init();

    /* PB5 as a plain digital input, with a pull-down so that an unplugged wire
     * reads 0 instead of floating at some random level. A real sensor output
     * easily overrides this weak internal resistor. */
    gpio_mode(PIR_PORT, PIR_PIN, BRD_MODE_IN);
    gpio_pull(PIR_PORT, PIR_PIN, BRD_PULL_DOWN);

    /* ---- 30 s warm-up: slow blink on PA5 while the PIR settles ----
     * Keep still and away from the sensor during this. The blink is also a useful
     * "system starting" indication in a demo even if the PIR itself is misbehaving. */
    {
        uint32_t t0 = millis();
        while ((millis() - t0) < PIR_WARMUP_MS) {
            led_set((uint8_t)(((millis() - t0) / 500u) & 1u));
        }
        led_set(0);
        pir_warm = 1;
    }

    /* Start from whatever the pin is doing now, so a pin that is already high
     * does not get counted as a motion event before anything has moved. */
    prev = gpio_read(PIR_PORT, PIR_PIN);

    for (;;) {
        uint8_t now = gpio_read(PIR_PORT, PIR_PIN);   /* read the pin */

        pir_state = now;
        if (!now) pir_low_seen  = 1;                  /* remember we saw a low  */
        else      pir_high_seen = 1;                  /* remember we saw a high */
        if (now && !prev) pir_events++;               /* count only the moment it goes high */
        prev = now;

        led_set(now);                                 /* LED copies the sensor */
        delay_ms(10);
    }
}
