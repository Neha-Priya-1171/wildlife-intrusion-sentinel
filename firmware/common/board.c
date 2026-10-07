/** @file board.c - implementation of the helpers declared in board.h */
#include "board.h"

static volatile uint32_t ms_ticks = 0;

void SysTick_Handler(void)
{
    ms_ticks++;
}

void board_init(void)
{
    SystemCoreClockUpdate();                       /* 16 MHz (HSI) unless you changed the clock */
    SysTick_Config(SystemCoreClock / 1000u);       /* 1 ms tick */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;                            /* dummy read: let the clock settle */
}

uint32_t millis(void) { return ms_ticks; }

void delay_ms(uint32_t ms)
{
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms) { __NOP(); }
}

void gpio_mode(GPIO_TypeDef *p, uint8_t pin, uint32_t mode)
{
    p->MODER = (p->MODER & ~(3u << (pin * 2u))) | (mode << (pin * 2u));
}

void gpio_pull(GPIO_TypeDef *p, uint8_t pin, uint32_t pull)
{
    p->PUPDR = (p->PUPDR & ~(3u << (pin * 2u))) | (pull << (pin * 2u));
}

void gpio_af(GPIO_TypeDef *p, uint8_t pin, uint32_t af)
{
    uint32_t idx = pin >> 3, sh = (pin & 7u) * 4u;
    p->AFR[idx] = (p->AFR[idx] & ~(0xFu << sh)) | (af << sh);
}

void gpio_open_drain(GPIO_TypeDef *p, uint8_t pin, uint8_t enable)
{
    if (enable) p->OTYPER |=  (1u << pin);
    else        p->OTYPER &= ~(1u << pin);
}

void gpio_write(GPIO_TypeDef *p, uint8_t pin, uint8_t level)
{
    p->BSRR = level ? (1u << pin) : (1u << (pin + 16u));   /* atomic set/reset */
}

uint8_t gpio_read(GPIO_TypeDef *p, uint8_t pin) { return (uint8_t)((p->IDR >> pin) & 1u); }

void led_init(void)
{
    gpio_write(LED_PORT, LED_PIN, 0);
    gpio_mode(LED_PORT, LED_PIN, BRD_MODE_OUT);
}

void led_set(uint8_t on) { gpio_write(LED_PORT, LED_PIN, on); }

void led_blink(uint8_t times, uint32_t on_ms, uint32_t off_ms)
{
    while (times--) {
        led_set(1); delay_ms(on_ms);
        led_set(0); delay_ms(off_ms);
    }
}

void board_error_blink(uint8_t code)
{
    led_init();
    for (;;) {
        led_blink(code, 100, 150);
        delay_ms(1200);
    }
}
