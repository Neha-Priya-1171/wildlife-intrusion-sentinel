# 3-Sensor STM32F401 Wildlife Intrusion & Smart Deterrent System

An interrupt-driven perimeter-security prototype for farms. Three independent sensors detect an animal crossing a field boundary; an **STM32F401CCU6 "Black Pill"** (ARM Cortex-M4) validates the event, fires a **sound + light deterrent**, shows status on an LCD and sends an alert to the farmer's phone over **Bluetooth**.

> Course project (23CSE304 Embedded Systems) · Toolchain: **Keil MDK + CMSIS**, flashed with **ST-LINK V2** over SWD.

---

## 1. The problem

Crop raiding by large animals (elephants, wild boar, deer) causes heavy losses for farmers, and manual night-guarding is unsafe and unsustainable. Many cheap deterrents are triggered by a single sensor and produce constant false alarms (wind, small animals, glare), so people learn to ignore them.

**This project** offers an automated, humane (non-lethal) deterrent that:

* detects a boundary breach in hardware with an **external interrupt** (EXTI) - the CPU does not have to poll;
* cross-checks the breach with a **second and third sensor** to cut false triggers;
* scares the animal with a **varying-cadence twin buzzer** (and a strobe siren, if fitted) - varying patterns reduce habituation;
* **alerts the farmer** over Bluetooth and shows state on a local LCD.

## 2. Technical architecture

```
 INPUTS (sensing)                  STM32F401CCU6                     OUTPUTS
┌───────────────────┐   PA0/EXTI0 ┌──────────────────┐ PB6/PB7 ┌─────────────────────┐
│ Active IR beam    ├────────────►│                  ├────────►│ 16x2 I2C LCD        │
├───────────────────┤   PB5       │  NVIC + EXTI     │ (level  │(via 3.3V↔5V shifter)│
│ HC-SR501 PIR      ├────────────►│  ADC1  (LDR)     │ shifter)└─────────────────────┘
├───────────────────┤   PA2       │  USART1 9600 8N1 │ PA9/10  ┌─────────────────────┐
│ SW-420 vibration  ├────────────►│  I2C1            ├────────►│ HC-05 Bluetooth     │
├───────────────────┤   PA4 (ADC) │  GPIO            │         └─────────────────────┘
│ LDR light module  ├────────────►│   (Cortex-M4)    │ PA5     ┌─────────────────────┐
└───────────────────┘             │                  ├────────►│ Green "SECURE" LED  │
                                  │                  │ PA6/PB0 ├─────────────────────┤
                                  │                  ├────────►│ Buzzer x2 (direct)  │
                                  │                  │ PA7     ├─────────────────────┤
                                  │                  ├────────►│ Relay → flashing lamp│
                                  └──────────────────┘         └─────────────────────┘
```

**Detection logic (target firmware behaviour)**

1. **IDLE / SECURE** - green LED on, LCD shows `STATUS: SECURE`, CPU waits for an interrupt.
2. **TRIGGER** - IR beam break (or vibration/PIR edge) raises an EXTI interrupt.
3. **VALIDATE** - firmware checks the other sensors inside a short time window; the **LDR** reading is used to adjust for day/night/glare behaviour.
4. **DETER** - the breached **zone's LED** lights to show which side; every fitted buzzer plays a varying pattern; the relay also switches the strobe siren if one is fitted.
5. **ALERT** - `ZONE 1: ANIMAL BREACH` is sent over USART1 → HC-05 → phone; LCD shows `STATUS: BREACH` until reset.

This repository currently contains the **hardware bring-up test firmware** (one small program per subsystem). Integrated firmware comes after every block passes its test.

