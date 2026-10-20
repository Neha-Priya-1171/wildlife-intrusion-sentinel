"""Generate simulation/firmware/sim_main.c from firmware/app/main.c.

main.c is the submission firmware and is never modified. sim_main.c is a derived copy
with machine-readable telemetry added for the desktop GUI. Re-run this script if main.c
changes and the two need to be brought back in sync.
"""
import io
import sys

SRC = 'firmware/app/main.c'
DST = 'simulation/firmware/sim_main.c'
CRLF = chr(92) + 'r' + chr(92) + 'n'   # the C escape \r\n as literal source text

NEW_HEADER = '''/**
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
 *     PA0  IR receiver      EXTI0 interrupt      PA6  Buzzer 1 (direct, no NPN)
 *     PB5  PIR OUT          polled input         PA7  Relay -> 12 V siren
 *     PA2  SW-420 DO        polled input         PA9  HC-05 RXD (USART1 TX)
 *     PA4  LDR AO           ADC1_IN4             PA10 HC-05 TXD (USART1 RX)
 *     PA5  Green LED        SECURE indicator     PB6/PB7 LCD SCL/SDA (I2C1)
 *     PB0  Buzzer 2 (zone B, optional)          PA5 = Zone A LED, PA8 = Zone B LED
 *
 * ZONES: each side of the farm has a buzzer and an LED. The LED says which side was
 * breached; the buzzers only make noise, so a dead buzzer never costs the location.
 * The heartbeat carries ZONE=A or ZONE=B so the GUI can show the right side.
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
'''

