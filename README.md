# 3-Sensor STM32F401 Wildlife Intrusion & Smart Deterrent System

An interrupt-driven perimeter-security prototype for farms. Three independent sensors detect an animal crossing a field boundary; an **STM32F401CCU6 "Black Pill"** (ARM Cortex-M4) validates the event, fires a **sound + light deterrent**, shows status on an LCD and sends an alert to the farmer's phone over **Bluetooth**.

> Course project (23CSE304 Embedded Systems) · Toolchain: **Keil MDK + CMSIS**, flashed with **ST-LINK V2** over SWD.

---

## 1. The problem

Crop raiding by large animals (elephants, wild boar, deer) causes heavy losses for farmers, and manual night-guarding is unsafe and unsustainable. Many cheap deterrents are triggered by a single sensor and produce constant false alarms (wind, small animals, glare), so people learn to ignore them.

**This project** offers an automated, humane (non-lethal) deterrent that:

* detects a boundary breach in hardware with an **external interrupt** (EXTI) - the CPU does not have to poll;
* cross-checks the breach with a **second and third sensor** to cut false triggers;
* scares the animal with a **flashing strobe and a varying-cadence buzzer** (varying patterns reduce habituation);
* **alerts the farmer** over Bluetooth and shows state on a local LCD.

## 2. Technical architecture

```
 INPUTS (sensing)                  STM32F401CCU6                     OUTPUTS
┌───────────────────┐   PA0/EXTI0 ┌──────────────────┐ PB6/PB7 ┌─────────────────────┐
│ Active IR beam    ├────────────►│                  ├────────►│ 16x2 I2C LCD        │
├───────────────────┤   PA1       │  NVIC + EXTI     │ (level  │ (via 3.3V↔5V shifter)│
│ HC-SR501 PIR      ├────────────►│  ADC1  (LDR)     │ shifter)└─────────────────────┘
├───────────────────┤   PA2       │  USART1 9600 8N1 │ PA9/10  ┌─────────────────────┐
│ SW-420 vibration  ├────────────►│  I2C1            ├────────►│ HC-05 Bluetooth     │
├───────────────────┤   PA4 (ADC) │  GPIO            │         └─────────────────────┘
│ LDR light module  ├────────────►│   (Cortex-M4)    │ PA5     ┌─────────────────────┐
└───────────────────┘             │                  ├────────►│ Green "SECURE" LED  │
                                  │                  │ PA6     ├─────────────────────┤
                                  │                  ├────────►│ Buzzer (NPN driver) │
                                  │                  │ PA7     ├─────────────────────┤
                                  │                  ├────────►│ Relay → strobe light │
                                  └──────────────────┘         └─────────────────────┘
```

**Detection logic (target firmware behaviour)**

1. **IDLE / SECURE** - green LED on, LCD shows `STATUS: SECURE`, CPU waits for an interrupt.
2. **TRIGGER** - IR beam break (or vibration/PIR edge) raises an EXTI interrupt.
3. **VALIDATE** - firmware checks the other sensors inside a short time window; the **LDR** reading is used to adjust for day/night/glare behaviour.
4. **DETER** - relay switches the strobe, buzzer plays a varying pattern.
5. **ALERT** - `ZONE 1: ANIMAL BREACH` is sent over USART1 → HC-05 → phone; LCD shows `STATUS: BREACH` until reset.

This repository currently contains the **hardware bring-up test firmware** (one small program per subsystem). Integrated firmware comes after every block passes its test.