## 3. Bill of Materials

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 1 | STM32F401CCU6 Black Pill | 1 | 3.3 V logic, 256 KB flash. Solder the header pins first |
| 2 | ST-LINK V2 programmer (SWD) | 1 | Needs ST-LINK driver on the PC |
| 3 | MB102 breadboard power module | 1 | Two switchable rails: set one to 3.3 V, one to 5 V |
| 4 | 12 V 1 A DC adapter (barrel jack) | 1 | VMAX SMPS, input AC 100-240 V, output 12 V / 1 A. Feeds the MB102 **and** the siren - see the budget in section 4 |
| 5 | USB Type-C cable | 1 | Optional power / USB access to the Black Pill |
| 6 | 5 mm IR transmitter + photodiode receiver pair | 1 pair | Replaces the unavailable active IR break-beam pair. 220 Ω resistor + one-transistor receiver stage (assembly guide, Stage 2). **Received, built and tested** |
| 7 | HC-SR501 PIR sensor | 1 | Needs ~30-60 s warm-up. Output wired to **PB5** (not PA1) |
| 8 | SW-420 vibration sensor module (LM393) | 1 | Digital output (DO) used |
| 9 | LDR sensor module | 1 | Analog output (AO) used |
| 10 | Green LED + 220 Ω resistor | 1 set | System "SECURE" indicator |
| 11 | 16×2 character LCD with PCF8574 I2C backpack | 1 | Address usually 0x27 or 0x3F |
| 12 | 4-channel bidirectional 3.3 V ↔ 5 V logic level converter | 1 | Used on the I2C lines |
| 13 | HC-05 Bluetooth module | 1 | 9600 baud in data mode |
| 14 | 12 mm buzzer (3-5 V) | 2 | **Both in use.** Driven directly at 3.3 V: buzzer 1 on PA6, buzzer 2 on PB0. One per pin. Active (built-in oscillator) is what the firmware expects |
| 15 | NPN transistor BC547 (or 2N2222) | 1 | **Used by the IR receiver stage**, not the buzzer. The buzzer is driven straight from PA6 |
| 16 | 5 V single-channel opto-isolated relay module | 1 | Check active-high/low. Switches the **siren** only; the buzzers do not use it. Optional while the siren is deferred |
| 17 | Mini strobe wired siren (model **A-12-B**, red) | 1 | Replaces the unavailable 5 V flashing lamp. **12 V DC confirmed** from the packaging; current rating is **not** printed and must be measured. Two flying leads, switched from the 12 V adapter via the relay. **Received - testing pending** |
| 18 | 830-point breadboard | 1 | |
| 19 | DuPont jumper wires (M-M, M-F, F-F) | 1 set | |
| 20 | Power / ground bus wiring | 1 set | **Design in progress** - not purchased, not implemented. Proposed approach under investigation; see section 9 |

**Recommended supporting parts**

| Part | Qty | Purpose |
|------|-----|---------|
| 1 kΩ resistor | 1 | Not needed - the buzzers are driven directly. Keep as spare |
| 220 Ω resistor (second) | 1 | IR transmitter LED current limit (only one 220 Ω is on hand, for the green LED) |
| 270 Ω resistor | 1 | Temporary series resistor for the first buzzer power-up only; removed afterwards |
| NPN transistor BC547 / 2N2222 (extra) | 1 | Spare for the IR receiver stage. The buzzers need none |
| 10 kΩ resistor | 1 | IR receiver collector pull-up |
| 10 kΩ + 20 kΩ resistors | 2 each | Spare voltage divider if any sensor output measures above 3.3 V |
| 100 nF ceramic capacitors | 3-5 | Decoupling near MCU and modules |
| 10 µF capacitors | 2 | Supply filtering |
| Multimeter | 1 | Needed for the checks in the assembly guide |
| Terminal blocks, brackets/stands, cable ties, heat-shrink | as needed | Mechanical robustness / IR alignment |

Estimated cost of the main list: **≈ ₹2,381**.

## 4. Power distribution strategy

```
 12 V adapter ─► [MB102] ─┬─ 3.3 V rail ─► STM32 3V3 pin, level-shifter LV side,
                          │                SW-420 module, LDR module
                          ├─ 5 V rail ──► HC-05, LCD backpack, level-shifter HV side,
                          │                relay VCC, PIR, IR sensor
                          └─ GND rails ─► ONE common ground grid (both rails' − joined)
```

* **3.3 V digital core:** the Black Pill and anything whose output goes to an STM32 pin that is not 5 V-safe.
* **5 V peripherals:** modules that need 5 V (HC-05, LCD, relay, PIR).
* **The buzzers are not a rail load at all.** Each is driven straight from its GPIO pin at 3.3 V (PA6 and PB0), so they draw from the MCU, not from the MB102.
* **Common ground:** join the − rails of both sides and connect every module GND and the ST-LINK GND to it. Missing grounds cause most "it doesn't work" problems.
* **SW-420 and LDR modules run from 3.3 V on purpose**: their outputs then can never exceed 3.3 V (important for the ADC pin PA4).
* **Never power the Black Pill from two sources.** Use *either* the MB102 3.3 V rail *or* USB-C. When flashing with ST-LINK, connect only GND, SWDIO and SWCLK (leave the ST-LINK 3.3 V wire off if the MB102 powers the board).
* **The strobe and any high-current load must not run from the MB102 rails.** The MB102 regulators are small linear regulators and get hot at 12 V input. The strobe (expected 12 V DC, roughly 60-300 mA) is switched by the relay contacts (COM/NO) directly from the 12 V adapter. Keep the combined load under the adapter's 1 A rating.
* **Low-voltage only. Do not connect mains voltage to the relay contacts** for this prototype.

## 5. Master pin mapping

