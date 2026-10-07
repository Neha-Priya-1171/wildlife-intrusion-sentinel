# Keil MDK + ST-LINK V2 setup (first-time guide)

Target: **STM32F401CCU6 Black Pill** · Tool: **Keil MDK-ARM v5 (µVision)** · Programmer: **ST-LINK V2 (SWD)**

## 1. Install on the PC (Windows)

1. **Keil MDK v5** from keil.com (the free MDK Community / evaluation licence is fine for a 256 KB part with small code).
2. **ST-LINK driver** - install ST's ST-LINK USB driver (package *STSW-LINK009*). Plug in the ST-LINK; Device Manager should list *STM32 STLink* with no warning icon.
3. Open Keil → **Pack Installer** (toolbar icon) → search `STM32F4` → install **Keil::STM32F4xx_DFP**.
4. *(Optional)* STM32CubeProgrammer / STM32CubeMX are handy for upgrading ST-LINK firmware or checking pins, but are **not required** for these tests, which use register-level CMSIS.
5. If the ST-LINK is not detected by Keil, update its firmware with *STM32CubeProgrammer* (or ST-LinkUpgrade).

## 2. Connect ST-LINK V2 to the Black Pill (SWD)

| ST-LINK V2 pin | Black Pill pin |
|----------------|----------------|
| SWDIO | DIO (PA13) |
| SWCLK | CLK (PA14) |
| GND | GND |
| 3.3V | 3V3 - **only if the board has no other power source** |

* Check the pin names printed on your ST-LINK; the metal-case clones differ.
* Keep the **BOOT0 jumper at 0** (normal flash boot).
* **Never power the board from ST-LINK 3.3 V, USB-C and the MB102 at the same time.**

## 3. Create the project

1. **Project → New µVision Project** → pick a folder (e.g. `keil_projects/led_test`) → select device **STM32F401CCUx** (STMicroelectronics → STM32F4 Series → STM32F401 → STM32F401CC → STM32F401CCUx).
2. In **Manage Run-Time Environment** tick:
   * **CMSIS → CORE**
   * **Device → Startup**
   * (Nothing else is needed. No HAL, no RTOS.)
3. Right-click *Source Group 1* → **Add Existing Files** → add:
   * one test file from `firmware/tests/` (e.g. `test_led.c`)
   * `firmware/common/board.c`
4. **Options for Target (Alt+F7)**
   * **C/C++ → Define:** `STM32F401xC`  *(if the compiler complains about "Please select first the target STM32F4xx device")*
   * **C/C++ → Include Paths:** the `firmware/common` folder (so `board.h` is found)
   * **C/C++ → Language C mode:** C99 (or GNU99)
   * **Debug:** choose **ST-Link Debugger** → Settings → Port **SW** (the SW Device box should show an IDCODE; if it says "No ST-Link detected" see troubleshooting)
   * **Utilities:** *Use Target Driver for Flash Programming* → ST-Link Debugger → *Settings → Flash Download*: tick **Reset and Run**; programming algorithm should list **STM32F4xx 256kB Flash** (click *Add* if empty)
5. **Build (F7)**. Expect `0 Error(s)`.
6. **Flash (F8, Load).** With *Reset and Run* ticked the program starts immediately.

## 4. Running different tests: one target per test

Only **one** `main()` can exist in a build. Choose one approach:

* **Simplest:** create a new Keil project folder per test.
* **Tidier:** in one project use *Project → Manage → Project Items → Targets* to create targets `LED`, `IR`, `SENSORS`, `LCD`, `BT`, `ACT`. For each target, right-click each test file → *Options for File* and untick **Include in Target Build** for every test except its own. Always include `board.c`.

Select the target in the toolbar drop-down before building.

## 5. Using the debugger (very useful for the sensor tests)

1. **Debug → Start/Stop Debug Session (Ctrl+F5)**, then **Run (F5)**.
2. Right-click a global variable (e.g. `breach_count`, `pir_events`, `adc_value`) → **Add to Watch 1**. Values update live (enable *View → Periodic Window Update* if they don't).
3. Stop the session before pulling the ST-LINK cable.

## 6. Troubleshooting

| Symptom | Fix |
|---------|-----|
| "No ST-Link detected" | Different USB port/cable; reinstall driver; check SWDIO/SWCLK not swapped; common GND connected; board powered |
| "Cannot access target / Flash download failed" | Connect GND first; in *Debug → Settings* set **Connect: under Reset**, lower **Max Clock** to 1 MHz; check BOOT0 = 0 |
| `stm32f4xx.h: No such file` | Install the F4 DFP pack; tick *CMSIS: CORE* and *Device: Startup* in RTE |
| "Please select first the target STM32F4xx device" | Add `STM32F401xC` to C/C++ → Define |
| `board.h: No such file` | Add `firmware/common` to Include Paths |
| "L6200E: Symbol main multiply defined" | Two test files are in the same target build (see section 4) |
| Program flashes but nothing happens | Check "Reset and Run" is ticked; press the reset button; verify the wiring and pin numbers |
| Everything works on USB but not on MB102 | MB102 jumpers not on correct voltage; grounds not common |
