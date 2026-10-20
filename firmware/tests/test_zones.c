/**
 * @file    test_zones.c
 * @brief   Checks the two-zone indicators: one LED and one buzzer per side of the farm.
 *
 * THE IDEA: the farm is split in two, one zone along each long edge of the board.
 * The LED says WHICH SIDE was breached. The buzzers only make noise, and every fitted
 * buzzer sounds for either zone. They are kept separate on purpose, so a dead buzzer
 * costs volume but never costs the information about where the animal came in.
 *
 * WIRING - only ONE new LED is needed
 *     Zone A LED  =  the green status LED already on PA5. Nothing new to wire.
 *     Zone B LED  =  PA8 -> 270 ohm -> LED long leg ; LED short leg -> GND
 *     Zone A buzzer  + --> PA6 ;  - --> GND
 *     Zone B buzzer  + --> PB0 ;  - --> GND    (optional - BUZ2_ENABLE in board.h)
 *
 * PA5 DOES TWO JOBS. It is the status LED and the zone A indicator, so this test never
 * calls led_set(): every LED decision goes through zone_led_set(), exactly as the
 * application firmware does it. Mixing the two would make the pin flicker nonsensically.
 *
 * WHAT SOLID MEANS: a steadily lit LED means "this zone is breached", and nothing else.
 * That is why the application shows SECURE as a brief heartbeat flash rather than a
 * steady light - so steady always has one meaning.
 *
 * NOTHING FITTED YET? Set ZONE_LEDS_ENABLE to 0 in board.h and the LED calls become
 * no-ops; set BUZ2_ENABLE to 0 if the PB0 buzzer is not working. The test still runs.
 *
 * WHAT YOU SHOULD SEE AND HEAR, from reset
 *     PART 1  zone A LED (PA5) alone, three slow flashes     - the status LED responds
 *     PART 2  zone B LED (PA8) alone, three slow flashes     - is the new LED wired?
 *     PART 3  both together, two flashes                     - both good
 *     PART 4  simulated breach in ZONE A:
 *               PA5 SOLID for 4 s, PA8 dark, buzzers sounding the alarm cadence
 *     PART 5  simulated breach in ZONE B:
 *               PA8 SOLID for 4 s, PA5 dark, buzzers sounding the alarm cadence
 *     then three seconds of quiet, and the cycle repeats.
 *
 * Parts 4 and 5 are the important ones. The buzzers do exactly the same thing in both.
 * Only the solid LED moves to the other side. That is the design working.
 *
 * Files to build: test_zones.c + common/board.c
 */
#include "board.h"

#define BREACH_MS   4000u

/* The deterrent cadence copied from the application firmware: the rhythm keeps
 * shifting, because a constant tone is something animals learn to ignore.
 * The breached zone's LED stays SOLID throughout - it must not blink with the
 * buzzer, or it would stop being a clear "this side" indicator. */
static void breach(uint8_t zone, uint32_t ms)
{
    uint32_t t0   = millis();
    uint32_t step = 0;

    zone_leds_off();
    zone_led_set(zone, 1);                       /* SOLID: this is the breached side */

    while ((millis() - t0) < ms) {
        uint32_t on  = 60u + 40u * (step % 4u);  /* 60, 100, 140, 180 ms */
        uint32_t off = 200u - 40u * (step % 4u); /* 200, 160, 120,  80 ms */
        buzzer_set(1); delay_ms(on);
        buzzer_set(0); delay_ms(off);
        step++;
    }

    buzzer_set(0);
    zone_leds_off();
}

static void flash(uint8_t zone_a, uint8_t zone_b, uint8_t times)
{
    while (times--) {
        if (zone_a) zone_led_set(ZONE_A, 1);
        if (zone_b) zone_led_set(ZONE_B, 1);
        delay_ms(400);
        zone_leds_off();
        delay_ms(400);
    }
}

int main(void)
{
    board_init();
    led_init();            /* PA5 as an output; from here on only zone_led_set drives it */
    buzzer_init();
    zone_leds_init();

    for (;;) {
        /* ---- PART 1: zone A, which is the existing status LED on PA5 ---- */
        flash(1, 0, 3);
        delay_ms(800);

        /* ---- PART 2: zone B on PA8, the one new LED ----
         * If part 1 flashed and this did not, the fault is PA8's wiring, not the code. */
        flash(0, 1, 3);
        delay_ms(800);

        /* ---- PART 3: both together ---- */
        flash(1, 1, 2);
        delay_ms(1200);

        /* ---- PART 4: a breach on side A ---- */
        breach(ZONE_A, BREACH_MS);
        delay_ms(1500);

        /* ---- PART 5: a breach on side B.
         * Listen: the buzzers do exactly what they did in part 4. Only the solid LED
         * moves. The sound says "something happened", the LED says "over there". ---- */
        breach(ZONE_B, BREACH_MS);

        delay_ms(3000);
    }
}
