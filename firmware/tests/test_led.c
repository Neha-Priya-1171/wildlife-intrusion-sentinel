/**
 * @file    test_led.c
 * @brief   TEST 1 - Green status LED on PA5 (blink, then steady "SECURE").
 * Wiring: PA5 --> 220R --> LED anode (long leg); LED cathode (short leg) --> GND.
 * Expect: 5 quick blinks, then LED solid ON for 3 s ("SECURE"), forever repeating.
 * Files to build: test_led.c + common/board.c
 */
#include "board.h"

int main(void)
{
    board_init();
    led_init();

    for (;;) {
        led_blink(5, 150, 150);   /* "I'm alive" */
        led_set(1);               /* SECURE state = LED steady ON */
        delay_ms(3000);
        led_set(0);
        delay_ms(500);
    }
}