| STM32 pin | Connected to | Function | Notes |
|-----------|--------------|----------|-------|
| **PA0** | IR receiver stage (NPN collector) | EXTI0 / digital in | Beam clear = LOW, beam broken = HIGH (rising edge). Polarity set by `IR_ACTIVE_LOW` in `board.h`. Black Pill KEY button is also on PA0 |
| **PB0** | Buzzer 2 **+** (zone B) | GPIO out | One buzzer per pin - never share PA6. Optional: set `BUZ2_ENABLE` to 0 if it is not working |
| **PA3** | Zone A indicator LED (via 220 Ω) | GPIO out | Lights when the **IR beam** side is breached |
| **PA8** | Zone B indicator LED (via 220 Ω) | GPIO out | Lights when the **PIR / vibration** side is breached |
| **PB5** | PIR OUT | Digital in (polled) | **Moved here from PA1.** PB5 has no ADC function, so it is an FT (5 V tolerant) pin and accepts the module's ~4.3 V output. PA1 is 3.3 V-only and is now treated as suspect |
| **PA2** | SW-420 DO | Digital in (EXTI2 capable) | Module powered from 3.3 V |
| **PA4** | LDR module AO | ADC1_IN4 | Must never exceed 3.3 V |
| **PA5** | Green LED (+) via 220 Ω | GPIO out | "SECURE" indicator |
| **PA6** | Buzzer 1 **+** (direct, no transistor) | GPIO out | High = buzzer on. ~20 mA at 3.3 V, inside the 25 mA pin limit. The BC547 went to the IR receiver stage |
| **PA7** | Relay module IN | GPIO out | Check module polarity (`RELAY_ACTIVE_LOW`). **For the siren only - nothing to do with the buzzers.** Optional while the siren is deferred |
| **PA9** | HC-05 RXD | USART1_TX (AF7) | 9600 8N1 |
| **PA10** | HC-05 TXD | USART1_RX (AF7) | |
| **PB6** | LCD SCL via level shifter | I2C1_SCL (AF4) | LV = 3.3 V, HV = 5 V |
| **PB7** | LCD SDA via level shifter | I2C1_SDA (AF4) | |
| PA13 / PA14 | ST-LINK SWDIO / SWCLK | SWD | Reserved for programming - do not reuse |

> This map supersedes the pin tables in the earlier draft documents (which used PA1 = buzzer, PA2 = relay, PA3 = PIR). Use only this table.

## 6. Repository layout

```
.
├── README.md
├── docs/
│   ├── assembly_guide.md      # build + test the hardware step by step
│   └── keil_setup.md          # Keil MDK, packs, ST-LINK, flashing
├── simulation/                # live GUI bridged to the hardware (separate from app/)
│   ├── swis_gui.py            # forest/farm animation driven by Bluetooth telemetry
│   ├── firmware/sim_main.c    # main.c + telemetry; main.c itself is untouched
│   └── README.md
└── firmware/
    ├── app/
    │   └── main.c             # INTEGRATED FIRMWARE - the real application
    ├── common/                # board.h / board.c  (pin map, SysTick delay, GPIO helpers)
    └── tests/
        ├── test_led.c         # PA5 status LED
        ├── test_ir_sensor.c   # PA0 EXTI0 interrupt
        ├── test_sensors.c     # PIR (PB5) + vibration (PA2)
        ├── test_pir.c         # simple PIR-only test on PB5
        ├── test_siren.c       # 12 V strobe siren via the PA7 relay
        ├── test_buzzer.c      # buzzers on PA6/PB0, direct drive, active/passive check
        ├── test_zones.c       # two-zone LEDs (PA3/PA8) + per-zone breach demo
        ├── test_ldr.c         # (bonus) PA4 ADC
        ├── test_lcd.c         # I2C1 16x2 LCD
        ├── test_bluetooth.c   # USART1 + HC-05
        └── test_actuators.c   # buzzers + relay together (relay = siren only, optional)
```

## 7. Quick start

1. Install the toolchain: [`docs/keil_setup.md`](docs/keil_setup.md).
2. Assemble and test one block at a time: [`docs/assembly_guide.md`](docs/assembly_guide.md). Order: **LED → IR → PIR/vibration → LDR → LCD → Bluetooth → buzzers**. The relay and siren (Stages 8-9) are optional and currently deferred.
3. Each test is its own program. Build **one test file + `common/board.c`** per Keil target.
4. When every test passes, integrate (state machine in section 2).

## 8. Design notes and known limitations

