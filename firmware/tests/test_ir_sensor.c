/**
 * @file    test_ir_sensor.c
 * @brief   TEST 2 - Active IR break-beam sensor on PA0 using the EXTI0 interrupt (NVIC).
 * Wiring (discrete pair): see docs/assembly_guide.md, Stage 2. Receiver stage output --> PA0 ; GND common.
 * Expect: LED (PA5) steady ON while beam is clear. Break the beam -> the ISR fires
 *         instantly, main() blinks the LED 6 times, then returns to steady ON.
 * Debugger tip: add 'breach_count' to the Watch window - it increments per break.
 * Files to build: test_ir_sensor.c + common/board.c
 */
#include "board.h"

#define DEBOUNCE_MS  200u

static volatile uint8_t  breach_flag  = 0;
static volatile uint32_t breach_count = 0;
static volatile uint32_t last_ms      = 0;

/* Interrupt Service Routine - name must match the startup file vector table */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;                 /* clear pending bit (write 1) */
        uint32_t now = millis();
        if ((now - last_ms) > DEBOUNCE_MS) {    /* ignore contact bounce/noise */
            last_ms = now;
            breach_count++;
            breach_flag = 1;
        }
    }
}

static void ir_exti_init(void)
{
    gpio_mode(IR_PORT, IR_PIN, BRD_MODE_IN);
#if IR_ACTIVE_LOW
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_UP);    /* idle HIGH, break = falling edge */
#elif IR_EXT_PULLUP
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_NONE);  /* external 10k pull-up on the NPN collector */
#else
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_DOWN);
#endif

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;       /* SYSCFG clock needed to route EXTI lines */
    (void)RCC->APB2ENR;
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0; /* EXTI0 <- port A (0b0000) */

    EXTI->IMR  |= EXTI_IMR_MR0;                 /* unmask line 0 */
#if IR_ACTIVE_LOW
    EXTI->FTSR |= EXTI_FTSR_TR0;  EXTI->RTSR &= ~EXTI_RTSR_TR0;
#else
    EXTI->RTSR |= EXTI_RTSR_TR0;  EXTI->FTSR &= ~EXTI_FTSR_TR0;
#endif
    EXTI->PR = EXTI_PR_PR0;                     /* clear any stale flag */

    NVIC_SetPriority(EXTI0_IRQn, 0);            /* highest priority: perimeter breach first */
    NVIC_EnableIRQ(EXTI0_IRQn);
}

int main(void)
{
    board_init();
    led_init();
    ir_exti_init();
    led_set(1);                                  /* SECURE */

    for (;;) {
        if (breach_flag) {
            breach_flag = 0;
            led_blink(6, 100, 100);              /* BREACH indication */
            led_set(1);
        }
        __WFI();                                 /* sleep until next interrupt (SysTick/EXTI) */
    }
}
