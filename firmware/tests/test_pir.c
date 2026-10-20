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
 * WHAT HAPPENS: the green LED on PA5 copies the PIR output directly. Wave your hand in
 * front of the sensor and the LED should light. Stand still and it should go out after
 * a few seconds. The PIR needs about 30-60 s after power-up before it behaves, so wait
 * before judging it.
 *
 * WHAT TO WATCH IN KEIL (right-click each name -> Add to Watch 1):
 *     pir_state     what the pin reads RIGHT NOW.  0 = low, 1 = high.
 *     pir_events    how many times motion has started. Goes up by 1 per wave.
 *     pir_low_seen  0 until the pin has been low at least once, then stays 1.
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

/* ---- the three variables to watch ---- */
volatile uint8_t  pir_state    = 0;   /* live level of PB5: 0 or 1          */
volatile uint32_t pir_events   = 0;   /* number of times motion started     */
volatile uint8_t  pir_low_seen = 0;   /* 1 once the pin has ever read low   */

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

    /* Start from whatever the pin is doing now, so a pin that is already high
     * does not get counted as a motion event before anything has moved. */
    prev = gpio_read(PIR_PORT, PIR_PIN);

    for (;;) {
        uint8_t now = gpio_read(PIR_PORT, PIR_PIN);   /* read the pin */

        pir_state = now;
        if (!now) pir_low_seen = 1;                   /* remember we saw a low */
        if (now && !prev) pir_events++;               /* count only the moment it goes high */
        prev = now;

        led_set(now);                                 /* LED copies the sensor */
        delay_ms(10);
    }
}
