/**
 * @file    main.c
 * @brief   Wildlife Intrusion Sentinel - integrated firmware (state machine).
 *
 * This is the real application: every subsystem that was proved by a test in
 * firmware/tests/ now runs together. It is ONE self-contained file on purpose, so the
 * Keil project stays simple - add exactly two files to the target:
 *
 *     firmware/app/main.c  +  firmware/common/board.c
 *
 * (The LCD, UART and ADC code below is the same code that passed test_lcd.c,
 *  test_bluetooth.c and test_ldr.c. It is copied rather than shared so that you do not
 *  have to set up extra modules or include paths in Keil.)
 *
 * PIN MAP (from board.h - do not change it here)
 *     PA0  IR receiver      EXTI0 interrupt      PA6  Buzzer 1 (direct, no NPN)
 *     PB5  PIR OUT          polled input         PA7  Relay -> 12 V siren
 *     PA2  SW-420 DO        polled input         PA9  HC-05 RXD (USART1 TX)
 *     PA4  LDR AO           ADC1_IN4             PA10 HC-05 TXD (USART1 RX)
 *     PA5  Green LED        SECURE indicator     PB6/PB7 LCD SCL/SDA (I2C1)
 *     PB0  Buzzer 2 (direct, optional - BUZ2_ENABLE in board.h)
 *
 * ZONES
 *     The farm is split in two, one zone along each long edge of the board. Each zone
 *     has a buzzer and an indicator LED. The LED says WHICH SIDE was breached; the
 *     buzzers only make noise, and every fitted buzzer sounds whichever zone fired.
 *     That is deliberate - if one buzzer is dead the alarm still sounds and the correct
 *     zone LED still lights, so the useful information survives a broken part.
 *         Zone A  IR beam (PA0)               -> LED PA3, buzzer PA6
 *         Zone B  PIR (PB5) + vibration (PA2) -> LED PA8, buzzer PB0 (if working)
 *     Change the ZONE_OF_* lines below to re-map a sensor to the other side.
 *
 * HOW IT BEHAVES
 *     WARMUP   ~30 s after power-up while the PIR settles. LED blinks slowly.
 *     SECURE   LED steady on, LCD "STATUS: SECURE". Waiting.
 *     VALIDATE A sensor fired. Look for a SECOND sensor within 2 s. This is what stops
 *              wind and glare causing false alarms. In the dark one sensor is enough.
 *     BREACH   Relay on (siren), buzzer varying cadence, LCD "STATUS: BREACH",
 *              one alert line sent over Bluetooth. Runs for 10 s.
 *     COOLDOWN Everything off for 3 s so one animal does not retrigger endlessly.
 *
 * BLUETOOTH COMMANDS (Android serial terminal, 9600 8N1)
 *     ?  help     S  status     R  reset to secure     T  2 s deterrent test
 *
 * NOTE: the siren is optional. With nothing wired to the relay contacts you will simply
 * hear the relay click, and everything else still works.
 */
#include "board.h"

/* ============================ TUNING ============================ */
#define PIR_WARMUP_MS      30000u   /* PIR settling time after power-up             */
#define VALIDATE_MS         2000u   /* window to find a confirming second sensor    */
#define BREACH_MS          10000u   /* how long the deterrent runs                  */
#define COOLDOWN_MS         3000u   /* quiet period after a breach                  */
#define IR_DEBOUNCE_MS       200u   /* ignore repeat IR edges inside this           */
#define LCD_REFRESH_MS       500u   /* how often the LCD may be rewritten           */

#define DARK_THRESHOLD      2500u   /* ADC reading above this = dark (see test_ldr.c) */
#define LDR_DARK_IS_HIGH        1   /* set to 0 if your module reads LOW when dark  */

/* 1 = two sensors needed in daylight (fewer false alarms).
 * 0 = any single sensor triggers a breach at any time. */
#define REQUIRE_SECOND_SENSOR   1

