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
6. **Decoupling (recommended):** a 100 nF capacitor between 3.3 V and GND right next to the Black Pill, and one 100 nF + 10 µF across the 5 V rail near the relay. (The buzzers sit on GPIO pins, not the 5 V rail.)

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

### Stage 3 - PIR (PB5) and vibration sensor (PA2) · `test_pir.c`, `test_sensors.c`

> **The PIR has moved from PA1 to PB5.** PA1 read permanently HIGH and would not change.
> The firmware was checked line by line and is correct, so the fault is electrical. PB5 is
> free, and unlike PA1 it has no ADC function, which makes it an FT (5 V tolerant) pin - so
> the ~4.3 V measured on the module's OUT is within what PB5 accepts. Do not reuse PA1 until
> it has been proven good.

**PIR wiring - three wires, nothing else**

| PIR pin | To |
|---------|----|
| VCC | 5 V |
| GND | GND (same ground as the Black Pill) |
| OUT | **PB5** |

Set the module's jumper to *repeat trigger (H)*. Turn the sensitivity trimmer to the middle
and the hold-time trimmer to minimum for testing.

**Test:** build `test_pir.c` + `common/board.c` and flash it. The green LED copies the PIR
output directly: wave your hand and it should light, stand still and it should go out after a
few seconds. Wait out the 30-60 s warm-up before judging anything.

**Three variables to watch in Keil** (right-click each → *Add to Watch 1*):

| Variable | Meaning |
|----------|---------|
| `pir_state` | what the pin reads right now: 0 or 1 |
| `pir_events` | how many times motion has started; goes up by 1 per wave |
| `pir_low_seen` | 0 until the pin has been low at least once, then stays 1 |

**How to read it:**

* `pir_low_seen` becomes 1 and `pir_events` counts your waves → **the PIR works.**
* `pir_low_seen` stays 0 for several minutes → the pin is stuck high. Unplug the OUT wire from
  PB5 and look again: with nothing connected, `pir_state` must read 0. If it still reads 1 with
  the wire off, the problem is on the board side, not the sensor.

**SW-420 vibration module**

| SW-420 pin | To |
|------------|----|
| VCC | **3.3 V** (so DO stays ≤ 3.3 V) |
| GND | GND |
| DO | **PA2** |

Turn the blue trimmer until the module's indicator LED is just off when it is still, and
flickers when you tap the table.

`test_sensors.c` runs the PIR and the vibration sensor together once both are wired.

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

### Stage 7 - Buzzers (PA6 and PB0) · `test_buzzer.c`

Two 12 mm buzzers, each driven straight from its own GPIO pin. **No transistor, no relay,
no 5 V rail.** This is the project's deterrent now that the siren is deferred.

| Buzzer | + leg (marked, or the longer one) | − leg |
|--------|-----------------------------------|-------|
| Buzzer 1 | **PA6** | GND |
| Buzzer 2 | **PB0** | GND |

Four wires in total. Both − legs go to the same ground as the Black Pill.

> **One buzzer per pin, always.** A 12 mm buzzer pulls about **20 mA** at 3.3 V and an
> STM32F401 pin is rated for **25 mA**. Two on PA6 would be roughly 40 mA and over the
> limit. Using only one buzzer is fine - set `BUZ2_ENABLE` to `0` in `board.h` and leave
> PB0 alone.

> **Why no transistor?** The original design switched the buzzer from 5 V through an NPN.
> The only BC547 in the kit went into the IR receiver stage in Stage 2, and a built and
> aligned circuit is not worth dismantling for some volume. Direct from the pin the
> buzzers run at 3.3 V: quieter, but well inside the pin's rating.

**First power-up:** a **270 Ω** resistor in series with a + leg makes the test safe while
you are still unsure of the part. It will sound thin. Once it beeps, take it out and wire
the pin straight to **+** for full volume. The final build has no resistor.

