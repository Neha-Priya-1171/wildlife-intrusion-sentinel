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
6. **Low-voltage strobe only** on the relay. No mains wiring on this prototype.
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

### Stage 2 - IR break-beam (PA0) · `test_ir_sensor.c`

> ✅ **Status: COMPLETE.** Transmitter and receiver stages are both built, and `test_ir_sensor.c` has been run and passes on hardware. The steps below are kept as the build record and for rebuilding the stage if the wiring is disturbed.

This build uses a **5 mm IR LED (transmitter) and IR photodiode (receiver) pair**, not a ready-made module. The photodiode alone cannot drive PA0, so a one-transistor receiver stage is added. Build the two halves separately.

**Identify the parts:** the IR LED is usually clear or light blue; the photodiode is usually dark/black. For both, the **longer leg is the anode (+)**. Confirm with the multimeter diode test if unsure.

**A. Transmitter (always on)**

| From | To |
|------|----|
| 5 V rail | 220 Ω resistor |
| 220 Ω resistor | IR LED **anode** (long leg) |
| IR LED **cathode** | GND |

The IR LED is invisible. Check it by looking at it through a phone camera: it shows a faint purple glow.

**B. Receiver stage (BC547/2N2222, `Q2`)**

| Connection | Detail |
|------------|--------|
| Photodiode **cathode** (short leg) | 3.3 V |
| Photodiode **anode** (long leg) | `Q2` **base** |
| `Q2` **emitter** | GND |
| `Q2` **collector** | 10 kΩ → 3.3 V (pull-up) |
| `Q2` **collector** | **PA0** |

Check the transistor pinout against the datasheet or the diode test before wiring (see Stage 7).

**How it works:** with the beam clear, the photodiode passes current into the base, `Q2` turns on and PA0 sits **LOW**. When the beam is broken the current stops, `Q2` turns off and PA0 rises to 3.3 V (**HIGH**). The firmware is set for this (`IR_ACTIVE_LOW 0`, `IR_EXT_PULLUP 1` in `board.h`). PA0 never exceeds 3.3 V.

**Check with the multimeter before flashing:** measure PA0 to GND.
* Beam clear: close to **0 V**.
* Beam blocked with a card: close to **3.3 V**.

**Alignment and tuning:** face the LED and photodiode directly at each other, 5-10 cm apart for the first test, then increase the distance. Shield the receiver from direct sunlight or bright lamps, which also send IR. If PA0 stays HIGH with the beam clear, move closer or improve alignment. If PA0 stays LOW when the beam is blocked, shield the receiver from stray light (a short piece of black tube or heat-shrink over the photodiode helps) or add a 100 kΩ resistor from base to GND.

**Test:** flash `test_ir_sensor.c`. **Expect:** LED steady ON; breaking the beam makes the LED blink 6 times. Add `breach_count` to the Watch window to see the interrupt counter increase.

**Fallback:** if the discrete circuit cannot be made reliable, use a ready-made IR break-beam or obstacle-sensor module on PA0 and set `IR_ACTIVE_LOW` to `1` and `IR_EXT_PULLUP` to `0` in `board.h`.

**Polarity:** if the LED blinks when the beam is *clear* or never reacts, flip `IR_ACTIVE_LOW`.

> Note: PA0 also has the Black Pill's onboard **KEY button**. Pressing it will look like a trigger; that is normal and handy for a quick test.

### Stage 3 - PIR (PA1) and vibration sensor (PA2) · `test_sensors.c`

**PIR HC-SR501**

| PIR pin | To |
|---------|----|
| VCC | 5 V |
| GND | GND |
| OUT | **PA1** - *only after the voltage check below passes* |

> ⚠ **PA1 is not 5 V tolerant.** PA1 carries ADC1_IN1, so its I/O structure is **TTa** in the STM32F401xB/C datasheet pin table, not FT. Absolute maximum on a TTa pin is VDD + 0.3 V ≈ 3.6 V. A genuine HC-SR501 has an onboard 3.3 V regulator and its OUT swings 0 / 3.3 V, which is fine - but clones and lookalike modules exist that swing to VCC. **Measure OUT before you plug it into PA1.**

