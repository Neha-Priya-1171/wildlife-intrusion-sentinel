/**
 * @file    test_bluetooth.c
 * @brief   TEST 5 - HC-05 Bluetooth telemetry on USART1 (PA9 TX, PA10 RX) @ 9600 8N1.
 * Wiring (CROSSED): STM32 PA9 (TX) --> HC-05 RXD ; STM32 PA10 (RX) <-- HC-05 TXD ;
 *         HC-05 VCC = 5 V, GND common. HC-05 EN/KEY pin left unconnected (data mode).
 * Phone: pair with "HC-05" (PIN 1234 or 0000) and open an Android app such as
 *        "Serial Bluetooth Terminal". (iPhones cannot use classic-Bluetooth HC-05.)
 * Expect: a line every second: "SWIS,SEQ=<n>,STATUS=SECURE". Type '1' / '0' in the app to
 *         switch the PA5 LED on/off, '?' for help, 'B' to send a sample breach alert.
 * Files to build: test_bluetooth.c + common/board.c
 */
#include "board.h"

#define BAUD  9600u

static void uart1_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    gpio_af(GPIOA, 9, 7);  gpio_af(GPIOA, 10, 7);         /* AF7 = USART1 */
    gpio_mode(GPIOA, 9, BRD_MODE_AF);
    gpio_mode(GPIOA, 10, BRD_MODE_AF);
    gpio_pull(GPIOA, 10, BRD_PULL_UP);                    /* keep RX idle-high if HC-05 is off */

    /* APB2 = 16 MHz, oversampling x16: BRD = fclk / baud = 1667 (0x683) */
    USART1->BRR = (SystemCoreClock + BAUD / 2u) / BAUD;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;   /* 8N1 default */
}

static void uart_putc(char c)
{
    while (!(USART1->SR & USART_SR_TXE)) { }
    USART1->DR = (uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    if (!v) { uart_putc('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i)  uart_putc(b[--i]);
}

int main(void)
{
    uint32_t seq = 0, last = 0;

    board_init();
    led_init();
    uart1_init();
    uart_puts("\r\nSWIS Bluetooth test ready. Send ? for help.\r\n");

    for (;;) {
        /* --- periodic telemetry --- */
        if ((millis() - last) >= 1000u) {
            last = millis();
            uart_puts("SWIS,SEQ="); uart_putu(seq++); uart_puts(",STATUS=SECURE\r\n");
        }

        /* --- commands from phone (non-blocking) --- */
        if (USART1->SR & USART_SR_RXNE) {
            char c = (char)USART1->DR;                    /* reading DR clears RXNE */
            switch (c) {
                case '1': led_set(1); uart_puts("LED ON\r\n");  break;
                case '0': led_set(0); uart_puts("LED OFF\r\n"); break;
                case 'B': uart_puts("ZONE 1: ANIMAL BREACH\r\n"); break;
                case '?': uart_puts("Cmds: 1=LED on 0=LED off B=breach alert\r\n"); break;
                default:  break;                          /* ignore CR/LF etc. */
            }
        }
        if (USART1->SR & USART_SR_ORE) { (void)USART1->SR; (void)USART1->DR; }  /* clear overrun */
    }
}
