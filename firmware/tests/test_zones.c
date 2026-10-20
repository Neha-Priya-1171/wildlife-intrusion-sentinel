/**
 * @file    test_zones.c
 * @brief   Checks the two-zone indicators: one LED and one buzzer per side of the farm.
 *
 * THE IDEA: the farm is split in two, one zone along each long edge of the board.
 * The LED is what tells you WHICH SIDE was breached. The buzzers only make noise. They
 * are kept separate on purpose, so that a dead buzzer costs you volume but never costs
 * you the information about where the animal came in.
 *
 * WIRING
 *     Zone A LED  +  --> 220 ohm (or 270) --> PA3 ;  LED -  --> GND
 *     Zone B LED  +  --> 220 ohm (or 270) --> PA8 ;  LED -  --> GND
 *     Zone A buzzer  + --> PA6 ;  - --> GND
 *     Zone B buzzer  + --> PB0 ;  - --> GND     (optional - BUZ2_ENABLE in board.h)
 *
 * The LED long leg is +. Both zone LEDs are on PORT A deliberately: port A is known
 * good on this board, so zone indication does not depend on the port that has been
 * giving trouble.
 *
 * NOTHING FITTED YET? Set ZONE_LEDS_ENABLE to 0 in board.h and the LED calls become
 * no-ops; set BUZ2_ENABLE to 0 if the PB0 buzzer is not working. The test still runs.
 *
 * WHAT YOU SHOULD SEE AND HEAR, from reset
 *     PART 1  zone A LED alone, three slow flashes          - is PA3 wired right?
 *     PART 2  zone B LED alone, three slow flashes          - is PA8 wired right?
 *     PART 3  both LEDs together, two flashes               - both good
 *     PART 4  simulated breach in ZONE A:
 *               zone A LED SOLID for 4 s, buzzers sounding the alarm cadence
 *     PART 5  simulated breach in ZONE B:
 *               zone B LED SOLID for 4 s, buzzers sounding the alarm cadence
 *     then three seconds of quiet, and the cycle repeats.
 *
 * Parts 4 and 5 are the important ones. The buzzers sound identically in both - only
 * the LED changes - which is exactly the behaviour the real firmware has.
 *
 * The PA5 status LED blinks fast during a simulated breach, as it does in the real
 * firmware, so you can tell the two phases apart at a glance.
 *
 * Files to build: test_zones.c + common/board.c
 */
#include "board.h"

#define BREACH_MS   4000u

/* The deterrent cadence copied from the application firmware: the rhythm keeps
 * shifting, because a constant tone is something animals learn to ignore. */
static void breach(uint8_t zone, uint32_t ms)
{
    uint32_t t0   = millis();
    uint32_t step = 0;

    zone_leds_off();
    zone_led_set(zone, 1);                       /* SOLID: this is the breached side */

    while ((millis() - t0) < ms) {
        uint32_t on  = 60u + 40u * (step % 4u);  /* 60, 100, 140, 180 ms */
        uint32_t off = 200u - 40u * (step % 4u); /* 200, 160, 120,  80 ms */
        led_set(1);  buzzer_set(1); delay_ms(on);
        led_set(0);  buzzer_set(0); delay_ms(off);
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
    led_init();
    buzzer_init();
    zone_leds_init();

    for (;;) {
        /* ---- PART 1: zone A LED on PA3, on its own ---- */
        flash(1, 0, 3);
        delay_ms(800);

        /* ---- PART 2: zone B LED on PA8, on its own ----
         * If part 1 lit and this did not, the fault is PA8's wiring, not the code. */
        flash(0, 1, 3);
        delay_ms(800);

        /* ---- PART 3: both together ---- */
        flash(1, 1, 2);
        delay_ms(1200);

        /* ---- PART 4: a breach on side A ---- */
        breach(ZONE_A, BREACH_MS);
        delay_ms(1500);

        /* ---- PART 5: a breach on side B.
         * Listen: the buzzers do exactly the same thing as in part 4. Only the LED
         * moves to the other side. That is the whole point of the design - the sound
         * says "something happened", the LED says "it happened over there". ---- */
        breach(ZONE_B, BREACH_MS);

        delay_ms(3000);
    }
}
