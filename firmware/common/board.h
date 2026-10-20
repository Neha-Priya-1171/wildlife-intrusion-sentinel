/**
 * @file    board.h
 * @brief   Shared pin map + tiny helper API for the Wildlife Intrusion Sentinel tests.
 *          Register-level CMSIS only (no HAL needed) - works on Keil MDK with the
 *          Keil::STM32F4xx_DFP pack. Core clock = 16 MHz HSI (reset default).
 *
 *  FINAL PIN MAP (see README.md)
 *    PA0  Active IR OUT      EXTI0          PA6  Buzzer (via NPN)   GPIO out
 *    PB5  PIR OUT (was PA1)  GPIO in        PA7  Relay IN           GPIO out
 *    PA2  SW-420 DO          GPIO in        PA9  HC-05 RXD          USART1_TX
 *    PA4  LDR module AO      ADC1_IN4       PA10 HC-05 TXD          USART1_RX
 *    PA5  Green status LED   GPIO out       PB6/PB7 LCD SCL/SDA     I2C1 (level shifter)
 *    PB0  Buzzer 2 (zone B)    GPIO out (optional, see BUZ2_ENABLE)
 *    PA5  doubles as the Zone A LED           PA8  Zone B indicator LED
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f4xx.h"   /* needs preprocessor symbol STM32F401xC (see docs/keil_setup.md) */
#include <stdint.h>

/* ---------- Pin assignments ---------- */
#define IR_PORT      GPIOA
#define IR_PIN       0
/* PIR moved off PA1 to PB5 after PA1 read permanently HIGH.
 * Two reasons for PB5: it is free, and unlike PA1 it has no ADC function, so it is an
 * FT (5 V tolerant) pin. The PIR module measured ~4.3 V on OUT, which is over the limit
 * for PA1 but within what an FT pin accepts. Treat PA1 as suspect until proven good. */
#define PIR_PORT     GPIOB
#define PIR_PIN      5
#define VIB_PORT     GPIOA
#define VIB_PIN      2
#define LDR_PORT     GPIOA
#define LDR_PIN      4
#define LED_PORT     GPIOA
#define LED_PIN      5
/* Buzzer: driven DIRECTLY from the GPIO pin, no transistor.
 * The original design used an NPN low-side switch so the buzzer could run from 5 V, but
 * the only BC547 on hand went into the IR receiver stage (assembly guide, Stage 2), and
 * a working circuit is not worth dismantling. A small 12 mm active buzzer straight on the
 * pin runs at 3.3 V instead: quieter, but around 20 mA, inside the 25 mA an STM32F401 pin
 * is rated for. Wire buzzer + to the pin and buzzer - to GND. */
#define BUZ_PORT     GPIOA
#define BUZ_PIN      6

/* Optional SECOND buzzer on PB0, for the spare one in the kit. Two buzzers must never
 * share one pin - that doubles the current past the pin's limit - so the spare gets its
 * own. Set BUZ2_ENABLE back to 0 if you unwire PB0. */
#define BUZ2_PORT    GPIOB
#define BUZ2_PIN     0
#define BUZ2_ENABLE  1
#define RELAY_PORT   GPIOA
#define RELAY_PIN    7

/* ---------- Zone indicator LEDs ----------
 * The farm is split into two zones, one along each long edge of the breadboard. Each
 * zone has a buzzer and an LED. The LED is what says WHICH SIDE the breach is on; the
 * buzzers only make noise. That separation is deliberate: if a zone's buzzer is dead,
 * the other zone's buzzer still sounds the alarm and the correct zone LED still lights,
 * so the system never loses the one piece of information that matters.
 *
 * Both LEDs are on PORT A on purpose. Port A is proven working on this board, so zone
 * indication does not depend on the port that is giving trouble.
 *
 * ZONE A REUSES THE STATUS LED ON PA5. No extra LED and no extra wiring - the green
 * LED that is already there does double duty. Because one pin now has two jobs, every
 * indicator decision in the application is made in ONE place (see indicators() in
 * main.c) so the status role and the zone role can never fight over PA5.
 *
 * Only ZONE B needs a new LED: PA8 -> 220 ohm (270 is fine) -> long leg, short leg GND. */
#define ZONE_A_LED_PORT  GPIOA
#define ZONE_A_LED_PIN   5        /* shared with the status LED - see note above */
#define ZONE_B_LED_PORT  GPIOA
#define ZONE_B_LED_PIN   8
#define ZONE_LEDS_ENABLE 1        /* 0 = no zone LEDs fitted, everything else still runs */

/* ---------- Module polarity - change here if YOUR module behaves the opposite way ---------- */
#define IR_ACTIVE_LOW     0   /* 0 = discrete IR LED + photodiode + NPN stage (PA0 goes HIGH when beam is broken).
                                 Set to 1 only if you use a ready-made break-beam module whose OUT goes LOW on break. */
#define IR_EXT_PULLUP     1   /* 1 = the NPN collector has its own 10k pull-up to 3.3 V, so the MCU pull is disabled */
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

/* Buzzer helpers. One implementation in board.c, used by every test and by the
 * application, so wiring a second buzzer is a board.h change and nothing else.
 * buzzer_set() drives BOTH buzzers together when BUZ2_ENABLE is 1. */
void     buzzer_init(void);
void     buzzer_set(uint8_t on);

/* Zone indicators. zone is 0 for zone A, 1 for zone B. */
#define ZONE_A  0u
#define ZONE_B  1u
void     zone_leds_init(void);
void     zone_led_set(uint8_t zone, uint8_t on);
void     zone_leds_off(void);

/* Status LED helpers (PA5) */
void     led_init(void);
void     led_set(uint8_t on);
void     led_blink(uint8_t times, uint32_t on_ms, uint32_t off_ms);

/* Never returns: blinks the status LED rapidly 'code' times, pauses, repeats. */
void     board_error_blink(uint8_t code);

#endif /* BOARD_H */