## 3. Bill of Materials

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 1 | STM32F401CCU6 Black Pill | 1 | 3.3 V logic, 256 KB flash. Solder the header pins first |
| 2 | ST-LINK V2 programmer (SWD) | 1 | Needs ST-LINK driver on the PC |
| 3 | MB102 breadboard power module | 1 | Two switchable rails: set one to 3.3 V, one to 5 V |
| 4 | 12 V 1 A DC adapter (barrel jack) | 1 | Input for the MB102 (7-12 V) |
| 5 | USB Type-C cable | 1 | Optional power / USB access to the Black Pill |
| 6 | 5 mm IR transmitter + photodiode receiver pair | 1 pair | Replaces the unavailable active IR break-beam pair. Needs a 220 Ω resistor and a one-transistor receiver stage (assembly guide, Stage 2). **Not yet purchased** |
| 7 | HC-SR501 PIR sensor | 1 | Needs ~30-60 s warm-up |
| 8 | SW-420 vibration sensor module (LM393) | 1 | Digital output (DO) used |
| 9 | LDR sensor module | 1 | Analog output (AO) used |
| 10 | Green LED + 220 Ω resistor | 1 set | System "SECURE" indicator |
| 11 | 16×2 character LCD with PCF8574 I2C backpack | 1 | Address usually 0x27 or 0x3F |
| 12 | 4-channel bidirectional 3.3 V ↔ 5 V logic level converter | 1 | Used on the I2C lines |
| 13 | HC-05 Bluetooth module | 1 | 9600 baud in data mode |
| 14 | Active piezo buzzer (5 V) | 1 | Active = built-in oscillator |
| 15 | NPN transistor BC547 (or 2N2222) | 1 | Buzzer driver |
| 16 | 5 V single-channel opto-isolated relay module | 1 | Check active-high/low |
| 17 | Mini strobe wired siren / indicator light | 1 | Replaces the unavailable 5 V flashing lamp. Expected 12 V DC: confirm rating, switch from the 12 V adapter via the relay. **Not yet purchased** |
| 18 | 830-point breadboard | 1 | |
| 19 | DuPont jumper wires (M-M, M-F, F-F) | 1 set | |
| 20 | Power / ground bus wiring | 1 set | **Not yet purchased** (jumper wires can substitute) |

**Recommended supporting parts**

| Part | Qty | Purpose |
|------|-----|---------|
| 1 kΩ resistor | 1 | NPN base resistor (buzzer driver) |
| 220 Ω resistor (second) | 1 | IR transmitter LED current limit (only one 220 Ω is on hand, for the green LED) |
| NPN transistor BC547 / 2N2222 (extra) | 2 | IR receiver stage (the first one is used for the buzzer) |
| 10 kΩ resistor | 1 | IR receiver collector pull-up |
| 10 kΩ + 20 kΩ resistors | 2 each | Spare voltage divider if any sensor output measures above 3.3 V |
| 100 nF ceramic capacitors | 3-5 | Decoupling near MCU and modules |
| 10 µF capacitors | 2 | Supply filtering |
| Multimeter | 1 | Needed for the checks in the assembly guide |
| Terminal blocks, brackets/stands, cable ties, heat-shrink | as needed | Mechanical robustness / IR alignment |

Estimated cost of the main list (from the project sheet): **≈ ₹2,381**.

## 4. Power distribution strategy

```
 12 V adapter ─► [MB102] ─┬─ 3.3 V rail ─► STM32 3V3 pin, level-shifter LV side,
                          │                SW-420 module, LDR module
                          ├─ 5 V rail ──► HC-05, LCD backpack, level-shifter HV side,
                          │                relay VCC, buzzer, PIR, IR sensor
                          └─ GND rails ─► ONE common ground grid (both rails' − joined)
```

