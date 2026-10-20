/**
 * @file    test_siren.c
 * @brief   TEST 8 - 12 V mini strobe siren (A-12-B) switched by the relay on PA7.
 *
 * This is NOT a duplicate of test_actuators.c. That test exercises the relay DRY, with
 * nothing on the contacts. This one is the first test that puts a real 12 V load on them,
 * so it is deliberately cautious: it arms slowly, pulses briefly with long gaps so you can
 * watch an ammeter and abort, and then stops instead of running the siren continuously.
 *
 * Hardware: the siren is NEVER driven from a GPIO pin or from the MB102 rails. The relay
 * contacts switch the 12 V adapter directly. See docs/assembly_guide.md Stage 8 for the
 * wiring table and the mandatory pre-power checks.
 *
 * Pin: PA7 (RELAY_PIN) - the existing relay output from the master pin map. No new pin.
 *
 * Expect: LED blinks for 5 s (arming, siren silent) -> 3 short bursts of 300 ms separated
 *         by 3 s of silence -> one 2 s burst -> relay OFF and the auto sequence stops.
 *         The program then sits in MANUAL mode doing nothing until you command it.
 *
 * Manual ON/OFF from the Keil Watch window: add `siren_cmd` to Watch and write 1 to switch
 *         the siren ON, 0 to switch it OFF. `siren_on` reports the state actually applied.
 *
 * Files to build: test_siren.c + common/board.c
 */
#include "board.h"

/* ---------------------------------------------------------------------------
 * Timing. Kept conservative on purpose - this is a loud device being switched
 * for the first time with a live 12 V load on the relay contacts.
 * ------------------------------------------------------------------------- */
#define SIREN_ARM_MS        5000u   /* silent arming window: time to step back / watch meter */
#define SIREN_PULSE_MS       300u   /* length of each short confirmation burst              */
#define SIREN_GAP_MS        3000u   /* silence between bursts: time to read current / abort */
#define SIREN_PULSES            3u  /* number of short bursts                               */
#define SIREN_LONG_MS       2000u   /* final, longer burst                                  */

/* Safety guard for MANUAL mode: if siren_cmd is left at 1 and forgotten, the siren is
 * switched off automatically after this long. Raise it if you need a longer soak test.
 * This protects your ears and the adapter; it does not mask any hardware fault. */
#define SIREN_MAX_ON_MS    30000u

/* ---- Watch these in Keil ---- */
volatile uint8_t  siren_cmd      = 0;   /* WRITE from Watch: 1 = ON, 0 = OFF (manual mode) */
volatile uint8_t  siren_on       = 0;   /* read-back: state actually driven onto the relay */
volatile uint8_t  siren_auto_done = 0;  /* 1 once the automatic sequence has finished      */
volatile uint32_t siren_on_count = 0;   /* how many times the relay has been energised     */
volatile uint32_t siren_on_ms    = 0;   /* cumulative ms the siren has been ON             */
volatile uint8_t  siren_guard_tripped = 0; /* 1 if SIREN_MAX_ON_MS forced an OFF           */

/* ----- Relay drive -----
 * Identical to the scheme in test_actuators.c, deliberately: an active-low opto board is
 * driven OPEN-DRAIN, so "off" releases the pin (high-Z) and "on" pulls it to GND. A 3.3 V
 * logic high is not reliably high enough for an input referenced to the module's 5 V rail,
 * which can leave the relay half-energised. Sinking to GND avoids that entirely.
 * The OFF level is loaded BEFORE the pin becomes an output, so the relay cannot twitch -
 * and with a siren on the contacts, a twitch at reset is a bang. */
static void relay_set(uint8_t on)
{
#if RELAY_ACTIVE_LOW
    gpio_write(RELAY_PORT, RELAY_PIN, on ? 0 : 1);
#else
    gpio_write(RELAY_PORT, RELAY_PIN, on ? 1 : 0);
#endif
}

static void relay_init(void)
{
    relay_set(0);                                   /* preload OFF before driving the pin */
#if RELAY_ACTIVE_LOW
    gpio_open_drain(RELAY_PORT, RELAY_PIN, 1);
#endif
    gpio_mode(RELAY_PORT, RELAY_PIN, BRD_MODE_OUT);
}

/* Single point of control, so siren_on always reflects what the relay was told to do. */
static void siren_set(uint8_t on)
{
    if (on && !siren_on) siren_on_count++;
    siren_on = on ? 1u : 0u;
    relay_set(siren_on);
    led_set(siren_on);                              /* LED mirrors the siren for a quiet dry run */
}

static void siren_burst(uint32_t on_ms, uint32_t off_ms)
{
    siren_set(1); delay_ms(on_ms);  siren_on_ms += on_ms;
    siren_set(0); delay_ms(off_ms);
}

int main(void)
{
    uint32_t manual_since = 0;

    board_init();
    led_init();
    relay_init();                                   /* relay is OFF from this instant on */

    /* Arming window: siren stays silent. The blinking LED says the program is running,
     * which lets you confirm the firmware works before anything gets loud. */
    {
        uint32_t t0 = millis();
        while ((millis() - t0) < SIREN_ARM_MS) {
            led_blink(1, 100, 400);
        }
        led_set(0);
    }

    /* Short bursts with long gaps: enough to prove the relay switches the load, short
     * enough to be harmless, and spaced so you can read a meter or pull power. */
    for (uint32_t i = 0; i < SIREN_PULSES; i++) {
        siren_burst(SIREN_PULSE_MS, SIREN_GAP_MS);
    }

    /* One longer burst to confirm the strobe and the sounder both sustain. */
    siren_burst(SIREN_LONG_MS, 0);

    siren_set(0);
    siren_auto_done = 1;

    /* MANUAL mode. The siren is off and stays off until you write 1 to siren_cmd in the
     * Keil Watch window. This is the "simple ON/OFF" control - no reflash needed. */
    for (;;) {
        if (siren_cmd && !siren_on) {
            manual_since = millis();
            siren_set(1);
        } else if (!siren_cmd && siren_on) {
            siren_set(0);
        }

        if (siren_on) {
            siren_on_ms += 10u;
            if ((millis() - manual_since) > SIREN_MAX_ON_MS) {
                siren_guard_tripped = 1;
                siren_cmd = 0;                      /* clear the command so it stays off */
                siren_set(0);
            }
        }

        delay_ms(10);
    }
}
