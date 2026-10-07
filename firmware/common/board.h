/**
 * @file    board.h
 * @brief   Shared pin map + tiny helper API for the Wildlife Intrusion Sentinel tests.
 *          Register-level CMSIS only (no HAL needed) - works on Keil MDK with the
 *          Keil::STM32F4xx_DFP pack. Core clock = 16 MHz HSI (reset default).
 *
 *  FINAL PIN MAP (see README.md)
 *    PA0  Active IR OUT      EXTI0          PA6  Buzzer (via NPN)   GPIO out
 *    PA1  HC-SR501 PIR OUT   GPIO in        PA7  Relay IN           GPIO out
 *    PA2  SW-420 DO          GPIO in        PA9  HC-05 RXD          USART1_TX
 *    PA4  LDR module AO      ADC1_IN4       PA10 HC-05 TXD          USART1_RX
 *    PA5  Green status LED   GPIO out       PB6/PB7 LCD SCL/SDA     I2C1 (level shifter)
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx.h"   /* needs preprocessor symbol STM32F401xC (see docs/keil_setup.md) */
#include <stdint.h>

/* ---------- Pin assignments ---------- */
#define IR_PORT      GPIOA
#define IR_PIN       0
#define PIR_PORT     GPIOA
#define PIR_PIN      1
#define VIB_PORT     GPIOA
#define VIB_PIN      2
#define LDR_PORT     GPIOA
#define LDR_PIN      4
#define LED_PORT     GPIOA
#define LED_PIN      5
#define BUZ_PORT     GPIOA
#define BUZ_PIN      6
#define RELAY_PORT   GPIOA
#define RELAY_PIN    7

/* ---------- Module polarity - change here if YOUR module behaves the opposite way ---------- */
#define IR_ACTIVE_LOW     1   /* 1: OUT goes LOW when beam is broken (typical NPN/LM393 modules) */
#define PIR_ACTIVE_HIGH   1   /* HC-SR501: OUT goes HIGH on motion                               */
#define VIB_ACTIVE_HIGH   1   /* SW-420 (LM393): DO goes HIGH on vibration (check the module LED) */
#define RELAY_ACTIVE_LOW  1   /* most opto-isolated 5V relay boards: LOW = relay ON               */

/* ---------- GPIO helpers ---------- */
#define BRD_MODE_IN      0u
#define BRD_MODE_OUT     1u
#define BRD_MODE_AF      2u
#define BRD_MODE_ANALOG  3u

#define BRD_PULL_NONE    0u
#define BRD_PULL_UP      1u
#define BRD_PULL_DOWN    2u

void     board_init(void);                             /* clock, SysTick 1 ms, GPIOA/GPIOB clocks */
uint32_t millis(void);                                 /* ms since board_init()                   */
void     delay_ms(uint32_t ms);

void     gpio_mode(GPIO_TypeDef *p, uint8_t pin, uint32_t mode);
void     gpio_pull(GPIO_TypeDef *p, uint8_t pin, uint32_t pull);
void     gpio_af(GPIO_TypeDef *p, uint8_t pin, uint32_t af);
void     gpio_open_drain(GPIO_TypeDef *p, uint8_t pin, uint8_t enable);
void     gpio_write(GPIO_TypeDef *p, uint8_t pin, uint8_t level);
uint8_t  gpio_read(GPIO_TypeDef *p, uint8_t pin);

/* Status LED helpers (PA5) */
void     led_init(void);
void     led_set(uint8_t on);
void     led_blink(uint8_t times, uint32_t on_ms, uint32_t off_ms);

/* Never returns: blinks the status LED rapidly 'code' times, pauses, repeats. */
void     board_error_blink(uint8_t code);

#endif /* BOARD_H */
