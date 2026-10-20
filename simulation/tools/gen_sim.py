"""Generate simulation/firmware/sim_main.c from firmware/app/main.c.

main.c is the submission firmware and is never modified. sim_main.c is the same code
with machine-readable telemetry switched on, so the desktop GUI can parse it.

This used to be sixteen fragile text patches that broke every time main.c changed.
It is now two: swap the file header, and flip MACHINE_TELEMETRY from 0 to 1. All the
behaviour differences live behind that one #if inside main.c, where they can be seen
and compiled by anyone reading it.

Run from the repo root:  python simulation/tools/gen_sim.py
"""
import io
import sys

SRC = 'firmware/app/main.c'
DST = 'simulation/firmware/sim_main.c'

NEW_HEADER = '''/**
 * @file    sim_main.c
 * @brief   SIMULATION BUILD of the Wildlife Intrusion Sentinel firmware.
 *
 * DO NOT EDIT. Generated from firmware/app/main.c by simulation/tools/gen_sim.py.
 * Change main.c and re-run that script.
 *
 * This is byte-for-byte the submission firmware with ONE line changed:
 *
 *     #define MACHINE_TELEMETRY  1      (main.c has 0)
 *
 * which swaps the human-readable Bluetooth output for terse machine-readable lines
 * that simulation/swis_gui.py parses. Pin map, state machine, timings, validation
 * rule and deterrent behaviour are all identical. Flash whichever you want.
 *
 * BUILD: simulation/firmware/sim_main.c + firmware/common/board.c
 *        (same Keil options and include path as main.c)
 *
 * TELEMETRY PROTOCOL - every line ends CR LF.
 *     EV,SRC=IR                  a sensor fired, sent the instant it happens
 *     ST,<STATE>,DETER=0|1       state changed; DETER = deterrent running
 *     ALERT,ZONE1,ANIMAL BREACH  the farmer notification
 *     SWIS,STATE=..,BREACH=..,FALSE=..,AWAY=..,LDR=..,ZONE=A|B,DEMO=0|1,DAY|NIGHT
 *
 * Event lines go out the instant a sensor fires, so the animal on screen starts
 * moving in step with the hand that broke the beam.
 */
'''

FLAG_OFF = '#define MACHINE_TELEMETRY       0'
FLAG_ON = '#define MACHINE_TELEMETRY       1'


def main():
    s = io.open(SRC, encoding='utf-8').read()

    marker = ' */\n#include "board.h"'
    if marker not in s:
        sys.exit('could not find the end of main.c\'s header comment')
    s = NEW_HEADER + s[s.index(marker) + len(' */\n'):]

    if FLAG_OFF not in s:
        sys.exit('main.c no longer contains %r - has the flag been renamed?' % FLAG_OFF)
    s = s.replace(FLAG_OFF, FLAG_ON, 1)

    io.open(DST, 'w', encoding='utf-8', newline='\n').write(s)
    print('wrote %s (%d lines)' % (DST, len(s.splitlines())))


if __name__ == '__main__':
    main()
