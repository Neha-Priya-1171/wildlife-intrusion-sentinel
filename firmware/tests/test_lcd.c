/**
 * @file    test_lcd.c
 * @brief   TEST 4 - 16x2 character LCD via PCF8574 I2C backpack on I2C1 (PB6 SCL / PB7 SDA).
 * Wiring: STM32 PB6/PB7 --> level shifter LV side ; level shifter HV side --> LCD backpack
 *         SCL/SDA ; LV pin = 3.3 V, HV pin = 5 V, grounds common ; backpack VCC = 5 V.
 * Expect: auto-detects address 0x27 or 0x3F, then shows "STATUS: SECURE" /
 *         "STATUS: BREACH" alternately with a seconds counter on line 2.
 * Errors (status LED on PA5): 2 blinks = no I2C ACK at 0x27/0x3F (check wiring/shifter/power).
 * If text is invisible but backlight is on: turn the blue contrast trimmer on the backpack.
 * Files to build: test_lcd.c + common/board.c
 */
#include "board.h"

/* PCF8574 -> HD44780 wiring used by nearly all backpacks */
#define LCD_RS  0x01u
#define LCD_RW  0x02u   /* kept LOW (write only) */
#define LCD_EN  0x04u
#define LCD_BL  0x08u   /* backlight */

static uint8_t lcd_addr = 0x27;

/* ---------------- minimal I2C1 master (standard mode 100 kHz) ---------------- */
static void i2c1_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;

    gpio_af(GPIOB, 6, 4);  gpio_af(GPIOB, 7, 4);          /* AF4 = I2C1 */
    gpio_open_drain(GPIOB, 6, 1);  gpio_open_drain(GPIOB, 7, 1);
    gpio_pull(GPIOB, 6, BRD_PULL_NONE);  gpio_pull(GPIOB, 7, BRD_PULL_NONE); /* external pull-ups */
    gpio_mode(GPIOB, 6, BRD_MODE_AF);    gpio_mode(GPIOB, 7, BRD_MODE_AF);

    I2C1->CR1 |= I2C_CR1_SWRST;  I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2  = 16u;                  /* APB1 = 16 MHz                              */
    I2C1->CCR  = 80u;                  /* 16 MHz / (2 x 100 kHz) = 80                */
    I2C1->TRISE = 17u;                 /* 1000 ns / 62.5 ns + 1                      */
    I2C1->CR1 |= I2C_CR1_PE;
}

/* returns 0 = OK (ACKed), -1 = timeout/NACK */
static int i2c_write_byte(uint8_t addr7, uint8_t data)
{
    int rc = -1;
    uint32_t t;

    I2C1->CR1 |= I2C_CR1_START;
    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_SB))   { if (millis() - t > 5u) goto done; }

    I2C1->DR = (uint8_t)(addr7 << 1);                     /* write */
    t = millis();
    while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF))) { if (millis() - t > 5u) goto done; }
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto done; }   /* NACK */
    (void)I2C1->SR1; (void)I2C1->SR2;                     /* clear ADDR */

    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_TXE))  { if (millis() - t > 5u) goto done; }
    I2C1->DR = data;
    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_BTF))  { if (millis() - t > 5u) goto done; }
    rc = 0;
done:
    I2C1->CR1 |= I2C_CR1_STOP;
    return rc;
}

/* ---------------- HD44780 driver in 4-bit mode ---------------- */
static void lcd_nibble(uint8_t nib, uint8_t rs)
{
    uint8_t d = (uint8_t)((nib << 4) | LCD_BL | (rs ? LCD_RS : 0u));
    i2c_write_byte(lcd_addr, d | LCD_EN);                 /* EN high  */
    i2c_write_byte(lcd_addr, d & (uint8_t)~LCD_EN);       /* EN low -> latch (each I2C write ~100 us) */
}

static void lcd_send(uint8_t byte, uint8_t rs)
{
    lcd_nibble(byte >> 4, rs);
    lcd_nibble(byte & 0x0Fu, rs);
}

static void lcd_cmd(uint8_t c)  { lcd_send(c, 0); delay_ms(2); }
static void lcd_putc(char c)    { lcd_send((uint8_t)c, 1); }
static void lcd_print(const char *s) { while (*s) lcd_putc(*s++); }
static void lcd_goto(uint8_t col, uint8_t row) { lcd_cmd((uint8_t)(0x80u | ((row ? 0x40u : 0u) + col))); }

static void lcd_print_uint(uint32_t v)
{
    char buf[11]; int i = 0;
    if (v == 0) { lcd_putc('0'); return; }
    while (v) { buf[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i)  lcd_putc(buf[--i]);
}

static void lcd_init(void)
{
    delay_ms(50);                       /* power-up wait */
    lcd_nibble(0x3, 0); delay_ms(5);    /* magic reset sequence */
    lcd_nibble(0x3, 0); delay_ms(1);
    lcd_nibble(0x3, 0); delay_ms(1);
    lcd_nibble(0x2, 0); delay_ms(1);    /* switch to 4-bit */
    lcd_cmd(0x28);                      /* 4-bit, 2 lines, 5x8 font */
    lcd_cmd(0x08);                      /* display off */
    lcd_cmd(0x01);                      /* clear */
    lcd_cmd(0x06);                      /* entry mode: increment, no shift */
    lcd_cmd(0x0C);                      /* display on, cursor off */
}

int main(void)
{
    uint32_t sec = 0;
    uint8_t  breach = 0;

    board_init();
    led_init();
    i2c1_init();

    /* find the backpack: PCF8574 = 0x27, PCF8574A = 0x3F */
    if      (i2c_write_byte(0x27, LCD_BL) == 0) lcd_addr = 0x27;
    else if (i2c_write_byte(0x3F, LCD_BL) == 0) lcd_addr = 0x3F;
    else board_error_blink(2);

    lcd_init();
    lcd_goto(0, 0); lcd_print("SWIS LCD TEST OK");
    lcd_goto(0, 1); lcd_print("I2C addr 0x");
    lcd_putc("0123456789ABCDEF"[lcd_addr >> 4]);
    lcd_putc("0123456789ABCDEF"[lcd_addr & 0xF]);
    delay_ms(2000);

    for (;;) {
        lcd_goto(0, 0);
        lcd_print(breach ? "STATUS: BREACH  " : "STATUS: SECURE  ");
        lcd_goto(0, 1);
        lcd_print("Uptime: "); lcd_print_uint(sec); lcd_print("s    ");
        led_set(!breach);
        delay_ms(1000);
        sec++;
        if ((sec % 4u) == 0u) breach = !breach;    /* flip message every 4 s */
    }
}
