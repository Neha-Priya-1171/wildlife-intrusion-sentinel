# Live simulation - hardware bridged to a GUI

Break the IR beam with your hand and an animal walks out of the forest into the farm on
screen. The firmware confirms the breach, the real buzzer and siren fire on the bench, and
at the same instant the GUI lights its siren and drops a Bluetooth alert onto the farmer's
phone.

Nothing here touches `firmware/app/main.c`. That file is the submission firmware and is
left exactly as it is.

```
  hand breaks beam
        |
        v
   STM32 (sim_main.c) --USART1--> HC-05 --Bluetooth--> Windows COM port --> swis_gui.py
        |                                                                        |
        +--> buzzers PA6 + PB0 (+ siren via PA7 if fitted)       forest/farm animation,
             (the real noise, on the bench)                      siren, phone alert card
```

## What is in here

| Path | What it is |
|------|-----------|
| `firmware/sim_main.c` | The simulation build of the firmware. A copy of `main.c` with fast machine-readable telemetry added. Same pins, same state machine, same timings. |
| `swis_gui.py` | The desktop GUI. Python + tkinter. |
| `tools/gen_sim.py` | Regenerates `sim_main.c` from `main.c` if `main.c` ever changes. |

## Quick start - no hardware needed

You can show the whole thing before the sensors are working:

```
python simulation/swis_gui.py --demo
```

Press **SPACE** to trigger an intrusion. Everything animates exactly as it would with the
real board, because the demo pushes the same telemetry lines through the same code path.

## Running it against the hardware

**1. Flash the simulation firmware.** New Keil target, two files:
`simulation/firmware/sim_main.c` + `firmware/common/board.c`. Same options as any other
target (define `STM32F401xC`, `firmware/common` on the include path, C99).

**2. Pair the HC-05 with Windows.** Settings → Bluetooth → Add device → `HC-05`,
PIN `1234` (sometimes `0000`). Windows then creates two COM ports for it; you want the
**outgoing** one.

**3. Find the port.**

```
pip install pyserial
python simulation/swis_gui.py --list
```

**4. Run it.**

```
python simulation/swis_gui.py --port COM5
```

Add `--sound` to make the PC speaker chirp along with the siren - handy if the real siren
is not wired up yet.

## What you will see

| On the bench | On screen |
|--------------|-----------|
| hand crosses the IR beam | beam turns red, "BEAM BROKEN", boar walks out of the forest |
| firmware enters VALIDATE | amber "CHECKING SECOND SENSOR..." banner |
| PIR or vibration also fires | that sensor's lamp flashes |
| breach confirmed, buzzer + siren sound | strobe pulses red/blue, buzzer throws sound arcs, boar turns and bolts for the trees |
| alert sent over Bluetooth | notification card appears on the phone panel |
| 10 s later, deterrent stops | COOLDOWN, then back to SECURE |

The LDR drives the sky: when the firmware reports `NIGHT` the scene goes dark and the moon
comes out.

## Keys

| Key | Does |
|-----|------|
| `SPACE` | demo mode only - trigger an intrusion |
| `S` | ask the board for a status line |
| `R` | reset to SECURE |
| `T` | 2 second deterrent test |
| `D` | toggle demo mode **on the board** - see below |

## Getting it to actually trigger

The firmware is deliberately hard to fool: **in daylight it wants two sensors to agree**
before it calls a breach. That is the anti-false-alarm rule, and it means breaking the IR
beam on its own will show `CHECKING...` and then go back to SECURE without an alarm.

Three ways to get a full breach:

1. **Break the beam and wave at the PIR** within 2 seconds. This is the honest demo.
2. **Press `D`** in the GUI. That toggles demo mode on the board, where one sensor is
   enough at any time of day. Best for a presentation where you only have one hand free.
3. **Do it in the dark.** At night one sensor is enough by design.

## The one real limitation

**An HC-05 accepts a single Bluetooth connection at a time.** While the laptop is
connected and feeding the GUI, your phone cannot also be connected, and vice versa. There
is no way around this with one HC-05.

So the phone panel in the GUI *is* the farmer's phone for this demo - it shows exactly the
alert that would arrive, at the moment it is sent. If you want to show a real handset as
well, do it as a separate run: disconnect the laptop, pair the phone, open a serial
terminal app, and break the beam. The same `ALERT,ZONE1,ANIMAL BREACH` line arrives there.

## Telemetry format

Plain text, one record per line, CRLF. Documented at the top of `sim_main.c`.

```
EV,SRC=IR                a sensor fired - sent immediately, this is what drives the animation
ST,BREACH,DETER=1        state changed; DETER says whether buzzer + siren are running
ALERT,ZONE1,ANIMAL BREACH
SWIS,STATE=SECURE,BREACH=3,FALSE=7,LDR=1842,DAY,DEMO=0      heartbeat, 1 Hz
```

Event lines go out the instant the sensor fires rather than waiting for the next
heartbeat - that is what keeps the animal on screen in step with your hand.

## If it does not work

| Symptom | Cause |
|---------|-------|
| `could not open COM5` | Wrong port, or the phone is still connected to the HC-05. Only one at a time. |
| Port opens, nothing arrives | You flashed `main.c` instead of `sim_main.c`. Only the simulation build sends `EV,`/`ST,` lines. |
| `pyserial is not installed` | `pip install pyserial` |
| Beam breaks, animal walks in, no alarm | Working as designed - daylight needs two sensors. Press `D`. |
| Window opens but stays empty | No telemetry arriving. Check the HC-05 LED: fast blink = unpaired, slow = connected. |

## Status

Written and tested on the desktop side: the parsing, the animal state machine and the full
render path all pass automated checks, and the GUI runs clean in demo mode.

**Never run against the real board.** The STM32 half has not been flashed or tested, and
the Bluetooth link has not been exercised end to end.
