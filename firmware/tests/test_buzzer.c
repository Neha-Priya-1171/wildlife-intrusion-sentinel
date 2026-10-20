/**
 * @file    test_buzzer.c
 * @brief   Buzzer test. Two 12 mm buzzers, each driven DIRECTLY from its own GPIO pin.
 *          No transistor, no relay, no 5 V - the pins do the work.
 *
 * WIRING (four wires total):
 *     buzzer 1  +  -->  PA6          buzzer 2  +  -->  PB0
 *     buzzer 1  -  -->  GND          buzzer 2  -  -->  GND
 *
 * The + leg is the marked one, or the longer one. Both buzzers share the board's GND.
 *
 * WHY TWO PINS: a 12 mm buzzer pulls roughly 20 mA at 3.3 V. An STM32F401 pin is rated
 * for 25 mA, so one buzzer per pin is fine but two on PA6 would be about 40 mA and over
 * the limit. Never join them onto one pin.
 *
 * ONE BUZZER ONLY? Set BUZ2_ENABLE to 0 in board.h and ignore PB0. Everything else here
 * still works; PART 2 and PART 3 are simply skipped.
 *
 * ACTIVE OR PASSIVE? An ACTIVE buzzer has its own oscillator, so switching the pin on
 * gives a TONE. A PASSIVE one has none and only CLICKS. The firmware just switches pins,
 * so you want active. Run this test and listen to each buzzer in turn:
 *     clear beeps  -> active, nothing to change
 *     faint clicks -> passive; set BUZZER_IS_PASSIVE to 1 below and rebuild, and this
 *                     file will generate the tone in software instead
 *
 * WHAT YOU SHOULD HEAR, from reset
 *     PART 1   buzzer 1 alone, three 300 ms beeps                 - is PA6 working?
 *     PART 2   buzzer 2 alone, three 300 ms beeps                 - is PB0 working?
 *     PART 3   both together, two long 500 ms blasts              - the full deterrent
 *     PART 4   the real alarm cadence, both buzzers, six seconds
 *     then three seconds of silence, and the whole cycle repeats.
 *
 * The green LED on PA5 is on whenever something should be sounding, so the test can be
 * read by eye in a noisy room. If the LED follows the pattern and you hear nothing, the
 * firmware is fine and the fault is in the buzzer or its two wires.
 *
 * Files to build: test_buzzer.c + common/board.c
 */
#include "board.h"

/* 0 = active buzzer (own oscillator - just switch it on).
 * 1 = passive buzzer (no oscillator - this file makes the tone in software). */
#define BUZZER_IS_PASSIVE   0

/* Tone used in passive mode. 2.7 kHz is near the loudest point for these 12 mm parts. */
#define TONE_HZ          2700u

/* Which buzzer(s) a step should drive. */
#define B1   0x01u
#define B2   0x02u
#define BOTH (B1 | B2)

#if BUZZER_IS_PASSIVE
/* Rough microsecond delay. The core runs at 16 MHz and this loop is about four cycles a
 * pass, so four passes per microsecond. Not accurate enough to time a protocol, but a
 * buzzer does not care whether the note is 2.7 or 2.6 kHz. */
static void delay_us(uint32_t us)
{
    volatile uint32_t n = us * 4u;
    while (n--) { __NOP(); }
}
#endif

/* Drive one or both buzzers. board.c owns buzzer_set(), which always drives both, so
 * for the single-buzzer parts of the test we poke the pins directly. */
static void drive(uint8_t which, uint8_t on)
{
    if (which & B1) gpio_write(BUZ_PORT, BUZ_PIN, on);
#if BUZ2_ENABLE
    if (which & B2) gpio_write(BUZ2_PORT, BUZ2_PIN, on);
#else
    (void)which;
#endif
}

static void sound(uint8_t which, uint32_t ms)
{
    led_set(1);
#if BUZZER_IS_PASSIVE
    {
        uint32_t half   = 500000u / TONE_HZ;        /* half period, microseconds */
        uint32_t cycles = (ms * 1000u) / (half * 2u);
        while (cycles--) {
            drive(which, 1); delay_us(half);
            drive(which, 0); delay_us(half);
        }
    }
#else
    drive(which, 1);
    delay_ms(ms);
    drive(which, 0);
#endif
    led_set(0);
}

static void quiet(uint32_t ms)
{
    buzzer_set(0);                                  /* both off, whatever was sounding */
    delay_ms(ms);
}

int main(void)
{
    board_init();
    led_init();
    buzzer_init();                                  /* both pins low, then outputs */

    for (;;) {
        /* ---- PART 1: buzzer 1 on PA6, on its own ---- */
        for (int i = 0; i < 3; i++) {
            sound(B1, 300);
            quiet(400);
        }
        quiet(1200);

#if BUZ2_ENABLE
        /* ---- PART 2: buzzer 2 on PB0, on its own ----
         * If PART 1 sounded and this does not, the fault is PB0's wiring, not the code. */
        for (int i = 0; i < 3; i++) {
            sound(B2, 300);
            quiet(400);
        }
        quiet(1200);

        /* ---- PART 3: both together - this is what the alarm will sound like ---- */
        sound(BOTH, 500);
        quiet(300);
        sound(BOTH, 500);
        quiet(1200);
#endif

        /* ---- PART 4: the real deterrent cadence from the alarm firmware.
         * The rhythm keeps shifting on purpose - a constant tone is something
         * animals learn to ignore. ---- */
        {
            uint32_t t0   = millis();
            uint32_t step = 0;
            while ((millis() - t0) < 6000u) {
                sound(BOTH, 60u + 40u * (step % 4u));   /* 60, 100, 140, 180 ms */
                quiet(200u - 40u * (step % 4u));        /* 200, 160, 120,  80 ms */
                step++;
            }
        }

        quiet(3000);
    }
}
