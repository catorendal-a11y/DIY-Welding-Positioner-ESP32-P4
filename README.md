<div align="center">

# DIY Welding Positioner Controller

### Precision Multi-Mode Welding Rotator for TIG, MIG, and Pipe Welding

**ESP32-P4 / ESP32-C6 &nbsp;&middot;&nbsp; Firmware v2.0.0**

<br>

<img src="docs/images/main_screen.svg" width="800" alt="TIG Rotator Controller — Main Screen">

<img src="docs/images/ui_screens.svg" width="800" alt="TIG Rotator Controller — All Screens">

<br>

Open-source welding positioner controller with BLE remote, WiFi connectivity,
and a glove-safe industrial touch UI built on dual-core FreeRTOS.

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

## Table of Contents

- [Quick Start](#quick-start)
- [Demo](#demo)
- [Features](#features)
- [UI Screens](#ui-screens)
- [Architecture](#industrial-rtos-architecture)
- [Gear System & Wiring](#gear-system)
- [BLE Remote Control](#ble-remote-control)
- [Bill of Materials](#bill-of-materials)
- [Performance Specifications](#performance-specifications)
- [Welding Modes](#welding-modes)
- [Configuration](#configuration)
- [Safety Notice](#safety-notice)
- [Troubleshooting](#troubleshooting)
- [Roadmap](#roadmap)
- [Project Structure](#project-structure)
- [License](#license)

---

## Quick Start

1. **Clone the repository:**

   ```bash
   git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
   cd DIY-Welding-Positioner-ESP32-P4
   ```

2. **Open in VS Code** with the **PlatformIO** extension installed.

3. **Select board environment:** `esp32p4-release` (GUITION JC4880P443C 4.3")

4. **Build and flash:**

   ```bash
   pio run -t upload
   ```

   Debug build with verbose serial logging:

   ```bash
   pio run -t upload -e esp32p4-debug
   ```

5. **Connect hardware** per the [wiring diagram](#wiring-diagram) below.

---

## Demo

Watch the system in action — UI interaction, motor rotation, screen navigation, E-STOP, direction switch, and pot control.

<div align="center">

[![DIY Welding Positioner Controller Demo](https://img.youtube.com/vi/GygLl6XY-TM/maxresdefault.jpg)](https://youtu.be/GygLl6XY-TM)

</div>

---

## Features

| Category | Details |
|:---|:---|
| **Welding Modes** | 5 modes — Continuous, Jog, Pulse, Step, and Countdown (configurable 1-10s delay) |
| **Speed Control** | Live RPM adjustment via potentiometer and touchscreen buttons |
| **Foot Pedal** | Analog speed input + digital start switch |
| **Direction Switch** | Physical CW/CCW toggle (GPIO 29) |
| **Touch UI** | LVGL 9.x glove-safe interface, high-contrast dark theme, 8 accent colors |
| **BLE Remote** | Phone control via Nordic UART Service — arm, start, stop, direction, RPM |
| **WiFi** | Network scanning, credential storage, OTA updates for C6 co-processor |
| **Program Presets** | Save/load up to 16 welding parameter sets to LittleFS flash |
| **Motor Config** | Microstepping (1/4 – 1/32), acceleration, calibration, direction invert |
| **Display Settings** | Brightness slider, dim timeout, theme color selection |
| **System Info** | Live CPU core load, free heap, PSRAM usage, WiFi status, uptime |
| **Hardware Safety** | NC E-STOP interrupt (<0.5 ms), software watchdog, CAS state transitions |
| **Thread Safety** | Mutex-protected stepper access, atomic cross-core variables, pending-flag patterns |

---

## UI Screens

The interface consists of **23 screens**, each purpose-built for industrial use with glove-safe touch targets.

| Screen | Description |
|:---|:---|
| **Main** | RPM gauge, start/stop, mode quick-access |
| **Menu** | Advanced mode selection and settings |
| **Continuous** | Constant rotation at set RPM |
| **Jog** | Touch-and-hold rotation for manual positioning |
| **Pulse** | ON/OFF cycle for tack welding |
| **Step** | Rotate exact angle, then stop |
| **Countdown** | Visual 3-2-1 before rotation starts |
| **Programs** | Preset list with save, load, delete |
| **Program Edit** | Full preset editor with on-screen keyboard |
| **Settings** | Hub for WiFi, Bluetooth, Display, System Info, Calibration, Motor Config, About |
| **WiFi** | Toggle, scan, connect, hidden networks, on-screen keyboard |
| **Bluetooth** | Toggle, scan, device list, on-screen keyboard |
| **Display** | Brightness slider, dim timeout |
| **System Info** | Core load, heap, PSRAM, WiFi, uptime |
| **Calibration** | Motor calibration factor adjustment |
| **Motor Config** | Microstepping, acceleration, direction switch, pedal enable |
| **About** | Firmware version, hardware info |
| **E-STOP Overlay** | Full-screen red overlay on any active screen |

---

## Industrial RTOS Architecture

Dual-core **FreeRTOS** design separating realtime motor control from UI rendering.

```
Core 0 (Realtime)                Core 1 (UI)
─────────────────                ──────────────────
safetyTask   (pri 5, 2 KB)      lvglTask    (pri 2, 64 KB)
motorTask    (pri 4, 5 KB)      storageTask (pri 1, 12 KB)
controlTask  (pri 3, 4 KB)
```

### Design Principles

| Principle | Implementation |
|:---|:---|
| **Task Isolation** | UI rendering cannot block motor pulse generation |
| **Hardware Timers** | RMT peripheral for jitter-free micro-stepping |
| **Fail-Safe** | E-STOP ISR cuts ENA pin in <0.5 ms |
| **Thread Safety** | Mutex on stepper access, atomic cross-core variables, pending-flag patterns |
| **Live Speed** | `applySpeedAcceleration()` for immediate RPM changes during rotation |

---

## Gear System

<div align="center">
  <img src="docs/images/motor.worm.svg" width="700" alt="Gear System — Worm Drive">
</div>

<div align="center">
  <img src="docs/images/stepper_setup.png" width="700" alt="Motor Assembly — NEMA 23 with Worm Gearbox and TB6600 Driver">
  <br><sub>NEMA 23 stepper with worm gear reducer and TB6600 microstep driver</sub>
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
| **GPIO 50** | STEP (Output) | RMT pulse to TB6600 PUL+ |
| **GPIO 51** | DIR (Output) | Direction to TB6600 DIR+ |
| **GPIO 52** | ENABLE (Output) | Active LOW to TB6600 EN+ |
| **GPIO 49** | POT (ADC Input) | 10k speed potentiometer |
| **GPIO 29** | DIR SWITCH (Input) | CW/CCW toggle, INPUT_PULLUP |
| **GPIO 34** | E-STOP (Input, ISR) | NC contact, active LOW |
| **GPIO 35** | PEDAL POT (ADC Input) | Foot pedal speed |
| **GPIO 33** | PEDAL SW (Input) | Foot pedal switch, active LOW |
| GPIO 7 / 8 | Touch I2C | GT911 (internal) |
| GPIO 28 / 32 | C6 UART | SDIO to ESP32-C6 (do not use) |
| GPIO 14-19, 54 | C6 SDIO | ESP-Hosted transport (do not use) |

> **Important:** GPIO 28 (C6_U0TXD) and GPIO 32 (C6_U0RXD) are PCB-routed to the ESP32-C6 co-processor. They cannot be used as general-purpose I/O when WiFi or BLE is active.

---

## BLE Remote Control

Connect via phone BLE apps (e.g., nRF Connect) using the Nordic UART Service.

| Character | Command |
|:---|:---|
| `0x00` | **Arm** — must arm within 5 s before start |
| `F` | Start continuous rotation |
| `S` / `X` | Stop |
| `R` | Set direction CW |
| `L` | Set direction CCW |
| `B` | Reverse + Start |

| Property | Value |
|:---|:---|
| **Service UUID** | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| **Passkey** | `123456` |
| **Notifications** | State + RPM pushed at 500 ms intervals (rate-limited to avoid SDIO saturation) |

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
| **Stepper Driver** | TB6600 (DM542T planned upgrade) | 1 |
| **Stepper Motor** | NEMA 23 (3 Nm torque) | 1 |
| **Gearbox** | Worm gear reducer (~200:1) | 1 |
| **Power Supply** | 24V DC, 5A+ | 1 |
| **Speed Pot** | 10k potentiometer | 1 |
| **Direction Switch** | SPDT toggle switch | 1 |
| **E-STOP** | NC mushroom button | 1 |
| **Foot Pedal** | Analog pot + momentary switch | 1 |

---

## Performance Specifications

| Parameter | Value |
|:---|:---|
| **Output RPM Range** | 0.02 – 1.0 RPM (TB6600), up to 5.0 RPM (DM542T) |
| **Gear Ratio** | 199.5 : 1 &ensp; (60 x 133 / 40) |
| **Microstepping** | 1/4, 1/8, 1/16, 1/32 (configurable) |
| **Motor Torque** | 3.0 Nm (NEMA 23) |
| **Control Resolution** | 0.01 RPM |
| **Display** | 800 x 480, landscape, LVGL 9.x |
| **Flash Usage** | ~27% &ensp; (1.8 MB / 6.5 MB) |
| **RAM Usage** | ~13% &ensp; (41 KB / 320 KB) |

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
#define MIN_RPM         0.02f   // Minimum workpiece RPM
#define MAX_RPM         1.0f    // Maximum workpiece RPM
#define GEAR_RATIO      199.5f  // Worm gear ratio (60 * 133 / 40)
#define ACCELERATION    10000   // Steps/s^2
#define START_SPEED     100     // Hz minimum for motor start
```

Settings can also be changed from the touchscreen via **Settings > Motor Config** and are persisted to LittleFS.

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
- [ ] BLE remote connects and controls motor
- [ ] WiFi connects and settings screen works

---

## Safety Notice

> **Warning:** This controller drives industrial stepper motors. Follow all safety precautions.

| Hazard | Precaution |
|:---|:---|
| **E-STOP** | NC contact hardware interrupt cuts motor enable in <0.5 ms |
| **Power Sequencing** | Never power motor without driver connected to coils |
| **Motor Coils** | Never connect/disconnect coils while driver is powered |
| **Voltage** | Verify 24V supply before connecting |

---

## Troubleshooting

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
<summary><b>WiFi or BLE causes crashes</b></summary>

- GPIO 28/32 are reserved for C6 co-processor — do not use for other purposes
- BLE notify rate is limited to 500 ms to avoid SDIO bus saturation

</details>

<details>
<summary><b>Settings not saving</b></summary>

- Check LittleFS partition is flashed (upload filesystem)
- storageTask handles writes with debounce — allow 1-2 seconds

</details>

---

## Known Limitations

- Single-axis control only
- TB6600 has no anti-resonance DSP — DM542T recommended for higher RPM
- WiFi/BLE share SDIO bus to C6 — simultaneous heavy traffic may cause latency
- GPIO 28/32 unavailable for GPIO use (C6 UART)

---

## Roadmap

**Completed:**

- [x] 5 welding modes (Continuous, Jog, Pulse, Step, Timer)
- [x] Program preset storage (LittleFS, 16 slots)
- [x] Live RPM adjustment (pot + buttons)
- [x] Foot pedal support
- [x] Direction switch
- [x] BLE remote control (NUS)
- [x] WiFi connectivity + network scanning
- [x] 8 accent color themes
- [x] Display settings (brightness, dim timeout)
- [x] System info screen (core load, heap, WiFi status)
- [x] Motor configuration UI
- [x] Thread-safe stepper access + atomic cross-core variables

**Planned:**

- [ ] Countdown before start (3-2-1 on screen, gives welder time to position)
- [ ] Increase MAX_RPM to 5.0 with DM542T
- [ ] Enclosure design (3D printable)
- [ ] Assembly guide

---

## Project Structure

```
src/
  main.cpp                  Setup, FreeRTOS tasks
  config.h                  Pinouts, gear ratio, RPM limits
  control/                  State machine + welding modes
    control.cpp               Core state machine with CAS transitions
    modes/                    continuous, jog, pulse, step_mode, timer
  motor/                    Stepper driver
    motor.cpp                 FastAccelStepper init, run, stop
    speed.cpp                 ADC pot, pedal, direction, RPM control
    acceleration.cpp          Acceleration ramps
    microstep.cpp             Microstepping config
    calibration.cpp           Calibration factor
  safety/                   E-STOP + watchdog
    safety.cpp                ISR, UI reset, state guard
  storage/                  LittleFS persistence
    storage.cpp               Settings/presets JSON, WiFi process, mutex
  ble/                      Bluetooth Low Energy
    ble.cpp                   NimBLE NUS service, scanning, OTA
  ui/                       LVGL display
    display.cpp               MIPI-DSI ST7701 init
    lvgl_hal.cpp              Flush callback, dim, touch polling
    theme.cpp/h               Color themes, font definitions
    screens.cpp/h             Screen management, lazy creation
    screens/                  23 screen files
docs/images/                Wiring diagrams, UI mockups
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

<div align="center">

<sub>DIY welding positioner &middot; ESP32-P4 &middot; Rotary welding table &middot; Pipe welding rotator &middot; TB6600 stepper driver &middot; NEMA 23 &middot; BLE remote &middot; WiFi &middot; LVGL touch UI &middot; FreeRTOS &middot; ESP32-C6 co-processor</sub>

</div>
