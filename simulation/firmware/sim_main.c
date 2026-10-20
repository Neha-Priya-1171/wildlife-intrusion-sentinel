/**
 * @file    sim_main.c
 * @brief   SIMULATION BUILD of the Wildlife Intrusion Sentinel firmware.
 *
 * This file is SEPARATE from firmware/app/main.c on purpose. main.c is the submission
 * firmware and nothing here touches it. sim_main.c is a copy of it with one thing added:
 * fast, machine-readable telemetry, so the desktop GUI in simulation/swis_gui.py can
 * animate what the hardware is doing in real time.
 *
 * Everything else - pin map, state machine, timings, validation rule - is identical to
 * main.c. Flash whichever you want; they do not interfere with each other.
 *
 * REGENERATE: this file is derived from main.c by simulation/tools/gen_sim.py.
 *
 * BUILD: simulation/firmware/sim_main.c + firmware/common/board.c
 *        (same Keil options and include path as main.c)
 *
 * PIN MAP (from board.h - do not change it here)
 *     PA0  IR receiver      EXTI0 interrupt      PA6  Buzzer via NPN
 *     PB5  PIR OUT          polled input         PA7  Relay -> 12 V siren
 *     PA2  SW-420 DO        polled input         PA9  HC-05 RXD (USART1 TX)
 *     PA4  LDR AO           ADC1_IN4             PA10 HC-05 TXD (USART1 RX)
 *     PA5  Green LED        SECURE indicator     PB6/PB7 LCD SCL/SDA (I2C1)
 *
 * STATE MACHINE
 *     WARMUP -> SECURE -> VALIDATE -> BREACH -> COOLDOWN -> SECURE
 *
 * BLUETOOTH COMMANDS (9600 8N1)
 *     ?  help   S  status   R  reset to secure   T  2 s deterrent test
 *     D  toggle demo mode (one sensor is enough to raise a breach, any time of day)
 *
 * TELEMETRY PROTOCOL - every line ends CR LF. simulation/swis_gui.py parses these.
 *     EV,SRC=IR            a sensor fired, sent the instant it happens (also PIR, VIB)
 *     ST,<STATE>,DETER=0|1 state changed; DETER says whether buzzer+siren are running
 *     ALERT,ZONE1,ANIMAL BREACH          the farmer notification
 *     SWIS,STATE=..,BREACH=..,FALSE=..,LDR=..,DAY|NIGHT,DEMO=0|1   heartbeat, 1 Hz
 *
 * Event lines go out the instant a sensor fires, so the animal on screen starts moving
 * in step with the hand that broke the beam. The heartbeat only refreshes slow numbers.
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
volatile uint8_t  deter_on     = 0;   /* 1 while the buzzer and siren are running    */
volatile uint8_t  demo_mode    = 0;   /* 1 = one sensor is enough (toggle with D)    */

static uint32_t state_since = 0;      /* millis() when the current state began       */

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

/* ====================== Buzzer (PA6) and relay (PA7) ====================== */
static void buzzer_init(void) { gpio_write(BUZ_PORT, BUZ_PIN, 0); gpio_mode(BUZ_PORT, BUZ_PIN, BRD_MODE_OUT); }
static void buzzer(uint8_t on) { gpio_write(BUZ_PORT, BUZ_PIN, on); }

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
        if (on) { on = 0; buzzer(0); }
        next_ms = now;
        step = 0;
        return;
    }
    if ((int32_t)(now - next_ms) < 0) return;

    on = (uint8_t)!on;
    buzzer(on);
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

static const char *const state_names[] = { "WARMUP", "SECURE", "VALIDATE", "BREACH", "COOLDOWN" };

static void send_status(void)
{
    uart_puts("SWIS,STATE="); uart_puts(state_names[state]);
    uart_puts(",BREACH=");    uart_putu(breach_count);
    uart_puts(",FALSE=");     uart_putu(false_alarms);
    uart_puts(",LDR=");       uart_putu(ldr_value);
    uart_puts(is_night ? ",NIGHT" : ",DAY");
    uart_puts(demo_mode ? ",DEMO=1\r\n" : ",DEMO=0\r\n");
}

/* Sent the moment a sensor fires, so the GUI animation starts in step with the event
 * instead of waiting for the next heartbeat. */
static void emit_event(const char *src)
{
    uart_puts("EV,SRC="); uart_puts(src); uart_puts("\r\n");
}