**Mandatory check before connecting OUT (do this with OUT unplugged from the MCU):**

1. Power the module, black probe on the module's own GND pin.
2. Measure module VCC→GND. It should read 4.8-5.1 V. A reading well below that means the MB102 rail is sagging.
3. Measure OUT→GND, idle and while waving a hand, for at least 60 s.
   * **0 V idle, ~3.3 V on motion** → correct, connect to PA1.
   * **Anything above 3.3 V** → do **not** connect it to PA1. See "PA1 stuck HIGH" below.
   * **A level that never changes** → the module is not operating; the fault is upstream of the firmware.

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

#### Diagnosing "PA1 always reads HIGH" · `test_pir.c`

`test_sensors.c` mixes two sensors and a 30 s blocking warm-up, which makes a stuck pin hard to see. Build **`test_pir.c` + `common/board.c`** instead: it watches PA1 only, samples from t = 0, and never blocks.

The variable that settles the question is **`pir_low_seen`**. It latches to 1 the first time PA1 reads 0. A PIR merely being *held* high by its hold-time pot still drops low eventually, so `pir_low_seen` becomes 1. If it stays 0 for several minutes, PA1 never goes low at all and the cause is electrical.

Run these three passes, editing `PIR_PULL_MODE` at the top of the file and re-flashing each time.

| Pass | `PIR_PULL_MODE` | PIR OUT wire | Expected `pir_raw` | If it differs |
|------|-----------------|--------------|--------------------|---------------|
| 1 | `BRD_PULL_DOWN` | **unplugged from PA1** | `0` | PA1 reads 1 with nothing attached and an internal pull-down active → the pin itself is damaged (likely from over-voltage) or a stray wire is driving it. Move the PIR to a spare pin such as PB0 and retest. |
| 2 | `BRD_PULL_UP` | **unplugged from PA1** | `1` | PA1 cannot be pulled high → broken wire to the MCU, or wrong pin on the board. Confirms the Watch plumbing works. |
| 3 | `BRD_PULL_DOWN` | connected | toggles with motion | Still stuck at 1 → the module is driving the line high permanently. Go back to the OUT→GND measurement above. |

Passes 1 and 2 test the **MCU pin in isolation**, with no sensor attached. Run them first; they take two minutes and they tell you whether you are debugging the STM32 or the sensor.

**Reading 4.3 V on OUT.** That is roughly 5 V minus one diode drop, which most often means the module is reverse-powered - VCC and GND swapped. Module pin order is not standardised: some are VCC-OUT-GND, some GND-OUT-VCC, and the silkscreen is usually hidden under the lens. Pull the dome off and read the labels on the PCB before trusting the order. A reverse-powered module typically sits at a fixed junk output level and never detects, which matches a stuck reading that two different sensors share and that survives changing both pots. Other possibilities: the module is a clone with no 3.3 V regulator (OUT swings to VCC), or the pin being probed is not OUT.

**Do not** fix a 4.3 V output with a bare resistive divider until you know why it is 4.3 V. If the module genuinely has a 5 V push-pull output, a 10 kΩ / 20 kΩ divider from OUT to PA1 to GND is appropriate. If it is 4.3 V because the supply is reversed, a divider just feeds PA1 a quieter wrong signal.

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

**Test 2 - the strobe siren** now has its own stage, because it is the first test that puts a live 12 V load on the relay contacts. Finish Test 1 above (relay clicking with **nothing** attached) before going there. See **Stage 8**.

**If it fails:** buzzer constantly on → transistor wrong way / base shorted; buzzer silent → buzzer polarity reversed or transistor pinout wrong; relay chatters or resets the MCU → supply sagging: check MB102 current, add decoupling capacitors, keep the strobe off the MB102 rails.

### Stage 8 - 12 V strobe siren (PA7, via the relay) · `test_siren.c`