**Active or passive?** The firmware only switches pins on and off, so you want **active**
buzzers (own oscillator → a tone). A **passive** one only clicks. If yours clicks, set
`BUZZER_IS_PASSIVE` to `1` in `test_buzzer.c` and it will generate the tone in software.
Bench check without flashing: touch **+** to **3V3** and **−** to **GND** - a steady tone
means active, a single click means passive.

**Test:** build `test_buzzer.c` + `common/board.c`. From reset:

| Part | What you hear | Tells you |
|------|---------------|-----------|
| 1 | buzzer 1 alone, three 300 ms beeps | PA6 and buzzer 1 are good |
| 2 | buzzer 2 alone, three 300 ms beeps | PB0 and buzzer 2 are good |
| 3 | both together, two 500 ms blasts | the full deterrent |
| 4 | six seconds of shifting cadence | what the alarm actually sounds like |

Then three seconds of silence and it repeats. The **PA5 LED is on whenever something
should be sounding**, so the test can be read by eye in a noisy room.

**If it fails:** part 1 silent → buzzer 1 polarity reversed (swap the legs), or it is the
passive one. Part 1 sounds but part 2 does not → PB0 wiring, not the code. Everything very
quiet → the 270 Ω test resistor is still fitted. Nothing at all, LED also dead → wrong
target built, or the board is not running.

Nothing in the Watch window is needed for this test.

### Stage 7b - Zone indicator LEDs (PA3 and PA8) · `test_zones.c`

The farm is split into two zones, one along each long edge of the breadboard. Each zone
has a buzzer **and** an LED.

| Zone | Watches | Buzzer | Indicator LED |
|------|---------|--------|---------------|
| **A** | IR beam (PA0) | PA6 | **PA5** - the status LED you already have |
| **B** | PIR (PB5) + vibration (PA2) | PB0 | **PA8** - one new LED |

**Only one new LED is needed.** Zone A reuses the green status LED already wired to PA5.

| LED | Wiring |
|-----|--------|
| Zone A | nothing to do - it is the existing PA5 LED |
| Zone B | **PA8** → 270 Ω → LED long leg; short leg → GND |

> **PA5 does two jobs**, so every LED decision in the firmware is made in one function
> (`indicators()` in `main.c`). Nothing else is allowed to touch PA5, or the status role
> and the zone role would overwrite each other on the same pin.
>
> **SOLID means "this zone is breached", and only that.** That is why SECURE now shows a
> brief heartbeat flash every 2 s instead of a steady light - so a steady LED always has
> exactly one meaning.

> **Why the LED carries the information, not the buzzer.** The LED says *which side* was
> breached. The buzzers only make noise, and every fitted buzzer sounds for either zone.
> So a dead buzzer costs you volume but never costs you the location - which is the part
> that actually matters. This is why the PB0 buzzer being silent does not hold the project
> up.
>
> **Both zone LEDs are on port A on purpose.** Port A is proven working on this board.
> Putting zone indication on the port that is giving trouble would defeat the point.

**Test:** build `test_zones.c` + `common/board.c`.

| Part | What happens | Tells you |
|------|--------------|-----------|
| 1 | zone A LED (PA5) alone, 3 slow flashes | the status LED responds |
| 2 | zone B LED (PA8) alone, 3 slow flashes | the new LED is wired right |
| 3 | both together, 2 flashes | both good |
| 4 | **breach in zone A**: PA5 solid 4 s, PA8 dark, buzzers sounding | the real behaviour |
| 5 | **breach in zone B**: PA8 solid 4 s, PA5 dark, buzzers sounding | same noise, other LED |

Parts 4 and 5 are the ones to watch. The buzzers do exactly the same thing in both -
**only the LED moves**. That is the design working as intended.

**Nothing fitted yet?** Set `ZONE_LEDS_ENABLE` to `0` in `board.h` and every LED call
becomes a no-op; the rest of the system is unaffected.

### Stage 8 - Relay (PA7) and the 12 V siren · `test_actuators.c`, `test_siren.c`

