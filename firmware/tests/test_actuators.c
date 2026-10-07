/**
 * @file    test_actuators.c
 * @brief   TEST 6 - Active buzzer (PA6 via NPN) and 5 V relay module (PA7).
 * Wiring: PA6 --> 1k --> NPN base ; NPN emitter --> GND ; buzzer(-) --> NPN collector ;
 *         buzzer(+) --> 5 V.   PA7 --> relay IN ; relay VCC 5 V, GND common.
 *         Relay output: COM/NO switches the lamp's OWN supply (never route lamp current
 *         through the MB102 - see README). Start the test with NOTHING on the relay contacts;
 *         you should simply hear/see the relay click.
 * Expect: 1) 3 short beeps  2) relay clicks ON/OFF 3 times  3) "breach" demo: relay ON
 *         + changing beep cadence for 6 s (varying pattern deters habituation)  4) pause, repeat.
 * Files to build: test_actuators.c + common/board.c
 */
#include "board.h"

/* ----- Buzzer ----- */
static void buzzer_init(void) { gpio_write(BUZ_PORT, BUZ_PIN, 0); gpio_mode(BUZ_PORT, BUZ_PIN, BRD_MODE_OUT); }
static void buzzer(uint8_t on) { gpio_write(BUZ_PORT, BUZ_PIN, on); }

static void beep(uint32_t on_ms, uint32_t off_ms)
{
    buzzer(1); delay_ms(on_ms);
    buzzer(0); delay_ms(off_ms);
}

/* ----- Relay -----
 * Active-low boards are driven OPEN-DRAIN: "off" = pin released (high-impedance), "on" = pulled low.
 * This avoids the classic problem where a 3.3 V "high" is not high enough for a 5 V-powered
 * opto input and the relay stays half-on. The pin is set to the OFF state BEFORE it becomes an
 * output, so the relay does not twitch at start-up. */
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
    relay_set(0);                                         /* preload OFF level */
#if RELAY_ACTIVE_LOW
    gpio_open_drain(RELAY_PORT, RELAY_PIN, 1);
#endif
    gpio_mode(RELAY_PORT, RELAY_PIN, BRD_MODE_OUT);
}

int main(void)
{
    board_init();
    led_init();
    relay_init();
    buzzer_init();

    for (;;) {
        /* 1) buzzer test */
        led_set(1);
        for (int i = 0; i < 3; i++) beep(150, 150);
        led_set(0);
        delay_ms(1000);

        /* 2) relay click test */
        for (int i = 0; i < 3; i++) {
            relay_set(1); delay_ms(800);
            relay_set(0); delay_ms(800);
        }
        delay_ms(1000);

        /* 3) combined breach demo with varying cadence */
        relay_set(1);
        uint32_t t0 = millis();
        uint32_t step = 0;
        while ((millis() - t0) < 6000u) {
            uint32_t on  = 60u + 40u * (step % 4u);        /* 60,100,140,180 ms */
            uint32_t off = 200u - 40u * (step % 4u);       /* 200,160,120,80 ms */
            beep(on, off);
            step++;
        }
        relay_set(0);
        buzzer(0);
        delay_ms(3000);
    }
}