/* Which side of the farm each sensor watches. Swap these to re-map a sensor. */
#define ZONE_OF_IR    ZONE_A
#define ZONE_OF_PIR   ZONE_B
#define ZONE_OF_VIB   ZONE_B

/* ============================ STATE ============================ */
typedef enum { ST_WARMUP, ST_SECURE, ST_VALIDATE, ST_BREACH, ST_COOLDOWN } state_t;

volatile state_t  state        = ST_WARMUP;
volatile uint32_t breach_count = 0;   /* confirmed breaches                          */
volatile uint32_t false_alarms = 0;   /* triggers no second sensor confirmed         */
volatile uint32_t ir_events    = 0;
volatile uint32_t pir_events   = 0;
volatile uint32_t vib_events   = 0;
volatile uint16_t ldr_value    = 0;
volatile uint8_t  is_night     = 0;
volatile uint8_t  lcd_ok       = 0;   /* 0 = no LCD found; system runs without it    */
volatile uint8_t  breach_zone  = ZONE_A; /* which side fired: ZONE_A or ZONE_B       */

/* set by the IR interrupt, cleared by the main loop */
static volatile uint8_t  ir_flag    = 0;
static volatile uint32_t ir_last_ms = 0;

/* ======================= IR on PA0 (EXTI0) ======================= */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;                      /* clear pending (write 1) */
        uint32_t now = millis();
        if ((now - ir_last_ms) > IR_DEBOUNCE_MS) {
            ir_last_ms = now;
            ir_events++;
            ir_flag = 1;
        }
    }
}

static void ir_init(void)
{
    gpio_mode(IR_PORT, IR_PIN, BRD_MODE_IN);
#if IR_ACTIVE_LOW
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_UP);
#elif IR_EXT_PULLUP
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_NONE);       /* external 10k on the NPN collector */
#else
    gpio_pull(IR_PORT, IR_PIN, BRD_PULL_DOWN);
#endif
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;      /* EXTI0 <- port A */
    EXTI->IMR |= EXTI_IMR_MR0;
#if IR_ACTIVE_LOW
    EXTI->FTSR |= EXTI_FTSR_TR0;  EXTI->RTSR &= ~EXTI_RTSR_TR0;
#else
    EXTI->RTSR |= EXTI_RTSR_TR0;  EXTI->FTSR &= ~EXTI_FTSR_TR0;
#endif
    EXTI->PR = EXTI_PR_PR0;
    NVIC_SetPriority(EXTI0_IRQn, 0);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

/* ==================== PIR (PB5) and vibration (PA2) ==================== */
static void sensors_init(void)
{
    gpio_mode(PIR_PORT, PIR_PIN, BRD_MODE_IN);
    gpio_pull(PIR_PORT, PIR_PIN, BRD_PULL_DOWN);
    gpio_mode(VIB_PORT, VIB_PIN, BRD_MODE_IN);
    gpio_pull(VIB_PORT, VIB_PIN, BRD_PULL_DOWN);
}

static uint8_t pir_active(void)
{
    uint8_t v = gpio_read(PIR_PORT, PIR_PIN);
    return (uint8_t)(PIR_ACTIVE_HIGH ? v : !v);
}

static uint8_t vib_active(void)
{
    uint8_t v = gpio_read(VIB_PORT, VIB_PIN);
    return (uint8_t)(VIB_ACTIVE_HIGH ? v : !v);
}

/* ======================= LDR on PA4 (ADC1_IN4) ======================= */
static void adc1_init(void)
{
    gpio_mode(LDR_PORT, LDR_PIN, BRD_MODE_ANALOG);
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    ADC1->SMPR2 |= (7u << (3u * 4u));                /* channel 4: long sample time */
    ADC1->SQR1   = 0;
    ADC1->SQR3   = 4u;
    ADC1->CR2   |= ADC_CR2_ADON;
    delay_ms(2);
}