* **3.3 V digital core:** the Black Pill and anything whose output goes to an STM32 pin that is not 5 V-safe.
* **5 V peripherals:** modules that need 5 V (HC-05, LCD, relay, buzzer, PIR).
* **Common ground:** join the − rails of both sides and connect every module GND and the ST-LINK GND to it. Missing grounds cause most "it doesn't work" problems.
* **SW-420 and LDR modules run from 3.3 V on purpose**: their outputs then can never exceed 3.3 V (important for the ADC pin PA4).
* **Never power the Black Pill from two sources.** Use *either* the MB102 3.3 V rail *or* USB-C. When flashing with ST-LINK, connect only GND, SWDIO and SWCLK (leave the ST-LINK 3.3 V wire off if the MB102 powers the board).
* **The strobe and any high-current load must not run from the MB102 rails.** The MB102 regulators are small linear regulators and get hot at 12 V input. The strobe (expected 12 V DC, roughly 60-300 mA) is switched by the relay contacts (COM/NO) directly from the 12 V adapter. Keep the combined load under the adapter's 1 A rating.
* **Low-voltage only. Do not connect mains voltage to the relay contacts** for this prototype.

## 5. Master pin mapping

| STM32 pin | Connected to | Function | Notes |
|-----------|--------------|----------|-------|
| **PA0** | IR receiver stage (NPN collector) | EXTI0 / digital in | Beam clear = LOW, beam broken = HIGH (rising edge). Polarity set by `IR_ACTIVE_LOW` in `board.h`. Black Pill KEY button is also on PA0 |
| **PA1** | HC-SR501 PIR OUT | Digital in (EXTI1 capable) | Output is 3.3 V |
| **PA2** | SW-420 DO | Digital in (EXTI2 capable) | Module powered from 3.3 V |
| **PA4** | LDR module AO | ADC1_IN4 | Must never exceed 3.3 V |
| **PA5** | Green LED (+) via 220 Ω | GPIO out | "SECURE" indicator |
| **PA6** | Buzzer via NPN (1 kΩ on base) | GPIO out | High = buzzer on |
| **PA7** | Relay module IN | GPIO out | Check module polarity (`RELAY_ACTIVE_LOW`) |
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
└── firmware/
    ├── common/                # board.h / board.c  (pin map, SysTick delay, GPIO helpers)
    └── tests/
        ├── test_led.c         # PA5 status LED
        ├── test_ir_sensor.c   # PA0 EXTI0 interrupt
        ├── test_sensors.c     # PIR (PA1) + vibration (PA2)
        ├── test_ldr.c         # (bonus) PA4 ADC
        ├── test_lcd.c         # I2C1 16x2 LCD
        ├── test_bluetooth.c   # USART1 + HC-05
        └── test_actuators.c   # buzzer PA6 + relay PA7
```

## 7. Quick start

1. Install the toolchain: [`docs/keil_setup.md`](docs/keil_setup.md).
2. Assemble and test one block at a time: [`docs/assembly_guide.md`](docs/assembly_guide.md). Order: **LED → IR → PIR/vibration → LDR → LCD → Bluetooth → buzzer → relay**.
3. Each test is its own program. Build **one test file + `common/board.c`** per Keil target.
4. When every test passes, integrate (state machine in section 2).

## 8. Design notes and known limitations

* Tests run on the default 16 MHz internal oscillator (no clock setup needed). Timing-sensitive code reads `SystemCoreClock`.
* The IR/PIR/SW-420/relay polarity differs between module makers. Flip the `*_ACTIVE_*` macros in `firmware/common/board.h` rather than editing the tests.
* The test code is written to be clear for beginners (blocking delays in tests). The integrated firmware should use interrupts + a state machine and avoid long blocking delays.
* The code has not been compiled on the target hardware by the author of this template; expect small fixes (Keil include paths, module polarity) on first build.

## 9. Build status

| Item | Status |
|------|--------|
| Items 1-5, 7-16, 18, 19 (see BoM) | Purchased |
| Item 6: IR transmitter + photodiode pair | Not yet purchased; compatibility with PA0 to be verified on the bench |
| Item 17: mini strobe siren | Not yet purchased; confirm rated voltage and current before connecting |
| Item 20: power / ground bus wiring | Not yet purchased |

Numbering follows the BoM in section 3.

## 10. License

Add your preferred license (e.g. MIT) as `LICENSE`.