> **OPTIONAL - currently deferred.** The buzzers in Stage 7 are the working deterrent.
> This stage exists only if you add the 12 V siren, which needs a barrel-jack-to-screw-
> terminal adapter to get wires off the DC supply. Skipping it costs you nothing: the
> firmware drives the relay either way, so with nothing on the contacts you simply hear
> it click at the moment of a breach.
>
> **The relay has nothing to do with the buzzers.** It only switches the siren's own 12 V.
> If you are not fitting a siren, you can skip this whole stage.

**Relay module**

| Relay pin | To |
|-----------|----|
| VCC | 5 V |
| GND | GND |
| IN | **PA7** |
| COM / NO / NC | leave empty unless the siren is fitted |

`test_actuators.c` exercises the buzzers and the relay together; with nothing on the
contacts you should just hear the relay click three times.

**Relay polarity:** the code assumes an **active-low** board and drives PA7 open-drain,
which avoids a half-energised relay when a 3.3 V high meets a 5 V-referenced input. If the
relay is ON while the program says OFF, set `RELAY_ACTIVE_LOW` to `0` in `board.h`.

> ⚠ **The siren has never been powered or tested.** Nothing below is a verified result.

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

#### Getting wires out of the 12 V adapter (no cutting, no soldering)

**The relay cannot replace the adapter.** A relay is only a switch - it has no power of its
own. It connects and disconnects the 12 V, so the 12 V still has to come from the adapter.
Leave the adapter out and the siren simply cannot run.

You do **not** need to cut anything. Buy a **DC barrel jack female-to-screw-terminal adapter**
(also sold as "DC 5.5 x 2.1 mm female connector with screw terminals"). It costs ₹20-40.
The adapter plugs into it, and two screws hold ordinary jumper wires. That is the whole job:
no stripping, no soldering, and the mains side is never touched.

* The screw terminal is marked **+** and **−**. Check with the multimeter on DC volts before
  connecting anything: red probe in **+** should read about +12 V.
* If it reads −12 V, your probes are swapped, not the adapter.

**If you cannot get one in time:** skip the siren and demonstrate with the **buzzer on PA6**,
which runs from the 5 V you already have. The integrated firmware drives the relay either way,
so you will still hear it click at the right moment - it just will not have a load on it. The
siren can be added later without changing a line of code. This is a perfectly reasonable
submission; record the siren as future work rather than wiring something unsafe in a hurry.

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

### Stage 9 - Integrated firmware · `firmware/app/main.c`

Run this only after the individual stages pass. It is the real application: all sensors,
the LCD, Bluetooth and both deterrents driven by one state machine.

**Build:** add exactly two files to a new Keil target - `firmware/app/main.c` and
`firmware/common/board.c`. Same project options as any test (define `STM32F401xC`,
`firmware/common` on the include path, C99).

**What you should see**

| Phase | LED (PA5) | LCD line 1 | Lasts |
|-------|-----------|------------|-------|
| Warm-up | slow blink | `SWIS WARMING UP` | ~30 s |
| Armed | steady on | `STATUS: SECURE` | until a sensor fires |
| Checking | fast blink | `CHECKING...` | up to 2 s |
| Breach | fast blink | `STATUS: BREACH` | 10 s |
| Cooldown | off | `COOLDOWN` | 3 s |

LCD line 2 shows `DAY`/`NIGHT` and the breach count.

**The validation rule.** In daylight the system wants **two** sensors to agree before it calls
a breach - that is what stops wind and sun glare setting it off. In the dark **one** sensor is
enough. Triggers that nothing confirms are counted in `false_alarms` and otherwise ignored.
Set `REQUIRE_SECOND_SENSOR` to `0` in main.c if you want any single sensor to fire the alarm
(useful for a quick demo, worse in the field).