static uint16_t adc1_read(void)
{
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC)) { }
    return (uint16_t)ADC1->DR;
}

/* ============ Buzzers (PA6, and PB0 when BUZ2_ENABLE) and relay (PA7) ============
 * buzzer_init() and buzzer_set() live in board.c so that wiring a second buzzer is a
 * board.h change and nothing here needs touching. They drive both buzzers together. */

/* Active-low opto relay boards are driven open-drain: the MCU only ever pulls the input
 * to GND, never tries to present 3.3 V to a 5 V-referenced input. Same scheme as
 * test_actuators.c and test_siren.c. */
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
    relay_set(0);                                    /* OFF before the pin drives */
#if RELAY_ACTIVE_LOW
    gpio_open_drain(RELAY_PORT, RELAY_PIN, 1);
#endif
    gpio_mode(RELAY_PORT, RELAY_PIN, BRD_MODE_OUT);
}

/* Varying buzzer cadence, driven from millis() so it never blocks the main loop.
 * The pattern changes on purpose: a constant tone is something animals get used to. */
static void buzzer_pattern(uint32_t now, uint8_t active)
{
    static uint32_t next_ms = 0;
    static uint32_t step    = 0;
    static uint8_t  on      = 0;

    if (!active) {
        if (on) { on = 0; buzzer_set(0); }
        next_ms = now;
        step = 0;
        return;
    }
    if ((int32_t)(now - next_ms) < 0) return;

    on = (uint8_t)!on;
    buzzer_set(on);
    if (on) next_ms = now + 60u + 40u * (step % 4u);
    else  { next_ms = now + 200u - 40u * (step % 4u); step++; }
}

/* ========================= LCD on I2C1 (PB6/PB7) ========================= */
#define LCD_RS  0x01u
#define LCD_EN  0x04u
#define LCD_BL  0x08u

static uint8_t lcd_addr = 0x27;

static void i2c1_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    gpio_af(GPIOB, 6, 4);  gpio_af(GPIOB, 7, 4);
    gpio_open_drain(GPIOB, 6, 1);  gpio_open_drain(GPIOB, 7, 1);
    gpio_pull(GPIOB, 6, BRD_PULL_NONE);  gpio_pull(GPIOB, 7, BRD_PULL_NONE);
    gpio_mode(GPIOB, 6, BRD_MODE_AF);    gpio_mode(GPIOB, 7, BRD_MODE_AF);

    I2C1->CR1 |= I2C_CR1_SWRST;  I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2   = 16u;
    I2C1->CCR   = 80u;
    I2C1->TRISE = 17u;
    I2C1->CR1 |= I2C_CR1_PE;
}

static int i2c_write_byte(uint8_t addr7, uint8_t data)
{
    int rc = -1;
    uint32_t t;

    I2C1->CR1 |= I2C_CR1_START;
    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_SB)) { if (millis() - t > 5u) goto done; }
    I2C1->DR = (uint8_t)(addr7 << 1);
    t = millis();
    while (!(I2C1->SR1 & (I2C_SR1_ADDR | I2C_SR1_AF))) { if (millis() - t > 5u) goto done; }
    if (I2C1->SR1 & I2C_SR1_AF) { I2C1->SR1 &= ~I2C_SR1_AF; goto done; }
    (void)I2C1->SR1; (void)I2C1->SR2;
    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_TXE)) { if (millis() - t > 5u) goto done; }
    I2C1->DR = data;
    t = millis();
    while (!(I2C1->SR1 & I2C_SR1_BTF)) { if (millis() - t > 5u) goto done; }
    rc = 0;
done:
    I2C1->CR1 |= I2C_CR1_STOP;
    return rc;
}

static void lcd_nibble(uint8_t nib, uint8_t rs)
{
    uint8_t d = (uint8_t)((nib << 4) | LCD_BL | (rs ? LCD_RS : 0u));
    i2c_write_byte(lcd_addr, (uint8_t)(d | LCD_EN));
    i2c_write_byte(lcd_addr, (uint8_t)(d & (uint8_t)~LCD_EN));
}