> ⚠ **Status: NOT YET TESTED.** The siren has been received and the test program is written,
> but it has never been run. Nothing below is a verified result.

**The part:** Mini Strobe Wired Siren, marked **A-12-B**, red, two flying leads.
The packaging states **12 V**. It does **not** state a current rating, so the current is
unknown until you measure it. Units of this size typically draw in the region of 100-300 mA,
but treat that as an expectation to check, not a specification.

**Supply:** VMAX SMPS adaptor, AC 100-240 V in, **12 V / 1 A** out. This single adapter feeds
both the MB102 and the siren, so the two together must stay under 1 A.

#### Safety rules for this stage

* The siren is **never** connected to a GPIO pin, to 3V3, to 5V, or to any MB102 rail. Only the
  relay contacts switch it, straight from the 12 V adapter.
* Keep the mains side of the adapter untouched. The adapter is marked *for indoor use only*.
* This stage stays entirely at 12 V DC. **Do not put mains voltage on the relay contacts.**
* It is loud. Test it briefly, and not next to your ear.

#### Pre-power checks (do all of these before applying 12 V)

1. **Confirm the relay contact rating** printed on the relay module. Typical boards are marked
   10 A 250 VAC / 10 A 30 VDC, which is far above what the siren needs. If yours is marked
   lower, check it still exceeds the measured siren current with margin.
2. **Confirm the relay input works at 3.3 V logic.** `board.h` has `RELAY_ACTIVE_LOW 1`, and the
   firmware drives PA7 **open-drain**, so the MCU only ever pulls the input to GND and never
   tries to present a 3.3 V "high" to a 5 V-referenced input. This is the arrangement that
   works reliably; Test 1 in Stage 7 is what proves it on your particular board.
3. **Check the JD-VCC jumper** if your relay module has one. With the jumper fitted, VCC and
   JD-VCC are tied and the opto-isolation is bypassed. That is acceptable here because
   everything shares one ground anyway, but know which case you are in.
4. **Identify siren polarity.** The red lead is **+**. Confirm with the multimeter if the second
   lead is not obviously black. Getting this backwards on a DC siren usually just means silence,
   but do not rely on that.
5. **Measure the siren current first, without the relay.** Briefly connect the siren straight
   across the 12 V adapter with the multimeter in series on the **10 A / A** range. Record the
   reading. Then subtract it from 1 A - what is left is your budget for the MB102 and
   everything on its rails.

#### Wiring

| Connection | Detail |
|------------|--------|
| 12 V adapter **+** | relay **COM** |
| relay **NO** | siren **+** (red lead) |
| siren **−** (second lead) | 12 V adapter **−** |
| relay **VCC** | 5 V rail |
| relay **GND** | common ground |
| relay **IN** | **PA7** |

The siren is energised only while the relay is active, so **NO** (normally open) is correct;
on **NC** it would scream whenever the board was off.

*Optional but recommended:* a 1N4007 across the siren leads, cathode to **+**, to catch any
inductive kick when the contacts open. A mini strobe siren is mostly electronic rather than a
plain coil, so this may do nothing - it is cheap insurance for the relay contacts either way.

#### Procedure

1. Wire everything with the 12 V adapter **unplugged**. Double-check COM/NO against the table.
2. Build **`test_siren.c` + `common/board.c`** as its own Keil target and flash it.
3. Keep the 12 V adapter unplugged for the first run. The on-board LED mirrors the relay, so you
   get the whole sequence silently and can confirm the firmware before anything is loud.
4. Plug in the 12 V adapter and press reset. **Expect:** LED blinks for 5 s (arming, silent) →
   3 bursts of ~300 ms with 3 s of silence between them → one 2 s burst → relay off, sequence
   ends. The siren should be silent from then on.
5. Add `siren_cmd`, `siren_on`, `siren_on_count` and `siren_guard_tripped` to the Keil Watch
   window. Write **1** to `siren_cmd` to switch the siren ON, **0** to switch it OFF. No reflash
   needed. If you leave it at 1, the firmware forces it off after 30 s and sets
   `siren_guard_tripped`.

