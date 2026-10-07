# Assembly & Verification Guide

A beginner-friendly, **build-one-block-then-test-it** guide. Flash the matching file from `firmware/tests/` after each stage (setup steps: [`keil_setup.md`](keil_setup.md)). Pin choices are listed in the [README pin map](../README.md#5-master-pin-mapping).

---

## 0. Before you start

**Tools:** multimeter (continuity + DC volts), small screwdriver, wire stripper, tweezers, optional soldering iron.

**Golden rules**

1. **Power OFF whenever you change wiring.** Unplug the 12 V adapter first.
2. **Check polarity** (VCC/GND) twice before switching on. Reversed power kills modules.
3. **Common ground**: every module GND, the MCU GND and the ST-LINK GND must be tied together.
4. **Only one power source for the Black Pill** at a time (MB102 3.3 V *or* USB-C).
5. Handle boards by the edges; touch a metal object first to discharge static.
6. **Low-voltage lamp only** on the relay. No mains wiring on this prototype.
7. Use a colour code: **red = 5 V, orange = 3.3 V, black = GND, other colours = signals**.

---

## 1. Prepare the parts

1. **Black Pill:** solder the header pins (long pins down into the breadboard). Check for solder bridges with the multimeter beeper between adjacent pins.
2. **MB102 module:** plug it into the breadboard's end power rails (check the orientation: `+`/`-` on the module match the rails). Set the two jumpers so that **one rail = 3.3 V** and the other **= 5 V**. Do not connect the adapter yet.
3. **Voltage check (adapter on, nothing else connected):** switch on and measure each rail against GND: **3.3 V ± 0.1** and **5.0 V ± 0.25**. Fix before proceeding. Power off.
4. **Join the grounds:** run a black jumper between the GND (−) rail of the 3.3 V side and the GND (−) rail of the 5 V side so you have one ground grid. (Many breadboards split rails in the middle; bridge those gaps with jumpers.)
5. **Place the Black Pill** across the centre gap of the breadboard, leaving rows free on both sides. Connect: **3V3 pin → 3.3 V rail**, **GND pin → ground grid**. Leave the 5V pin unconnected.
6. **Decoupling (recommended):** a 100 nF capacitor between 3.3 V and GND right next to the Black Pill, and one 100 nF + 10 µF across the 5 V rail near the relay/buzzer.

**Black Pill checkpoint:** Power on. The board's power LED should light. If anything gets warm, power off at once and recheck.

## 2. Connect the ST-LINK

| ST-LINK | Black Pill |
|---------|-----------|
| SWDIO | DIO (PA13) |
| SWCLK | CLK (PA14) |
| GND | GND (ground grid) |
| 3.3 V | **leave disconnected** (MB102 powers the board) |

Plug the ST-LINK into the PC. Make sure BOOT0 is 0.

---

## 3. Stage-by-stage build and test

Each stage: **wire → power-check → flash test → expected result → troubleshooting.**

### Stage 1 - Green status LED (PA5) · `test_led.c`

| From | To |
|------|----|
| PA5 | 220 Ω resistor → LED **anode** (long leg) |
| LED **cathode** (short leg, flat side) | GND |

**Test:** flash `test_led.c`. **Expect:** 5 quick blinks, then LED steady ON for 3 s, repeating. This proves toolchain + flashing + GPIO all work.

**If it fails:** LED backwards (flip it); resistor missing; the project didn't start (tick *Reset and Run*, press reset); build used the wrong device.

### Stage 2 - Active IR break-beam (PA0) · `test_ir_sensor.c`

| From | To |
|------|----|
| IR receiver VCC | 3.3 V (if the module accepts 3.3-5 V) or 5 V - see below |
| IR receiver GND | GND |
| IR receiver OUT | **PA0** |
| IR emitter | power per its datasheet (usually same VCC/GND) |

**Before connecting OUT to PA0:** with the beam clear and powered, measure OUT to GND. It must read **≤ 3.3 V** both with the beam clear and broken. If it reads about 5 V, either power the sensor from 3.3 V (if allowed) or put a **10 kΩ/20 kΩ divider** (OUT → 10 kΩ → PA0, PA0 → 20 kΩ → GND). Open-collector (NPN) outputs are fine - the firmware turns on PA0's internal pull-up.

**Alignment:** face emitter and receiver directly at each other, 10-50 cm apart for the first test; mount on stands.

**Test:** flash `test_ir_sensor.c`. **Expect:** LED steady ON; breaking the beam makes the LED blink 6 times. Add `breach_count` to the Watch window to see the interrupt counter increase.

**Polarity:** if the LED blinks when the beam is *clear* or never reacts, set `IR_ACTIVE_LOW` to `0` in `board.h`.

**If it fails:** emitter/receiver not aligned (the module's indicator LED shows the state); grounds not common; trimmer pot needs adjusting; some beams need a few seconds to settle.

> Note: PA0 also has the Black Pill's onboard **KEY button**. Pressing it will look like a trigger; that is normal and handy for a quick test.

### Stage 3 - PIR (PA1) and vibration sensor (PA2) · `test_sensors.c`

**PIR HC-SR501**

| PIR pin | To |
|---------|----|
| VCC | 5 V |
| GND | GND |
| OUT | **PA1** (output is 3.3 V, safe) |

Set the module's jumper to *repeat trigger (H)* and turn both trimmers to the middle (one sets sensitivity/range, one sets hold time - turn hold time to minimum for testing).

**SW-420 vibration module**

| SW-420 pin | To |
|------------|----|
| VCC | **3.3 V** (so DO stays ≤ 3.3 V) |
| GND | GND |
| DO | **PA2** |

Turn the blue trimmer until the module's indicator LED is just off when it is still, and flickers when you tap the table.

**Test:** flash `test_sensors.c`. **Expect:** LED blinks slowly for ~30 s (PIR warm-up; keep still / away from the sensor), then:
* wave a hand → **one long blink** (PIR)
* tap the sensor/table → **two quick blinks** (vibration)

Watch `pir_events` / `vib_events` in the debugger.

**If it fails:** PIR still warming up; PIR dome facing a heat source/sunlight; vibration too sensitive → turn pot to reduce, or too insensitive → turn pot the other way; polarity wrong → adjust `PIR_ACTIVE_HIGH` / `VIB_ACTIVE_HIGH`.

### Stage 4 - LDR light sensor (PA4) · `test_ldr.c` *(bonus test)*

| LDR module | To |
|------------|----|
| VCC | **3.3 V** |
| GND | GND |
| AO | **PA4** |

Check with the multimeter: AO must never exceed 3.3 V (covered or lit). **Expect:** cover the LDR → LED toggles. If your module is reversed (high when bright), invert the comparison in `test_ldr.c`. Adjust `DARK_THRESHOLD` from watching `adc_value`.

### Stage 5 - 16×2 I2C LCD (PB6/PB7) · `test_lcd.c`

The LCD backpack runs at 5 V while the STM32 is 3.3 V, so the **level shifter** sits in between.

| Level shifter pin | To |
|-------------------|----|
| LV (low-voltage reference) | 3.3 V |
| HV (high-voltage reference) | 5 V |
| GND (both sides) | GND |
| LV1 | **PB6** (SCL) |
| LV2 | **PB7** (SDA) |
| HV1 | LCD backpack **SCL** |
| HV2 | LCD backpack **SDA** |

LCD backpack: **VCC → 5 V**, **GND → GND**.

Both sides of the I2C bus need **pull-up resistors** (≈ 4.7-10 kΩ to their own supply). Most level-shifter boards and LCD backpacks already include them. Verify with the multimeter (power off): resistance between SDA and 3.3 V on the LV side, and between SDA and 5 V on the HV side, should read several kΩ - not open.

**Test:** flash `test_lcd.c`. **Expect:** `SWIS LCD TEST OK` and the detected I2C address (0x27 or 0x3F), then `STATUS: SECURE / BREACH` alternating with a counter.

**If it fails:**
* Backlight on but no text → turn the contrast trimmer on the backpack.
* Status LED blinking **2×, pause, 2×…** → no I2C acknowledge: check SDA/SCL not swapped, LV/HV supply pins, shared ground, pull-ups.
* Garbage characters → loose wire; keep I2C wires short.

### Stage 6 - HC-05 Bluetooth (PA9/PA10) · `test_bluetooth.c`

| HC-05 pin | To |
|-----------|----|
| VCC | 5 V |
| GND | GND |
| RXD | **PA9** (STM32 TX) |
| TXD | **PA10** (STM32 RX) |
| STATE, EN/KEY | not connected |

Remember **TX goes to RX and RX goes to TX** (crossed). The HC-05's RXD expects 3.3 V logic, which the STM32 provides, so no extra shifting is needed on this line.

**Test:**
1. Flash `test_bluetooth.c`. The HC-05 LED should **blink fast** (unpaired).
2. On an **Android** phone: Bluetooth settings → pair **HC-05** with PIN **1234** (or 0000). The HC-05 LED then blinks slowly (about every 2 s).
3. Open a Bluetooth serial terminal app, connect to HC-05 and set 9600 baud.
4. **Expect:** a line `SWIS,SEQ=n,STATUS=SECURE` every second. Send `1` / `0` → the PA5 LED switches. Send `B` → you get `ZONE 1: ANIMAL BREACH`.

**If it fails:** TX/RX not crossed; HC-05 on 3.3 V (needs 5 V); module configured for another baud rate (default data mode is 9600); phone is an iPhone (classic Bluetooth SPP is not supported - use Android or a PC terminal).

### Stage 7 - Buzzer (PA6) and relay (PA7) · `test_actuators.c`

**Buzzer driver (NPN low-side switch)**

| Connection | Detail |
|------------|--------|
| PA6 → 1 kΩ → transistor **base** | limits base current |
| transistor **emitter** → GND | |
| buzzer **−** → transistor **collector** | |
| buzzer **+** → 5 V | |

**Check the transistor pinout before wiring**: BC547 (TO-92, flat side facing you, legs down) is typically **C-B-E from left to right**; many 2N2222 (plastic) are **E-B-C**. Use your datasheet, or the multimeter diode test to find the base.

**Relay module**

| Relay pin | To |
|-----------|----|
| VCC | 5 V |
| GND | GND |
| IN | **PA7** |
| COM / NO / NC | **leave empty for the first test** |

**Test 1:** flash `test_actuators.c` with nothing connected to the relay contacts. **Expect:** 3 beeps, then 3 relay clicks, then a 6 s "breach" demo (relay on + varying beeps), repeat.

**Relay polarity:** the code assumes an **active-low** board and drives PA7 as open-drain (which avoids half-on relays when a 3.3 V "high" meets a 5 V-powered input). If the relay is ON while the program says OFF, set `RELAY_ACTIVE_LOW` to `0` in `board.h`.

**Test 2 - add the lamp (power OFF first):** wire the lamp's **own supply +** → relay **COM**; relay **NO** → lamp +; lamp − → supply −. The lamp turns on only when the relay clicks. The lamp supply's ground does **not** need to touch the STM32 ground (the relay is opto-isolated).

**If it fails:** buzzer constantly on → transistor wrong way / base shorted; buzzer silent → buzzer polarity reversed or transistor pinout wrong; relay chatters or resets the MCU → supply sagging: check MB102 current, add decoupling capacitors, power the lamp separately.

---

## 4. Power-budget sanity check

| Load | Approx. current |
|------|-----------------|
| Relay coil | 70-90 mA |
| HC-05 | 30-40 mA |
| LCD + backlight | 20-30 mA |
| Buzzer | ~30 mA |
| IR emitter/receiver | ~20-40 mA |
| PIR / SW-420 / LDR / STM32 | < 50 mA total |

A few hundred mA in total is fine for the MB102, **but the lamp must not be powered from it**. Linear regulators dissipate `(Vin − Vout) × I` as heat: with a 12 V adapter the 5 V regulator can get hot. Do a finger-touch check after a minute; if too hot, use a lower input voltage (e.g. 9 V) or a heat sink.

## 5. Final integration checklist (before powering the full system)

- [ ] 3.3 V rail = 3.3 V ± 0.1 V, 5 V rail = 5 V ± 0.25 V (measured with everything connected)
- [ ] 0 Ω / beep between GND of every module, the Black Pill and the ST-LINK
- [ ] PA0, PA1, PA2 and PA4 each measured ≤ 3.3 V in every sensor state
- [ ] LCD level shifter: LV = 3.3 V, HV = 5 V, pull-ups verified on both sides
- [ ] HC-05 TXD→PA10, RXD→PA9 (crossed), VCC = 5 V
- [ ] Buzzer transistor orientation verified, 1 kΩ in series with the base
- [ ] Relay polarity confirmed, lamp on separate supply
- [ ] All six tests (plus LDR) pass **individually**
- [ ] Wires tidy, joints insulated, nothing loose near the relay contacts

## 6. Quick diagnostics table

| Symptom | Most likely cause |
|---------|-------------------|
| Nothing works, board dead | No power, rails miswired, grounds missing |
| Random resets when relay/buzzer fires | Supply sag / noise → decoupling caps, separate lamp supply |
| One sensor always "triggered" | Wrong polarity macro, floating input, trimmer pot mis-set |
| LCD blank | Contrast pot, wrong I2C wiring, level-shifter supplies swapped |
| Bluetooth connects but no text | TX/RX not crossed, wrong baud, wrong app mode |
| Can't flash | See *Troubleshooting* in [`keil_setup.md`](keil_setup.md) |