static void lcd_send(uint8_t byte, uint8_t rs)
{
    lcd_nibble((uint8_t)(byte >> 4), rs);
    lcd_nibble((uint8_t)(byte & 0x0Fu), rs);
}

static void lcd_cmd(uint8_t c) { lcd_send(c, 0); delay_ms(2); }
static void lcd_putc(char c)   { lcd_send((uint8_t)c, 1); }
static void lcd_goto(uint8_t col, uint8_t row) { lcd_cmd((uint8_t)(0x80u | ((row ? 0x40u : 0u) + col))); }

static void lcd_init(void)
{
    delay_ms(50);
    lcd_nibble(0x3, 0); delay_ms(5);
    lcd_nibble(0x3, 0); delay_ms(1);
    lcd_nibble(0x3, 0); delay_ms(1);
    lcd_nibble(0x2, 0); delay_ms(1);
    lcd_cmd(0x28); lcd_cmd(0x08); lcd_cmd(0x01); lcd_cmd(0x06); lcd_cmd(0x0C);
}

/* Write one 16-character line, padding with spaces so old text is erased. */
static void lcd_line(uint8_t row, const char *s)
{
    uint8_t i;
    uint8_t ended = 0;
    if (!lcd_ok) return;
    lcd_goto(0, row);
    for (i = 0; i < 16u; i++) {
        if (!ended && s[i] == '\0') ended = 1;
        lcd_putc(ended ? ' ' : s[i]);
    }
}

/* ===================== HC-05 on USART1 (PA9/PA10) ===================== */
#define BAUD  9600u

static void uart1_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    gpio_af(GPIOA, 9, 7);  gpio_af(GPIOA, 10, 7);
    gpio_mode(GPIOA, 9, BRD_MODE_AF);
    gpio_mode(GPIOA, 10, BRD_MODE_AF);
    gpio_pull(GPIOA, 10, BRD_PULL_UP);
    USART1->BRR = (SystemCoreClock + BAUD / 2u) / BAUD;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