#### Expected results

| Relay state | PA7 | Siren | LED (PA5) | `siren_on` |
|-------------|-----|-------|-----------|------------|
| OFF | released (high-Z, pulled high by the module) | silent, no strobe | off | 0 |
| ON | pulled LOW by the MCU | sounding + flashing | on | 1 |

**If it fails:** relay clicks but the siren stays dead → siren polarity reversed, or the load is
on NC instead of NO, or the adapter is not actually delivering 12 V under load (measure it while
the relay is on). Siren sounds weakly or the MCU resets when it fires → the adapter is being
overloaded: the siren plus the MB102 exceeds 1 A, so power the siren from a separate supply.
Siren sounds the moment power is applied, before the sequence → load wired to NC, or
`RELAY_ACTIVE_LOW` is set wrong for your board.

---

## 4. Power-budget sanity check

| Load | Approx. current |
|------|-----------------|
| Relay coil | 70-90 mA |
| HC-05 | 30-40 mA |
| LCD + backlight | 20-30 mA |
| Buzzer | ~30 mA |
| IR LED (220 Ω) + receiver stage | ~20 mA |
| PIR / SW-420 / LDR / STM32 | < 50 mA total |
| **Siren A-12-B (12 V side, not an MB102 load)** | **unknown - measure it (expect roughly 100-300 mA)** |

A few hundred mA in total is fine for the MB102, **but the siren must not be powered from its rails** (it is switched from the 12 V adapter through the relay).

**The 1 A adapter is shared.** One VMAX 12 V / 1 A supply feeds the MB102 input *and* the siren through the relay contacts. Budget it as `MB102 input current + siren current < 1 A`. The siren figure is still unknown, so measure it (Stage 8, pre-power check 5) before any sustained run. If the total is tight, give the siren its own 12 V supply and just share the ground.

Linear regulators dissipate `(Vin − Vout) × I` as heat: with a 12 V adapter the 5 V regulator can get hot. Do a finger-touch check after a minute; if too hot, use a lower input voltage (e.g. 9 V) or a heat sink.

## 5. Final integration checklist (before powering the full system)

- [ ] 3.3 V rail = 3.3 V ± 0.1 V, 5 V rail = 5 V ± 0.25 V (measured with everything connected)
- [ ] 0 Ω / beep between GND of every module, the Black Pill and the ST-LINK
- [ ] PA0, PA1, PA2 and PA4 each measured ≤ 3.3 V in every sensor state (PA0: about 0 V beam clear, about 3.3 V beam blocked)
- [ ] LCD level shifter: LV = 3.3 V, HV = 5 V, pull-ups verified on both sides
- [ ] HC-05 TXD→PA10, RXD→PA9 (crossed), VCC = 5 V
- [ ] Buzzer transistor orientation verified, 1 kΩ in series with the base
- [ ] Relay polarity confirmed, siren switched from the 12 V adapter (not the MB102 rails)
- [ ] Siren current measured and recorded; `MB102 input + siren < 1 A` on the shared adapter
- [ ] Siren on relay **NO** (not NC), red lead to **+**, relay contact rating checked
- [ ] All stage tests pass **individually**, including Stage 8 (siren)
- [ ] Wires tidy, joints insulated, nothing loose near the relay contacts

## 6. Quick diagnostics table

| Symptom | Most likely cause |
|---------|-------------------|
| Nothing works, board dead | No power, rails miswired, grounds missing |
| Random resets when relay/buzzer fires | Supply sag / noise → decoupling caps, strobe off the MB102 rails |
| One sensor always "triggered" | Wrong polarity macro, floating input, trimmer pot mis-set |
| LCD blank | Contrast pot, wrong I2C wiring, level-shifter supplies swapped |
| Bluetooth connects but no text | TX/RX not crossed, wrong baud, wrong app mode |
| Can't flash | See *Troubleshooting* in [`keil_setup.md`](keil_setup.md) |
