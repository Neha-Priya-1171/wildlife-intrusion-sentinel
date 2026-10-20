/**
 * @file    test_buzzer.c
 * @brief   Buzzer test for a small 12 mm buzzer driven DIRECTLY from PA6 - no transistor.
 *
 * WIRING (two wires, that is all):
 *     buzzer  +  (the marked leg, or the longer one)  -->  PA6
 *     buzzer  -  (the other leg)                      -->  GND
 *
 * For the very first power-up you may put a 270 ohm resistor in series with the + leg.
 * It will sound quiet and thin, but nothing can be stressed while you find out which
 * buzzer is which. Once it beeps, take the resistor out and wire PA6 straight to + for
 * full volume. The final build does NOT want the resistor.
 *
 * ACTIVE OR PASSIVE? That is what this test tells you.
 *     An ACTIVE buzzer has its own oscillator: switch the pin on and it makes a TONE.
 *     A PASSIVE buzzer has none: switching the pin on and off only makes a CLICK.
 * Run the test as shipped and listen to PART 1:
 *     you hear three clear BEEPS      -> active buzzer, you are done, leave this file alone
 *     you hear three faint CLICKS     -> passive buzzer, set BUZZER_IS_PASSIVE to 1 below,
 *                                        rebuild, and it will sing properly
 *
 * The spare buzzer in the kit can be wired to PB0 as a second sounder. Two buzzers must
 * never share one pin: that doubles the current past what the pin can give. Wire it to
 * PB0 and set BUZ2_ENABLE to 1 in board.h.
 *
 * WHAT YOU SHOULD HEAR
 *     PART 1  three separate 300 ms beeps                  - identifies active vs passive
 *     PART 2  rising pitch sweep (passive mode only)       - skipped if active
 *     PART 3  the deterrent cadence used by the real alarm - six seconds, varying rhythm
 *     then a three second silence, and the whole thing repeats.
 * The green LED on PA5 is on whenever the buzzer is meant to be sounding, so you can see
 * the test working even somewhere noisy.
 *
 * Files to build: test_buzzer.c + common/board.c
 */
#include "board.h"

/* 0 = active buzzer (has its own oscillator - just switch it on).
 * 1 = passive buzzer (no oscillator - this file generates the tone in software). */
#define BUZZER_IS_PASSIVE   0

/* Tone used in passive mode. 2.7 kHz is near the loudest point for these 12 mm parts. */
#define TONE_HZ          2700u

/* ---------------------------------------------------------------------------
 * Rough microsecond delay. The core runs at 16 MHz and this loop is about four
 * cycles per pass, so four passes per microsecond. It is not accurate enough for
 * timing a protocol, but a buzzer does not care whether the note is 2.7 or 2.6 kHz.
 * ------------------------------------------------------------------------- */
#if BUZZER_IS_PASSIVE
static void delay_us(uint32_t us)
{
    volatile uint32_t n = us * 4u;
    while (n--) { __NOP(); }
}
#endif

/* ---------------- the two pins ---------------- */
static void buz_write(uint8_t on)
{
    gpio_write(BUZ_PORT, BUZ_PIN, on);
#if BUZ2_ENABLE
    gpio_write(BUZ2_PORT, BUZ2_PIN, on);
#endif
}

static void buzzer_init(void)
{
    buz_write(0);                                   /* silent before the pin drives */
    gpio_mode(BUZ_PORT, BUZ_PIN, BRD_MODE_OUT);
#if BUZ2_ENABLE
    gpio_mode(BUZ2_PORT, BUZ2_PIN, BRD_MODE_OUT);
#endif
}

/* Sound the buzzer for ms milliseconds.
 * Active: hold the pin high and wait.
 * Passive: toggle the pin at TONE_HZ for the whole duration to make the tone. */
static void sound(uint32_t ms)
{
    led_set(1);
#if BUZZER_IS_PASSIVE
    {
        uint32_t half = 500000u / TONE_HZ;          /* half period, microseconds */
        uint32_t cycles = (ms * 1000u) / (half * 2u);
        while (cycles--) {
            buz_write(1); delay_us(half);
            buz_write(0); delay_us(half);
        }
    }
#else
    buz_write(1);
    delay_ms(ms);
    buz_write(0);
#endif
    led_set(0);
}

static void quiet(uint32_t ms)
{
    buz_write(0);
    delay_ms(ms);
}

#if BUZZER_IS_PASSIVE
/* Pitch sweep - only meaningful on a passive buzzer, where we control the frequency. */
static void sweep(uint32_t from_hz, uint32_t to_hz, uint32_t ms)
{
    uint32_t steps = 40u, i;
    led_set(1);
    for (i = 0; i < steps; i++) {
        uint32_t hz   = from_hz + (to_hz - from_hz) * i / steps;
        uint32_t half = 500000u / hz;
        uint32_t cyc  = (ms * 1000u / steps) / (half * 2u);
        while (cyc--) {
            buz_write(1); delay_us(half);
            buz_write(0); delay_us(half);
        }
    }
    led_set(0);
}
#endif

int main(void)
{
    board_init();
    led_init();
    buzzer_init();

    for (;;) {
        /* ---- PART 1: three plain beeps. BEEPS = active, CLICKS = passive. ---- */
        for (int i = 0; i < 3; i++) {
            sound(300);
            quiet(400);
        }
        quiet(900);

#if BUZZER_IS_PASSIVE
        /* ---- PART 2: pitch sweep, proving software tone generation works ---- */
        sweep(800u, 3200u, 900u);
        quiet(900);
#endif

        /* ---- PART 3: the real deterrent cadence from the alarm firmware.
         * The rhythm keeps changing on purpose - a constant tone is something
         * animals quickly learn to ignore. ---- */
        {
            uint32_t t0 = millis();
            uint32_t step = 0;
            while ((millis() - t0) < 6000u) {
                sound(60u + 40u * (step % 4u));     /* 60, 100, 140, 180 ms */
                quiet(200u - 40u * (step % 4u));    /* 200, 160, 120,  80 ms */
                step++;
            }
        }

        quiet(3000);
    }
}