* Tests run on the default 16 MHz internal oscillator (no clock setup needed). Timing-sensitive code reads `SystemCoreClock`.
* The IR/PIR/SW-420/relay polarity differs between module makers. Flip the `*_ACTIVE_*` macros in `firmware/common/board.h` rather than editing the tests.
* The test code is written to be clear for beginners (blocking delays in tests). The integrated firmware should use interrupts + a state machine and avoid long blocking delays.
* The code has not been compiled on the target hardware by the author of this template; expect small fixes (Keil include paths, module polarity) on first build.

## 9. Build status

Numbering follows the BoM in section 3. **Received** means the part is physically in hand; it says nothing about whether it works. A row only reaches *tested* after that test has actually been run on hardware.

### 9.1 Component status

| Item | Component | Received | Connected | Tested |
|------|-----------|----------|-----------|--------|
| 1-5, 7-16, 18, 19 | Core boards, sensors, modules, breadboard, jumpers | Yes | per stage below | per stage below |
| 6 | IR transmitter + photodiode receiver pair | Yes | Yes | **Yes - passed** |
| 17 | Mini strobe siren A-12-B (12 V) | **Yes** | **No** | **No - testing pending** |
| 20 | Power / ground bus wiring | No | No | No - **design in progress** |

### 9.2 Stage-wise checklist

Stages match the sections in [`docs/assembly_guide.md`](docs/assembly_guide.md).

| Stage | Subsystem | Pin(s) | Test program | Status |
|-------|-----------|--------|--------------|--------|
| 1 | Green status LED | PA5 | `test_led.c` | Not recorded |
| 2 | IR break-beam transmitter + receiver | PA0 | `test_ir_sensor.c` | **Complete - tested and passing** |
| 3 | PIR | **PB5** | `test_pir.c` | **Retest pending** - moved off PA1 (stuck HIGH) onto a 5 V-tolerant pin |
| 3 | SW-420 vibration | PA2 | `test_sensors.c` | Not recorded |
| 4 | LDR light sensor | PA4 | `test_ldr.c` | Not recorded |
| 5 | 16x2 I2C LCD | PB6/PB7 | `test_lcd.c` | Not recorded |
| 6 | HC-05 Bluetooth | PA9/PA10 | `test_bluetooth.c` | Not recorded |
| 7 | Buzzers x2 (direct) | PA6/PB0 | `test_buzzer.c` | **Buzzer 1 (PA6) works.** Buzzer 2 on PB0 silent - proven not to be the buzzer (swapped both), cause unresolved |
| 7b | Zone indicator LEDs | PA3/PA8 | `test_zones.c` | **Not yet wired** - code written, never run |
| 8 | Relay (optional, for the siren) | PA7 | `test_actuators.c` | Not recorded - skippable while the siren is deferred |
| 9 | 12 V strobe siren | PA7 | `test_siren.c` | **Deferred** - hardware received, code written, never run |
| 10 | Integrated state machine | all | `firmware/app/main.c` | **Written, never run on hardware** |
| 11 | Live GUI simulation | - | `simulation/swis_gui.py` + `simulation/firmware/sim_main.c` | **Desktop side tested; never run against the board** |

"Not recorded" means no result has been logged here, not that the stage failed. Update the row when you run the stage.

### 9.3 Open items

* **Buzzer 2 on PB0 is silent.** Buzzer 1 on PA6 works. Swapping the two buzzers over did not move the fault, so the parts are fine and the problem is PB0, its wire or its breadboard row. **Unresolved, and deliberately no longer blocking:** the design now puts zone indication on two LEDs (PA3/PA8, both on the proven port A) rather than on the buzzers, so the system still reports which side was breached with only one working buzzer. Worth noting that PB0 being dead may also be a warning about PB5 (PIR) and PB6/PB7 (LCD), which are on the same port and not yet tested.
* **PIR moved to PB5** - PA1 read permanently HIGH and never changed. The firmware logic was audited and is correct, so the fault is electrical. Rather than keep debugging PA1, the PIR now lives on **PB5**, which unlike PA1 has no ADC function and therefore tolerates 5 V - so the module's measured ~4.3 V output is within spec there. Rerun `test_pir.c` on the new pin. PA1 remains unproven and should not be reused until tested.
* **Siren** - received and confirmed 12 V from the packaging, but the **current rating is not printed on the unit or the box**. Measure it before any extended run and check it against the 1 A adapter budget in section 4.
* **Bus wiring (item 20)** - *proposed approach only, nothing built.* The idea discussed was to reuse the existing relay module together with suitable wiring. Assessment: the relay is a **switch for the siren load**, not a power-distribution element, so it does not by itself satisfy item 20. What actually carries the rails today is the breadboard's power strips plus DuPont jumpers (item 19). Remaining work: decide whether dedicated bus wire or terminal blocks are needed for the final build, then implement and test. **Not implemented, not verified.**

## 10. License

Add your preferred license (e.g. MIT) as `LICENSE`.