static void uart_putc(char c) { while (!(USART1->SR & USART_SR_TXE)) { } USART1->DR = (uint8_t)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_putu(uint32_t v)
{
    char b[11]; int i = 0;
    if (!v) { uart_putc('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10u); v /= 10u; }
    while (i) uart_putc(b[--i]);
}

static void send_status(void)
{
    static const char *const names[] = { "WARMUP", "SECURE", "VALIDATE", "BREACH", "COOLDOWN" };
    uart_puts("SWIS,STATE="); uart_puts(names[state]);
    uart_puts(",BREACH=");    uart_putu(breach_count);
    uart_puts(",FALSE=");     uart_putu(false_alarms);
    uart_puts(",LDR=");       uart_putu(ldr_value);
    uart_puts((breach_zone == ZONE_A) ? ",ZONE=A" : ",ZONE=B");
    uart_puts(is_night ? ",NIGHT\r\n" : ",DAY\r\n");
}

/* Build "DAY   B=12" / "NIGHT B=12" into a 17-byte buffer. */
static void make_line2(char *out)
{
    const char *tag = (breach_zone == ZONE_A) ? "ZONE A  B=" : "ZONE B  B=";
    uint32_t n = breach_count;
    int i = 0;

    while (tag[i] && i < 16) { out[i] = tag[i]; i++; }
    if (n == 0) {
        if (i < 16) out[i++] = '0';
    } else {
        char tmp[11]; int k = 0;
        while (n && k < 11) { tmp[k++] = (char)('0' + n % 10u); n /= 10u; }
        while (k && i < 16) out[i++] = tmp[--k];
    }
    out[i] = '\0';
}

/* ============================== MAIN ============================== */
int main(void)
{
    uint32_t now;
    uint32_t state_since = 0;     /* millis() when the current state began       */
    uint32_t last_lcd    = 0;
    uint32_t last_ldr    = 0;
    uint32_t last_beacon = 0;
    uint8_t  trigger_mask = 0;    /* bit0 IR, bit1 PIR, bit2 VIB during VALIDATE */
    uint8_t  pir_prev, vib_prev;
    const char *lcd_top   = "";
    const char *lcd_shown = 0;

    board_init();
    led_init();
    relay_init();                 /* relay OFF first, so the siren cannot bang at reset */
    buzzer_init();
    zone_leds_init();             /* both zone LEDs off */
    sensors_init();
    ir_init();
    adc1_init();
    uart1_init();
    i2c1_init();

    /* The LCD is optional: if no backpack answers, lcd_ok stays 0 and every lcd_*
     * call becomes a no-op instead of hanging the system. */
    if      (i2c_write_byte(0x27, LCD_BL) == 0) { lcd_addr = 0x27; lcd_ok = 1; }
    else if (i2c_write_byte(0x3F, LCD_BL) == 0) { lcd_addr = 0x3F; lcd_ok = 1; }
    if (lcd_ok) lcd_init();

    lcd_line(0, "SWIS STARTING");
    lcd_line(1, "PIR warming up");
    uart_puts("\r\nSWIS ready. ? for help.\r\n");

    pir_prev    = pir_active();
    vib_prev    = vib_active();
    state       = ST_WARMUP;
    state_since = millis();

    for (;;) {
        now = millis();

        /* ---------- read the light level a few times a second ---------- */
        if ((now - last_ldr) >= 250u) {
            last_ldr  = now;
            ldr_value = adc1_read();
#if LDR_DARK_IS_HIGH
            is_night = (uint8_t)(ldr_value > DARK_THRESHOLD);
#else
            is_night = (uint8_t)(ldr_value < DARK_THRESHOLD);
#endif
        }

        /* ---------- sample the polled sensors, count rising edges ---------- */
        {
            uint8_t p = pir_active();
            uint8_t v = vib_active();
            if (p && !pir_prev) pir_events++;
            if (v && !vib_prev) vib_events++;
            if (state == ST_VALIDATE) {               /* collect confirmation */
                if (p) trigger_mask |= 0x02u;
                if (v) trigger_mask |= 0x04u;
            }
            pir_prev = p;
            vib_prev = v;
        }

        /* ---------- Bluetooth commands (non-blocking) ---------- */
        if (USART1->SR & USART_SR_RXNE) {
            char c = (char)USART1->DR;
            switch (c) {
                case '?':
                    uart_puts("Cmds: ?=help S=status R=reset T=test deterrent\r\n");
                    break;
                case 'S': case 's':
                    send_status();
                    break;
                case 'R': case 'r':
                    relay_set(0);
                    buzzer_pattern(now, 0);
                    zone_leds_off();
                    state = ST_SECURE; state_since = now; trigger_mask = 0;
                    uart_puts("Reset to SECURE\r\n");
                    break;
                case 'T': case 't': {
                    uint32_t t0 = millis();
                    uart_puts("Deterrent test 2s\r\n");
                    relay_set(1);
                    while ((millis() - t0) < 2000u) buzzer_pattern(millis(), 1);
                    buzzer_pattern(millis(), 0);
                    relay_set(0);
                    break;
                }
                default:
                    break;
            }
        }
        if (USART1->SR & USART_SR_ORE) { (void)USART1->SR; (void)USART1->DR; }

        /* ======================== STATE MACHINE ======================== */
        switch (state) {

        case ST_WARMUP:
            led_set((uint8_t)((now / 500u) & 1u));       /* slow blink, non-blocking */
            zone_leds_off();
            lcd_top = "SWIS WARMING UP";
            ir_flag = 0;                                 /* ignore triggers while settling */
            if ((now - state_since) >= PIR_WARMUP_MS) {
                state = ST_SECURE; state_since = now;
                uart_puts("Warmup done. Armed.\r\n");
            }
            break;

        case ST_SECURE:
            led_set(1);                                  /* steady = armed */
            zone_leds_off();                             /* no zone is alarming */
            lcd_top = "STATUS: SECURE";
            if (ir_flag || pir_active() || vib_active()) {
                trigger_mask = 0;
                /* The FIRST sensor to fire decides the zone - that is the side the
                 * animal crossed, even if the other side confirms a moment later. */
                if (ir_flag)           breach_zone = ZONE_OF_IR;
                else if (pir_active()) breach_zone = ZONE_OF_PIR;
                else                   breach_zone = ZONE_OF_VIB;
                if (ir_flag)      { trigger_mask |= 0x01u; ir_flag = 0; }
                if (pir_active())   trigger_mask |= 0x02u;
                if (vib_active())   trigger_mask |= 0x04u;
                state = ST_VALIDATE; state_since = now;
            }
            break;

        case ST_VALIDATE: {
            uint8_t votes;
            uint8_t needed;

            if (ir_flag) { trigger_mask |= 0x01u; ir_flag = 0; }
            votes = (uint8_t)(( trigger_mask       & 1u) +
                              ((trigger_mask >> 1) & 1u) +
                              ((trigger_mask >> 2) & 1u));
            /* At night a single sensor is enough; in daylight we want two, because
             * sun glare and wind are what produce daytime false alarms. */
            needed = (uint8_t)((REQUIRE_SECOND_SENSOR && !is_night) ? 2u : 1u);

            led_set((uint8_t)((now / 100u) & 1u));       /* fast blink = checking */
            /* the suspect zone's LED blinks while we look for confirmation */
            zone_led_set(breach_zone, (uint8_t)((now / 250u) & 1u));
            lcd_top = "CHECKING...";

            if (votes >= needed) {
                state = ST_BREACH; state_since = now;
                breach_count++;
                relay_set(1);
                uart_puts("ZONE 1: ANIMAL BREACH\r\n");
                send_status();
            } else if ((now - state_since) >= VALIDATE_MS) {
                false_alarms++;
                state = ST_SECURE; state_since = now;
                uart_puts("Unconfirmed trigger ignored\r\n");
            }
            break;
        }

        case ST_BREACH:
            led_set((uint8_t)((now / 100u) & 1u));       /* fast blink = alarm */
            /* SOLID zone LED - the one thing that says which side was breached, and it
             * does not depend on any buzzer working. */
            zone_led_set(breach_zone, 1);
            lcd_top = "STATUS: BREACH";
            buzzer_pattern(now, 1);                      /* every fitted buzzer sounds */
            if ((now - state_since) >= BREACH_MS) {
                relay_set(0);
                buzzer_pattern(now, 0);
                state = ST_COOLDOWN; state_since = now;
            }
            break;

        case ST_COOLDOWN:
            led_set(0);
            zone_led_set(breach_zone, (uint8_t)((now / 500u) & 1u)); /* slow: still flagged */
            lcd_top = "COOLDOWN";
            relay_set(0);
            buzzer_pattern(now, 0);
            ir_flag = 0;
            if ((now - state_since) >= COOLDOWN_MS) {
                state = ST_SECURE; state_since = now;
                trigger_mask = 0;
            }
            break;

        default:
            state = ST_SECURE; state_since = now;
            break;
        }

        /* ---------- LCD: only rewrite when the message actually changes ---------- */
        if (lcd_ok && lcd_top != lcd_shown && (now - last_lcd) >= LCD_REFRESH_MS) {
            char l2[17];
            last_lcd  = now;
            lcd_shown = lcd_top;
            make_line2(l2);
            lcd_line(0, lcd_top);
            lcd_line(1, l2);
        }

        /* ---------- heartbeat to the phone every 5 s ---------- */
        if ((now - last_beacon) >= 5000u) {
            last_beacon = now;
            send_status();
        }
    }
}