/* The single place the state changes, so every transition is announced exactly once. */
static void enter_state(state_t s, uint32_t now)
{
    state       = s;
    state_since = now;
    uart_puts("ST,"); uart_puts(state_names[state]);
    uart_puts(deter_on ? ",DETER=1\r\n" : ",DETER=0\r\n");
}

/* Build "DAY   B=12" / "NIGHT B=12" into a 17-byte buffer. */
static void make_line2(char *out)
{
    const char *tag = is_night ? "NIGHT B=" : "DAY   B=";
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
    uint32_t last_lcd    = 0;
    uint32_t last_ldr    = 0;
    uint32_t last_beacon = 0;
    uint8_t  trigger_mask = 0;    /* bit0 IR, bit1 PIR, bit2 VIB during VALIDATE */
    uint8_t  pir_prev, vib_prev;
    uint32_t ir_seen = 0;         /* last value of ir_events we reported          */
    const char *lcd_top   = "";
    const char *lcd_shown = 0;

    board_init();
    led_init();
    relay_init();                 /* relay OFF first, so the siren cannot bang at reset */
    buzzer_init();
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

    pir_prev = pir_active();
    vib_prev = vib_active();
    enter_state(ST_WARMUP, millis());

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
            if (p && !pir_prev) { pir_events++; emit_event("PIR"); }
            if (v && !vib_prev) { vib_events++; emit_event("VIB"); }
            if (ir_events != ir_seen) { ir_seen = ir_events; emit_event("IR"); }
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
                    uart_puts("Cmds: ?=help S=status R=reset T=test D=demo mode\r\n");
                    break;
                case 'S': case 's':
                    send_status();
                    break;
                case 'R': case 'r':
                    relay_set(0);
                    buzzer_pattern(now, 0);
                    deter_on = 0;
                    trigger_mask = 0;
                    enter_state(ST_SECURE, now);
                    break;
                case 'D': case 'd':
                    demo_mode = (uint8_t)!demo_mode;
                    uart_puts(demo_mode ? "Demo mode ON: one sensor triggers\r\n"
                                        : "Demo mode OFF: daylight needs two\r\n");
                    send_status();
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
            lcd_top = "SWIS WARMING UP";
            ir_flag = 0;                                 /* ignore triggers while settling */
            if ((now - state_since) >= PIR_WARMUP_MS) {
                enter_state(ST_SECURE, now);
            }
            break;

        case ST_SECURE:
            led_set(1);                                  /* steady = armed */
            lcd_top = "STATUS: SECURE";
            if (ir_flag || pir_active() || vib_active()) {
                trigger_mask = 0;
                if (ir_flag)      { trigger_mask |= 0x01u; ir_flag = 0; }
                if (pir_active())   trigger_mask |= 0x02u;
                if (vib_active())   trigger_mask |= 0x04u;
                enter_state(ST_VALIDATE, now);
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
            needed = (uint8_t)((REQUIRE_SECOND_SENSOR && !is_night && !demo_mode) ? 2u : 1u);

            led_set((uint8_t)((now / 100u) & 1u));       /* fast blink = checking */
            lcd_top = "CHECKING...";

            if (votes >= needed) {
                breach_count++;
                relay_set(1);
                deter_on = 1;
                enter_state(ST_BREACH, now);
                uart_puts("ALERT,ZONE1,ANIMAL BREACH\r\n");
                send_status();
            } else if ((now - state_since) >= VALIDATE_MS) {
                false_alarms++;
                enter_state(ST_SECURE, now);
            }
            break;
        }

        case ST_BREACH:
            led_set((uint8_t)((now / 100u) & 1u));       /* fast blink = alarm */
            lcd_top = "STATUS: BREACH";
            buzzer_pattern(now, 1);
            if ((now - state_since) >= BREACH_MS) {
                relay_set(0);
                buzzer_pattern(now, 0);
                deter_on = 0;
                enter_state(ST_COOLDOWN, now);
            }
            break;

        case ST_COOLDOWN:
            led_set(0);
            lcd_top = "COOLDOWN";
            relay_set(0);
            buzzer_pattern(now, 0);
            ir_flag = 0;
            if ((now - state_since) >= COOLDOWN_MS) {
                trigger_mask = 0;
                enter_state(ST_SECURE, now);
            }
            break;

        default:
            enter_state(ST_SECURE, now);
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
        if ((now - last_beacon) >= 1000u) {
            last_beacon = now;
            send_status();
        }
    }
}
