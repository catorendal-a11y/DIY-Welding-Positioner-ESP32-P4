<div align="center">

# DIY Welding Positioner Controller

### Precision Multi-Mode Welding Rotator for TIG, MIG, and Pipe Welding

**ESP32-P4 &nbsp;&middot;&nbsp; Firmware v2.0.9**

[![PlatformIO CI](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml/badge.svg)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml)
[![Latest Release](https://img.shields.io/github/v/release/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/latest)
[![Wiki](https://img.shields.io/badge/Wiki-Builder_Guide-blue)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/wiki)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--P4-orange)](platformio.ini)

<sub>GUITION JC4880P443C 4.3" touch display board.</sub>

<br>

<img src="https://raw.githubusercontent.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/1e613d4ebf6fad8be8cec4baac7e45d750d8d661/docs/images/main_screen.svg" width="800" alt="TIG Rotator Controller — Main Screen">

<img src="docs/images/ui_screens.svg" width="800" alt="TIG Rotator Controller — All Screens">

<br>

Open-source controller for a stepper-driven welding positioner / pipe rotator, with a glove-safe touch UI, real-time motor tasking, persistent presets, foot pedal support, and hardwired E-STOP behavior.

Builder docs: [GitHub Wiki](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/wiki) for getting started, hardware setup, troubleshooting, architecture, and roadmap.

<br>

[![Status](https://img.shields.io/badge/Status-Active_Development-brightgreen)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-P4](https://img.shields.io/badge/Platform-ESP32--P4-blue.svg)](https://espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino_ESP32-green.svg)](https://docs.espressif.com/)
[![Graphics: LVGL](https://img.shields.io/badge/LVGL-9.x-blue.svg)](https://lvgl.io/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)

<br>

*If this project helped you build something, consider giving it a star.*

</div>

<br>

---

## Quick Navigation

| Start | Build | Hardware | Safety | Docs |
|:---|:---|:---|:---|:---|
| [Quick Start](#quick-start) | [Build Commands](#build-commands) | [Wiring](#wiring-diagram) | [TIG HF Requirement](#tig-hf-noise-requirement) | [Documentation Map](#documentation-map) |
| [Demo](#demo) | [Features](#features) | [Bill of Materials](#bill-of-materials) | [Troubleshooting](#troubleshooting) | [Project Structure](#project-structure) |

---

## Quick Start

Fast path for builders who already have PlatformIO installed.

**Needed:** GUITION JC4880P443C ESP32-P4 board, PlatformIO, verified driver wiring, and a grounded metal enclosure for TIG HF welding. No LittleFS upload is needed; settings and presets use NVS.

1. **Clone the repository:**

   ```bash
   git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
   cd DIY-Welding-Positioner-ESP32-P4
   ```

2. **Build and flash** from VS Code / PlatformIO terminal:

   ```bash
   pio run --target upload
   ```

   Debug build with verbose serial logging:

   ```bash
   pio run --target upload -e esp32p4-debug
   ```

   Build output is written to `.pio/build-fw` to avoid Windows file-lock conflicts.

3. **Optional native tests** - pure logic, no hardware required:

   ```bash
   pio test -e native
   ```

4. **Connect hardware** per the [wiring diagram](#wiring-diagram). Keep motor PSU off until logic power, E-STOP, ENA, STEP, DIR, and driver settings are verified.

5. **For TIG HF start welding:** put the ESP32-P4 screen, stepper driver, and motor PSU inside the same grounded metal enclosure before welding near the controller.

---

## At a Glance

Single-axis welding positioner firmware for a NEMA 23 / DM542T-style stepper rotator.

| Area | Current Implementation |
|:---|:---|
| Controller | GUITION JC4880P443C ESP32-P4 4.3" MIPI-DSI touch board |
| Motion | NEMA 23 stepper through PUL/DIR driver or DM542T |
| Drivetrain | NMRV030 60:1 worm stage plus 72/40 spur stage, total 1:108 |
| Speed | Main potentiometer, optional ADS1115 pedal pot, saved presets |
| UI | LVGL 9, 800x480 landscape, glove-safe welding workflow |
| Safety | NC E-STOP on GPIO34, DM542T ALM input, ENA forced safe in ISR |
| Storage | NVS JSON settings + up to 16 presets |

The main screen is deliberately pot-driven: there are no main-screen RPM +/- buttons. Jog has its own touch +/- controls for setup movement.

---

## Demo

Watch the system in action — UI interaction, motor rotation, screen navigation, E-STOP, direction switch, and pot control.

<div align="center">

[![DIY Welding Positioner Controller Demo](https://img.youtube.com/vi/GygLl6XY-TM/maxresdefault.jpg)](https://youtu.be/GygLl6XY-TM)

</div>

---

## Features

| Area | What matters |
|:---|:---|
| **Welding workflow** | Continuous, Jog, Pulse, Step, and Countdown modes with pot-driven main RPM and Jog +/- setup control |
| **Presets** | 16 NVS-backed jobs with RPM, direction, workpiece diameter, pulse cycles, step repeats/dwell, soft-start, and auto-stop fields |
| **Industrial inputs** | E-STOP, DM542T ALM, direction switch, foot pedal switch, optional ADS1115 pedal speed input |
| **Touch HMI** | LVGL 9, 800x480 landscape, dark/light neutral UI, 8 accent colors, diagnostics, system info, motor config, display settings |
| **Motor control** | FastAccelStepper RMT pulses, milli-Hz RPM commands, acceleration tuning, microstepping, calibration, direction invert |
| **Safety model** | ISR disables ENA, state machine latches faults, start paths re-check E-STOP/ALM after ENA LOW, dimmed display wakes on fault |
| **Robustness** | Dual-core FreeRTOS split, mutex-protected stepper API, `std::atomic` cross-core flags, debounced NVS writes |

---

## Why This Project Is Different

Most DIY welding rotator projects stop at "turn a stepper at a set speed." This firmware is built around the parts that usually fail in a real TIG shop: HF noise, blocked starts, unsafe enable timing, touch UI reliability, and recoverable field diagnostics.

| Compared With | Typical Limitation | This Project |
|:---|:---|:---|
| Basic Arduino stepper sketch | Single loop, no UI state machine, limited fault handling | Dual-core FreeRTOS with separate motor, control, safety, UI, and storage tasks |
| Generic CNC / GRBL controller | Optimized for G-code, not glove-safe welding workflow | Purpose-built TIG rotator UI with Continuous, Jog, Pulse, Step, Timer, presets, and foot pedal support |
| Cheap speed-controller modules | Pot-only control, no saved jobs, weak diagnostics | NVS presets, workpiece diameter fields, diagnostics screen, event log, and display/system info |
| Open-bench ESP32 projects | Often unstable near HF-start TIG welding | Field-tested with TIG after moving ESP32-P4 screen, DM542T driver, and PSU into one grounded metal enclosure |
| Simple E-STOP input | Software polling or unsafe enable assumptions | GPIO34 ISR forces ENA HIGH, state machine latches fault, and motion-start paths re-check E-STOP/ALM after ENA LOW |
| Display demos | Pretty screen but no realtime motor isolation | LVGL 9 UI isolated to Core 1; FastAccelStepper and safety-critical logic stay on Core 0 |
| Fixed motor configs | Hardcoded microstep/speed assumptions | Touch-configurable microstepping, acceleration, direction invert, max RPM clamp, calibration, and storage validation |

The practical difference: this is not just a rotating table demo. It is firmware for a welding positioner that has been tested against the real failure modes of TIG HF start, noisy wiring, stalled UI, blocked starts, and unsafe motor enable timing.

---

## Operator Workflow

1. Power the ESP32-P4/controller logic first. The firmware boots with ENA disabled.
2. Verify the main screen appears, touch responds, and E-STOP is released.
3. Set driver current and microstep DIP switches to match **Settings > Motor Config**.
4. Turn on the motor supply after STEP/DIR/ENA and E-STOP wiring are checked.
5. Use the main potentiometer to set workpiece RPM, then press **START** for continuous mode.
6. Use **JOG** for manual positioning, **PULSE** for tack cycles, **STEP** for indexed rotation, or **TIMER** for countdown start.
7. Use the E-STOP as the first response to unsafe motion. The red overlay only resets after the physical fault is cleared.

---

## UI Screens

The UI is built from 21 active `ScreenId` roots plus a separate full-screen E-STOP overlay. The full visual map is in `docs/images/ui_screens.svg`; the table below maps the firmware registry.

<details>
<summary><b>ScreenId registry</b></summary>

| Screen | `ScreenId` | Description |
|:---|:---|:---|
| **Boot** | `SCREEN_BOOT` | Startup splash / transition to main |
| **Main** | `SCREEN_MAIN` | Large RPM gauge (pot-driven), start/stop, mode quick-access (no main-screen RPM +/-) |
| **Menu** | `SCREEN_MENU` | Advanced mode selection and settings |
| **Jog** | `SCREEN_JOG` | Touch-and-hold rotation for manual positioning (has its own RPM +/-) |
| **Pulse** | `SCREEN_PULSE` | ON/OFF cycle for tack welding |
| **Step** | `SCREEN_STEP` | Rotate exact angle, then stop |
| **Timer (Countdown)** | `SCREEN_TIMER` | Visual 3-2-1 countdown before continuous rotation starts (1-10 s configurable) |
| **Programs** | `SCREEN_PROGRAMS` | Preset list with save, load, delete |
| **Program Edit** | `SCREEN_PROGRAM_EDIT` | Full preset editor with on-screen keyboard |
| **Edit Pulse** | `SCREEN_EDIT_PULSE` | Quick preset edit for pulse parameters |
| **Edit Step** | `SCREEN_EDIT_STEP` | Quick preset edit for step parameters |
| **Edit Continuous** | `SCREEN_EDIT_CONT` | Quick preset edit for continuous / RPM preset fields |
| **Settings** | `SCREEN_SETTINGS` | Hub for Motor Config, Calibration, Display, Pedal Settings, Diagnostics, System Info and About |
| **Display** | `SCREEN_DISPLAY` | Brightness, dim timeout, **UI MODE** (DARK/LIGHT), accent theme selection |
| **System Info** | `SCREEN_SYSINFO` | Core load, heap, PSRAM, uptime |
| **Calibration** | `SCREEN_CALIBRATION` | Motor calibration factor adjustment |
| **Motor Config** | `SCREEN_MOTOR_CONFIG` | Microstepping, acceleration, direction switch, pedal enable |
| **Pedal Settings** | `SCREEN_PEDAL_SETTINGS` | Pedal arm/disarm plus live GPIO33 and ADS1115 status |
| **Diagnostics** | `SCREEN_DIAGNOSTICS` | Live GPIO/fault page for ESTOP, ALM, DIR switch, pedal switch, ENA, direction, RPM state and recent event log |
| **About** | `SCREEN_ABOUT` | Firmware version, hardware info |
| **Confirm** | `SCREEN_CONFIRM` | Shared confirmation dialog (destructive actions, etc.) |
| **E-STOP Overlay** | _(separate module, not a `ScreenId`)_ | Full-screen red overlay on any active screen |

</details>

---

## Industrial RTOS Architecture

Dual-core **FreeRTOS** design separating realtime motor control from UI rendering.

```
Core 0 (Realtime)                Core 1 (UI)
─────────────────                ──────────────────
safetyTask   (pri 5, 4 KB)      lvglTask    (pri 2, 64 KB)
motorTask    (pri 4, 5 KB)      storageTask (pri 1, 12 KB)
controlTask  (pri 3, 4 KB)
```

### Design Principles

| Principle | Implementation |
|:---|:---|
| **Task Isolation** | UI rendering cannot block motor pulse generation |
| **Hardware Timers** | RMT peripheral for jitter-free micro-stepping |
| **Fail-Safe** | E-STOP ISR cuts ENA pin in <0.5 ms; motion-start paths re-check E-STOP/ALM after ENA LOW and disable again if unsafe |
| **Thread Safety** | FreeRTOS mutex on stepper access, atomic cross-core variables, pending-flag patterns |
| **Live Speed** | `applySpeedAcceleration()` for immediate RPM changes during rotation |

---

## Gear System

<div align="center">
  <img src="docs/images/motor.worm.svg" width="700" alt="Gear System — Worm Drive">
</div>

<div align="center">
  <img src="docs/images/stepper_setup.png" width="700" alt="Motor Assembly — NEMA 23 with worm gearbox and stepper driver">
  <br><sub>NEMA 23 stepper with worm gear reducer and PUL/DIR driver</sub>
</div>

---

## Wiring Diagram

<div align="center">
  <img src="docs/images/Wiring_diagram.v2.svg" width="800" alt="Wiring Diagram">
</div>

### Pinout

<div align="center">
  <img src="docs/images/pinout.jpg" width="600" alt="GUITION JC4880P443C — Pin Definitions">
  <br><sub>Header pin definitions — color-coded by function</sub>
</div>

<br>

| ESP32-P4 Pin | Function | Notes |
|:---|:---|:---|
| **GPIO 50** | STEP (Output) | RMT pulse to driver PUL+ |
| **GPIO 51** | DIR (Output) | Direction to driver DIR+ |
| **GPIO 52** | ENABLE (Output) | Active LOW to driver ENA |
| **GPIO 49** | POT (ADC Input) | 10k speed potentiometer (ADC2_CH0, 11 dB attenuation, ref range 0-3315) |
| **GPIO 29** | DIR SWITCH (Input) | CW/CCW toggle, `INPUT_PULLUP` (LOW = CCW, HIGH = CW) |
| **GPIO 34** | E-STOP (Input, ISR) | NC contact, active LOW, `FALLING` edge, 3-sample boot de-floating (no internal pull-up on ESP32-P4 — add external pull-up for long/noisy leads) |
| **GPIO 33** | PEDAL SW (Input) | Foot pedal switch, `INPUT_PULLUP`, active LOW |
| **GPIO 32** | DRIVER ALM (Input) | DM542T alarm input, `INPUT_PULLUP`, open-drain active LOW |
| GPIO 7 / 8 | Touch I2C | GT911 @ 0x5D + ADS1115 pedal ADC @ 0x48-0x4B when `ENABLE_ADS1115_PEDAL=1` |
| GPIO 14–19, 28, 54 | Board / C6 routing | On GUITION JC4880P443C some pins route to the on-board ESP32-C6; **application firmware uses only the ESP32-P4 side for control**. Do not repurpose without the vendor schematic. |

---

## Build Commands

Default environment: `esp32p4-release`. Build output goes to `.pio/build-fw` to reduce Windows file-lock problems.

| Task | Command |
|:---|:---|
| Release build | `pio run` |
| Debug build | `pio run -e esp32p4-debug` |
| Flash release firmware | `pio run --target upload` |
| Serial monitor | `pio device monitor` |
| Native tests | `pio test -e native` |
| On-device tests | `pio test -e esp32p4-test` |

`COM5` is configured in `platformio.ini`; change `upload_port` / `monitor_port` if Windows assigns another port. Native tests do not need hardware.

---

## Bill of Materials

<div align="center">
  <img src="docs/images/board_overview.jpg" width="600" alt="GUITION JC4880P443C — ESP32-P4 + C6 Touch Display Dev Board">
  <br><sub>GUITION JC4880P443C — 4.3" MIPI-DSI touch display with ESP32-P4 and ESP32-C6</sub>
</div>

<br>

| Component | Model / Specs | Qty |
|:---|:---|:---:|
| **MCU Board** | GUITION JC4880P443C (800x480, ESP32-P4 + ESP32-C6, MIPI-DSI) | 1 |
| **Stepper Driver** | PUL/DIR or DM542T | 1 |
| **Stepper Motor** | NEMA 23 (3 Nm torque) | 1 |
| **Gearbox** | NMRV030 + spur (**1:108** total) | 1 |
| **Power Supply** | 24–36V DC, 5A+ (**36V optimal**, **24V** works) | 1 |
| **Speed Pot** | 10k potentiometer | 1 |
| **Direction Switch** | SPDT toggle switch | 1 |
| **E-STOP** | NC mushroom button | 1 |
| **Foot Pedal** | Analog pot + momentary switch | 1 |
| **ADS1115** | 16-bit I2C ADC (0x48-0x4B) for pedal pot | 1 |

---

## Performance Specifications

| Parameter | Value |
|:---|:---|
| **Output RPM Range** | **0.001 – 3.0 RPM** workpiece (`MIN_RPM` / `MAX_RPM` in `config.h`); Motor Config can set a lower **max RPM** ceiling in NVS |
| **Gear Ratio** | **1 : 108** total &ensp; (NMRV030 60:1 x spur 72/40) |
| **Roller / workpiece (defaults)** | `D_RULLE` 80 mm roller, `D_EMNE` 300 mm reference workpiece OD — used in `rpmToStepHz()` / `angleToSteps()` (see `config.h`, `speed.cpp`) |
| **Microstepping** | 1/4, 1/8, 1/16 (default), 1/32 — selectable in Motor Config, persisted to NVS |
| **Motor Torque** | 3.0 Nm (NEMA 23) |
| **Control Resolution** | Sub-milli-RPM (speed is computed in milli-Hz and applied via `setSpeedInMilliHz()` + `applySpeedAcceleration()`) |
| **Display** | 800 x 480, landscape, LVGL 9.5.0, RGB565, 2-lane MIPI-DSI |
| **Flash partition** | 16 MB total; 2x 4 MB app (OTA-capable); 8 MB storage partition |
| **RAM Usage** | ~10% &ensp; (about 32 KB / 320 KB internal SRAM in release build) — LVGL buffers live in PSRAM (`CONFIG_SPIRAM_FETCH_INSTRUCTIONS`) |

---

## Safety-Critical Wiring

Verify these signals with a meter before enabling motor power:

| Signal | Required Safe Behavior |
|:---|:---|
| ENA / GPIO52 | HIGH = driver disabled, LOW = driver enabled |
| E-STOP / GPIO34 | Released = HIGH, pressed/fault = LOW |
| DM542T ALM / GPIO32 | HIGH = driver OK, LOW = alarm/fault |
| STEP / GPIO50 | Pulse output only; do not share with other hardware |
| DIR / GPIO51 | Direction output to driver |
| Grounds | ESP32 logic ground, driver signal ground, and pedal/ADS1115 ground must be common |
| Enclosure | ESP32-P4 screen, driver, and motor PSU must be inside one grounded metal enclosure for TIG HF use |

The firmware never intentionally enables the motor while E-STOP is active. If the motor moves at boot or while ENA is HIGH, treat it as a wiring or driver configuration fault before continuing.

At motion start, firmware checks the safety inputs before taking the stepper mutex and again immediately after ENA is pulled LOW. If E-STOP or DM542T ALM becomes active in that final window, ENA is driven HIGH again before a run/move command is sent.

---

## TIG HF Noise Requirement

Real TIG welding validation has been completed. The controller, motor logic, UI, and E-STOP behavior worked during TIG welding after the electronics were installed in a shared grounded metal enclosure.

This is a hard installation requirement for HF-start TIG environments:

- Put the ESP32-P4 display board, stepper driver, and motor PSU in the same aluminum or steel enclosure.
- Bond the enclosure to protective earth / welding chassis ground using a short, low-impedance connection.
- Route motor power separately from GPIO, ADC, I2C, E-STOP, and foot-pedal wiring.
- Use shielded external cables where practical, with the shield terminated to the enclosure at the controller side.
- Add ferrites on motor, E-STOP, pedal/pot, USB/power, and external signal cables if the welder still injects noise.

Do not judge TIG reliability from an open-bench test. Bench wiring is acceptable for firmware and motion checks only.

---

## Welding Modes

| Mode | Description |
|:---|:---|
| **Continuous** | Runs at set RPM. Start with ON, stop with STOP. |
| **Jog** | Touch-and-hold rotation for manual positioning. |
| **Pulse** | Rotate ON ms, pause OFF ms, repeat. For tack welding. |
| **Step** | Rotate exact angle (e.g., 90 deg), then stop. |
| **Countdown** | Visual 3-2-1 countdown before rotation starts (configurable 1-10 s). |

---

## Configuration

Open `src/config.h` to adjust hardware parameters:

```cpp
#define MIN_RPM         0.001f  // Minimum workpiece RPM (pot / clamp floor)
#define MAX_RPM         3.0f    // Absolute ceiling for Max RPM setting and firmware clamp
#define GEAR_RATIO      (60.0f * 72.0f / 40.0f)  // 108 = total 1:108
#define D_EMNE          0.300f  // Reference workpiece diameter (m) — kinematics
#define D_RULLE         0.080f  // Roller diameter (m) — kinematics
// Acceleration and microstep are stored in NVS (Motor Config); defaults 7500 steps/s^2, 1/16
#define START_SPEED     20      // Hz minimum step-frequency floor
```

Settings can also be changed from the touchscreen via **Settings > Motor Config** and are persisted to **NVS** (see [Persistence (NVS)](#persistence-nvs)).

---

## Persistence (NVS)

Non-volatile settings and program presets are stored in the ESP32 **NVS** (Non-Volatile Storage) partition using the Arduino **`Preferences`** API (`src/storage/storage.cpp`).

| Item | NVS namespace | Key | Format |
|:---|:---|:---|:---|
| System settings | `wrot` | `cfg` | JSON object (ArduinoJson serialized to a binary blob via `putBytes`) |
| Program presets (max 16) | `wrot` | `prs` | JSON array of preset objects (same serialization) |

**Behaviour**

- **`storage_init()`** opens the namespace, then runs a **one-time migration**: if `cfg` / `prs` are empty but legacy LittleFS files exist (`/settings.json`, `/presets.json`), their contents are copied into NVS. After that, normal operation uses NVS only.
- **Load order:** settings load before presets, so preset RPM values are clamped against the saved **Max RPM** setting. Invalid persisted microstep values are sanitized to the safe default `1/16`.
- **Display / UI:** the settings blob includes e.g. **`color_scheme`** (`0` = dark neutral palette, `1` = light), **`accent_color`** (0–7), brightness and dim-timeout fields — see `SystemSettings` / `storage.cpp` for the authoritative list.
- **Saves** run on **Core 1** in `storageTask`: debounced **~500 ms** after preset changes and **~1 s** after settings changes (`storage_flush()`). UI code sets a pending flag; it does not write flash directly.
- **Flash cache:** NVS commits can disable the flash cache briefly. The firmware sets `g_flashWriting` so the UI can avoid glitches during writes; PSRAM code/rodata mitigations still apply (`CONFIG_SPIRAM_FETCH_INSTRUCTIONS`, `CONFIG_SPIRAM_RODATA` in sdkconfig).

**Troubleshooting**

- You do **not** need to upload a LittleFS filesystem for settings/presets on current firmware.
- If the NVS partition is corrupt or full, erase flash or use the product’s storage format path (if exposed) and reconfigure.

---

## First Bench Test Checklist

- [ ] Display boots successfully
- [ ] Touch input responds correctly
- [ ] Motor rotates at target RPM
- [ ] E-STOP halts motion immediately
- [ ] Direction switch toggles CW/CCW
- [ ] Potentiometer controls speed
- [ ] All 5 welding modes tested
- [ ] Program preset save/load verified
- [ ] Foot pedal starts/stops motor (if connected)
- [ ] TIG HF start tested with controller electronics inside the grounded metal enclosure
- [ ] Let display dim, then trigger E-STOP — backlight returns and overlay is readable

---

## Safety Notice

> **Warning:** This controller drives industrial stepper motors. Follow all safety precautions.

| Hazard | Precaution |
|:---|:---|
| **E-STOP** | NC contact hardware interrupt cuts motor enable in <0.5 ms |
| **Dim + fault** | If the panel has dimmed on timeout, E-STOP still wakes the backlight so the red overlay is visible (`g_wakePending` / `dim_reset_activity()`) |
| **Power Sequencing** | Never power motor without driver connected to coils |
| **Motor Coils** | Never connect/disconnect coils while driver is powered |
| **Voltage** | Verify motor PSU (24V or 36V) before connecting; **36V** is optimal if your driver and wiring allow it |
| **TIG HF noise** | Use a grounded metal enclosure around ESP32-P4 screen, stepper driver, and PSU before welding |

---

## Troubleshooting

<details>
<summary><b>Controller resets, touch freezes, or inputs glitch when TIG arc starts</b></summary>

- HF-start TIG can couple into open wiring strongly enough to reset ESP32-P4, lock I2C/touch, or create false GPIO/ADC readings
- Install the ESP32-P4 screen, stepper driver, and motor PSU inside the same grounded aluminum/steel enclosure
- Bond enclosure to PE/chassis ground with a short low-impedance connection
- Keep motor cables physically separated from E-STOP, pedal, pot, STEP/DIR/ENA, and I2C wiring
- Use shielded external cables and terminate shields to the enclosure at the controller side
- Add ferrites on motor, E-STOP, pedal/pot, USB/power, and external signal cables if needed
- See [docs/EMI_MITIGATION.md](docs/EMI_MITIGATION.md) and [docs/emi_test.md](docs/emi_test.md)

</details>

<details>
<summary><b>Main START does not always start motor</b></summary>

- Current control logic uses a single overwrite command queue, so the latest START/STOP command wins and stale STOP requests cannot block a later START
- Go to **Settings > Diagnostics** and verify `MOTION BLOCK = NO`, `ESTOP GPIO34 = HIGH OK`, `DM542T ALM GPIO32 = HIGH OK`, and `ENA GPIO52 = HIGH DISABLED` while idle
- The Diagnostics event log shows the last START/STOP, pedal, program and fault events, so you can see whether the UI requested motion or safety blocked it
- If a foot pedal is connected, use **Settings > Pedal Settings** to confirm `GPIO33 switch` changes between `HIGH OPEN` and `LOW PRESSED`
- JOG release cancels a pending JOG start so a quick tap/release cannot start motion after the button was released

</details>

<details>
<summary><b>Motor does not move</b></summary>

- Check STEP/DIR/ENA wiring
- Verify ENA logic (LOW = enabled)
- Confirm driver power supply is active

</details>

<details>
<summary><b>Wrong rotation direction</b></summary>

- Swap DIR polarity in firmware, or reverse A+/A- coil wiring

</details>

<details>
<summary><b>Settings not saving</b></summary>

- Data lives in **NVS** (`wrot` / `cfg` and `prs`), not LittleFS
- `storageTask` debounces writes — wait **1-2 seconds** after changing settings or presets
- If nothing persists after flash erase, confirm the **NVS** partition is present in your partition table (typical size 24 KB / `0x6000` in shipped configs)

</details>

<details>
<summary><b>ESTOP triggered at power-on with button released</b></summary>

- GPIO 34 has **no internal pull-up** on ESP32-P4; a disconnected or long-wired ESTOP line can float at boot
- v2.0.5+ samples `PIN_ESTOP` three times with 500 µs spacing (requires 2/3 LOW before treating as pressed); serial log will show `Safety init: ESTOP=PRESSED (low samples X/3)`
- If you still see false boot ESTOPs, add an **external 10 kΩ pull-up to 3V3** on GPIO 34 and/or shorten/shield the E-STOP wiring (see `docs/EMI_MITIGATION.md`)

</details>

<details>
<summary><b>Boot loop after flashing</b></summary>

- Open the serial monitor — v2.0.5+ logs fatal init errors via `LOG_E` (always compiled in) before rebooting, e.g. `FATAL: storage: NVS namespace open — rebooting`. The reason string tells you which subsystem failed.
- Common causes: corrupt NVS (erase flash and retry), missing partition (`default_16MB.csv`), or hardware not powered (driver unpowered when motor init runs)

</details>

---

## Known Limitations

- Single-axis control only
- No closed-loop tachometer feedback; RPM is commanded from kinematics, calibration, and step timing
- Basic PUL/DIR drivers have no anti-resonance DSP; DM542T is recommended for smoother low-speed and higher-RPM work
- GPIO 14-19, 28, and 54 may be tied to the on-board ESP32-C6 per PCB; confirm the board pinout before using them for custom circuits

---

## Roadmap

**Completed:** 5 welding modes, NVS presets, foot pedal support, direction switch, diagnostics, workpiece diameter presets, motor config UI, dark/light display mode, FreeRTOS/atomic hardening, E-STOP display wake, non-blocking ADS1115 pedal ADC, and TIG HF validation in a grounded enclosure.

**Next useful work:**

- [ ] Enclosure CAD / printable panel files
- [ ] Assembly guide with real build photos
- [ ] Wider DM542T speed/current tuning data
- [ ] Optional binary release workflow for builders without PlatformIO

---

## Documentation Map

| Document | Use It For |
|:---|:---|
| [docs/HARDWARE_SETUP.md](docs/HARDWARE_SETUP.md) | Detailed driver wiring, DM542T checklist, ADS1115 wiring, reserved pins |
| [docs/EMI_MITIGATION.md](docs/EMI_MITIGATION.md) | Required TIG HF enclosure, shielding, grounding, ferrites, RC and TVS guidance |
| [docs/SAFETY_SYSTEM.md](docs/SAFETY_SYSTEM.md) | E-STOP behavior, watchdog model, safety assumptions |
| [docs/PROJECT_IMPLEMENTATION.md](docs/PROJECT_IMPLEMENTATION.md) | RTOS architecture, storage, display pipeline, known workarounds |
| [docs/INSTRUCTABLES.md](docs/INSTRUCTABLES.md) | Builder-friendly article content and assembly flow |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Fork, build, test, and pull-request workflow |
| [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) | Contributor Covenant (community standards for GitHub / contributors) |
| [GitHub Wiki](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/wiki) | Public builder guide with clone/build, hardware setup, troubleshooting, architecture, and roadmap |
| [wiki/Getting-Started.md](wiki/Getting-Started.md) | Short build/flash/use walkthrough |
| [wiki/Troubleshooting.md](wiki/Troubleshooting.md) | Field problems and fixes |
| [test/README.md](test/README.md) | Native and on-device test entry points |

---

## Project Structure

<details>
<summary><b>Source tree overview</b></summary>

```
src/
  main.cpp                  Setup, FreeRTOS tasks (Core 0 / Core 1 pinning)
  config.h                  Pinouts, gear ratio, RPM limits, log macros
  app_state.h/cpp           Single source of truth for cross-core std::atomic flags
                              (g_estopPending, g_uiResetPending, g_wakePending,
                               g_flashWriting, g_screenRedraw, g_dir_switch_cache,
                               motorConfigApplyPending) + fatal_halt()
  event_log.h/cpp           Small RAM ring buffer for Diagnostics runtime events
  control/                  State machine + welding modes
    control.cpp               Core state machine with CAS transitions
    program_executor.cpp      Applies saved presets through the control command queue
    modes/                    continuous, jog, pulse, step_mode, timer
  motor/                    Stepper driver
    motor.cpp                 FastAccelStepper init, run, stop, motor_set_target_milli_hz()
    speed.cpp                 ADC pot, pedal (non-blocking ADS1115 state machine), direction, RPM
    acceleration.cpp          Acceleration ramps
    microstep.cpp             Microstepping config
    calibration.cpp           Calibration factor
  safety/                   E-STOP + watchdog
    safety.cpp                ISR (GPIO + atomic flags only), boot de-floating, UI reset, state guard
  storage/                  NVS persistence (Preferences + JSON blobs)
    storage.cpp               Settings/presets serialize, NVS mutex, LittleFS migration
  ui/                       LVGL display
    display.cpp               MIPI-DSI ST7701 init
    lvgl_hal.cpp              Flush callback (manual 90° rotation), dim, touch polling
    theme.cpp/h               Runtime neutral palettes (dark/light), accent themes, `COL_HDR_MUTED`, fonts, layout constants
    screens.cpp/h             Screen management, lazy creation, g_lvgl_mutex
    screens/                  screen_*.cpp (21 active ScreenId roots + ESTOP overlay module)
test/
  test_logic/               Native Unity tests (no hardware required)
  test_device_*/            On-device integration tests (require ESP32-P4)
docs/images/                Wiring diagrams, UI mockups
```

</details>

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

<div align="center">

<sub>DIY welding positioner &middot; ESP32-P4 &middot; Rotary welding table &middot; Pipe welding rotator &middot; Stepper driver &middot; NEMA 23 &middot; LVGL touch UI &middot; FreeRTOS</sub>

</div>
