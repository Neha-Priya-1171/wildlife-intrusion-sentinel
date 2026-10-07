/**
 * @file    test_ldr.c   (BONUS test - not in the original six)
 * @brief   LDR module analog output on PA4 -> ADC1 channel 4 (12-bit).
 * Wiring: LDR module AO --> PA4 ; module VCC from the 3.3 V rail so AO can never exceed 3.3 V.
 * Expect: LED steady ON when it is DARK (reading above DARK_THRESHOLD), OFF when bright.
 *         Cover the LDR with a finger to test. Watch 'adc_value' (0-4095) in the debugger.
 *         NOTE: many modules read HIGHER when darker; if yours is reversed, flip the compare.
 * Files to build: test_ldr.c + common/board.c
 */
#include "board.h"

#define DARK_THRESHOLD  2500u

volatile uint16_t adc_value = 0;

static void adc1_init(void)
{
    gpio_mode(LDR_PORT, LDR_PIN, BRD_MODE_ANALOG);
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    ADC1->SMPR2 |= (7u << (3u * 4u));            /* channel 4: 480 cycles (slow, stable)   */
    ADC1->SQR1   = 0;                            /* 1 conversion in the sequence           */
    ADC1->SQR3   = 4u;                           /* first conversion = channel 4 (PA4)     */
    ADC1->CR2   |= ADC_CR2_ADON;                 /* power on ADC                           */
    delay_ms(2);
}

static uint16_t adc1_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;                /* start conversion */
    while (!(ADC1->SR & ADC_SR_EOC)) { }
    return (uint16_t)ADC1->DR;                   /* reading DR clears EOC */
}

int main(void)
{
    board_init();
    led_init();
    adc1_init();

    for (;;) {
        uint32_t sum = 0;
        for (int i = 0; i < 16; i++) { sum += adc1_read(); }   /* average to reduce noise */
        adc_value = (uint16_t)(sum / 16u);
        led_set(adc_value > DARK_THRESHOLD);
        delay_ms(100);
    }
}