**On the phone** (Android serial terminal, 9600 8N1): a status line arrives every 5 s, and
`ZONE 1: ANIMAL BREACH` on each confirmed breach. Commands: `?` help, `S` status,
`R` reset to secure, `T` 2 s deterrent test.

**Missing hardware is tolerated.** If no LCD answers on I2C the system logs `lcd_ok = 0` and
carries on without it. If nothing is wired to the relay contacts you just hear the relay click.
Neither stops the rest of the system.

**Watch in Keil:** `state`, `breach_count`, `false_alarms`, `ir_events`, `pir_events`,
`vib_events`, `ldr_value`, `is_night`, `lcd_ok`.

> **Status: written, never run on hardware.** Expect to adjust `DARK_THRESHOLD` and the
> polarity macros in `board.h` on the first real test.

---

## 4. Power-budget sanity check

| Load | Approx. current |
|------|-----------------|
| Relay coil | 70-90 mA |
| HC-05 | 30-40 mA |
| LCD + backlight | 20-30 mA |
| Buzzers (2, on GPIO pins - **not** a rail load) | ~20 mA each, from PA6 and PB0 |
| IR LED (220 Ω) + receiver stage | ~20 mA |
| PIR / SW-420 / LDR / STM32 | < 50 mA total |
| **Siren A-12-B (12 V side, not an MB102 load)** | **unknown - measure it (expect roughly 100-300 mA)** |

A few hundred mA in total is fine for the MB102, **but the siren must not be powered from its rails** (it is switched from the 12 V adapter through the relay).

**The 1 A adapter is shared.** One VMAX 12 V / 1 A supply feeds the MB102 input *and* the siren through the relay contacts. Budget it as `MB102 input current + siren current < 1 A`. The siren figure is still unknown, so measure it (Stage 8, pre-power check 5) before any sustained run. If the total is tight, give the siren its own 12 V supply and just share the ground.

Linear regulators dissipate `(Vin − Vout) × I` as heat: with a 12 V adapter the 5 V regulator can get hot. Do a finger-touch check after a minute; if too hot, use a lower input voltage (e.g. 9 V) or a heat sink.

## 5. Final integration checklist (before powering the full system)

- [ ] 3.3 V rail = 3.3 V ± 0.1 V, 5 V rail = 5 V ± 0.25 V (measured with everything connected)
- [ ] 0 Ω / beep between GND of every module, the Black Pill and the ST-LINK
- [ ] PA0, PA2 and PA4 each measured ≤ 3.3 V in every sensor state (PA0: about 0 V beam clear, about 3.3 V beam blocked). PB5 carries the PIR and tolerates up to 5 V
- [ ] LCD level shifter: LV = 3.3 V, HV = 5 V, pull-ups verified on both sides
- [ ] HC-05 TXD→PA10, RXD→PA9 (crossed), VCC = 5 V
- [ ] Buzzer + on PA6 and − on GND, 270 Ω test resistor removed, tone confirmed
- [ ] Relay polarity confirmed, siren switched from the 12 V adapter (not the MB102 rails)
- [ ] Siren current measured and recorded; `MB102 input + siren < 1 A` on the shared adapter
- [ ] Siren on relay **NO** (not NC), red lead to **+**, relay contact rating checked
- [ ] All stage tests pass **individually**, including Stage 8 (siren)
- [ ] Wires tidy, joints insulated, nothing loose near the relay contacts

## 6. Quick diagnostics table

| Symptom | Most likely cause |
|---------|-------------------|
| Nothing works, board dead | No power, rails miswired, grounds missing |
| Random resets when the relay or buzzers fire | Supply sag / noise → decoupling caps, strobe off the MB102 rails |
| One sensor always "triggered" | Wrong polarity macro, floating input, trimmer pot mis-set |
| LCD blank | Contrast pot, wrong I2C wiring, level-shifter supplies swapped |
| Bluetooth connects but no text | TX/RX not crossed, wrong baud, wrong app mode |
| Can't flash | See *Troubleshooting* in [`keil_setup.md`](keil_setup.md) |