REPS = [
    # ---- extra globals ----
    ("volatile uint8_t  lcd_ok       = 0;   /* 0 = no LCD found; system runs without it    */\n",
     "volatile uint8_t  lcd_ok       = 0;   /* 0 = no LCD found; system runs without it    */\n"
     "volatile uint8_t  deter_on     = 0;   /* 1 while the buzzer and siren are running    */\n"
     "\nstatic uint32_t state_since = 0;      /* millis() when the current state began       */\n"),

    # ---- hoist the state-name table out of send_status ----
    ('static void send_status(void)\n{\n'
     '    static const char *const names[] = { "WARMUP", "SECURE", "VALIDATE", "BREACH", "COOLDOWN" };\n'
     '    uart_puts("SWIS,STATE="); uart_puts(names[state]);\n',
     'static const char *const state_names[] = { "WARMUP", "SECURE", "VALIDATE", "BREACH", "COOLDOWN" };\n\n'
     'static void send_status(void)\n{\n'
     '    uart_puts("SWIS,STATE="); uart_puts(state_names[state]);\n'),

    # ---- heartbeat gains DEMO, plus the two new helpers ----
    ('    uart_puts(is_night ? ",NIGHT' + CRLF + '" : ",DAY' + CRLF + '");\n}\n',
     '    uart_puts(is_night ? ",NIGHT' + CRLF + '" : ",DAY' + CRLF + '");\n}\n\n'
     '/* Sent the moment a sensor fires, so the GUI animation starts in step with the event\n'
     ' * instead of waiting for the next heartbeat. */\n'
     'static void emit_event(const char *src)\n{\n'
     '    uart_puts("EV,SRC="); uart_puts(src); uart_puts("' + CRLF + '");\n}\n\n'
     '/* The single place the state changes, so every transition is announced exactly once. */\n'
     'static void enter_state(state_t s, uint32_t now)\n{\n'
     '    state       = s;\n'
     '    state_since = now;\n'
     '    uart_puts("ST,"); uart_puts(state_names[state]);\n'
     '    uart_puts(deter_on ? ",DETER=1' + CRLF + '" : ",DETER=0' + CRLF + '");\n}\n'),

    # ---- main(): state_since is now file scope ----
    ("    uint32_t now;\n    uint32_t state_since = 0;     /* millis() when the current state began       */\n",
     "    uint32_t now;\n"),
    ("    uint8_t  pir_prev, vib_prev;\n",
     "    uint8_t  pir_prev, vib_prev;\n"
     "    uint32_t ir_seen = 0;         /* last value of ir_events we reported          */\n"),
    ("    pir_prev    = pir_active();\n    vib_prev    = vib_active();\n"
     "    state       = ST_WARMUP;\n    state_since = millis();\n",
     "    pir_prev = pir_active();\n    vib_prev = vib_active();\n"
     "    enter_state(ST_WARMUP, millis());\n"),

    # ---- announce every sensor edge immediately ----
    ("            if (p && !pir_prev) pir_events++;\n            if (v && !vib_prev) vib_events++;\n",
     '            if (p && !pir_prev) { pir_events++; emit_event("PIR"); }\n'
     '            if (v && !vib_prev) { vib_events++; emit_event("VIB"); }\n'
     '            if (ir_events != ir_seen) { ir_seen = ir_events; emit_event("IR"); }\n'),

    # ---- commands ----
    ('                    state = ST_SECURE; state_since = now; trigger_mask = 0;\n'
     '                    uart_puts("Reset to SECURE' + CRLF + '");\n',
     '                    deter_on = 0;\n                    trigger_mask = 0;\n'
     '                    enter_state(ST_SECURE, now);\n'),

    # ---- state transitions all go through enter_state() ----
    ('            if ((now - state_since) >= PIR_WARMUP_MS) {\n'
     '                state = ST_SECURE; state_since = now;\n'
     '                uart_puts("Warmup done. Armed.' + CRLF + '");\n            }\n',
     "            if ((now - state_since) >= PIR_WARMUP_MS) {\n"
     "                enter_state(ST_SECURE, now);\n            }\n"),
    ("                state = ST_VALIDATE; state_since = now;\n",
     "                enter_state(ST_VALIDATE, now);\n"),
    ('            if (votes >= needed) {\n                state = ST_BREACH; state_since = now;\n'
     '                breach_count++;\n                relay_set(1);\n'
     '                uart_puts("ZONE 1: ANIMAL BREACH' + CRLF + '");\n                send_status();\n',
     '            if (votes >= needed) {\n                breach_count++;\n'
     '                relay_set(1);\n                deter_on = 1;\n'
     '                enter_state(ST_BREACH, now);\n'
     '                uart_puts("ALERT,ZONE1,ANIMAL BREACH' + CRLF + '");\n                send_status();\n'),
    ('            } else if ((now - state_since) >= VALIDATE_MS) {\n                false_alarms++;\n'
     '                state = ST_SECURE; state_since = now;\n'
     '                uart_puts("Unconfirmed trigger ignored' + CRLF + '");\n            }\n',
     "            } else if ((now - state_since) >= VALIDATE_MS) {\n                false_alarms++;\n"
     "                enter_state(ST_SECURE, now);\n            }\n"),
    ("                relay_set(0);\n                buzzer_pattern(now, 0);\n"
     "                state = ST_COOLDOWN; state_since = now;\n",
     "                relay_set(0);\n                buzzer_pattern(now, 0);\n"
     "                deter_on = 0;\n                enter_state(ST_COOLDOWN, now);\n"),
    ("            if ((now - state_since) >= COOLDOWN_MS) {\n"
     "                state = ST_SECURE; state_since = now;\n                trigger_mask = 0;\n            }\n",
     "            if ((now - state_since) >= COOLDOWN_MS) {\n"
     "                trigger_mask = 0;\n                enter_state(ST_SECURE, now);\n            }\n"),
    ("        default:\n            state = ST_SECURE; state_since = now;\n            break;\n",
     "        default:\n            enter_state(ST_SECURE, now);\n            break;\n"),

    # ---- 1 Hz heartbeat so the GUI numbers feel live ----
    ("        if ((now - last_beacon) >= 5000u) {\n",
     "        if ((now - last_beacon) >= 1000u) {\n"),
]


def main():
    s = io.open(SRC, encoding='utf-8').read()
    s = NEW_HEADER + s[s.index(' */\n#include "board.h"') + len(' */\n'):]
    for i, (a, b) in enumerate(REPS):
        if a not in s:
            sys.exit('patch %d does not apply - main.c has changed:\n%r' % (i, a[:80]))
        s = s.replace(a, b, 1)
    io.open(DST, 'w', encoding='utf-8', newline='\n').write(s)
    print('wrote %s (%d lines, %d patches)' % (DST, len(s.splitlines()), len(REPS)))


if __name__ == '__main__':
    main()
