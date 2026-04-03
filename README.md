<div align="center">

# DIY Welding Positioner Controller (ESP32-P4/C6)
**Precision Multi-Mode Welding Rotator for TIG, MIG, and Pipe Welding**

**Firmware Version:** v2.0.0

<img src="docs/images/main_screen.svg" width="800" alt="TIG Rotator Controller - Main Screen">

**Open-source ESP32-P4 based welding positioner controller with BLE remote, WiFi connectivity, and a glove-safe industrial touch UI.**

[![Status](https://img.shields.io/badge/Status-Active%20Development-brightgreen)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-P4](https://img.shields.io/badge/Platform-ESP32--P4-blue.svg)](https://espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino%20ESP32-green.svg)](https://docs.espressif.com/)
[![Graphics: LVGL](https://img.shields.io/badge/LVGL-9.x-blue.svg)](https://lvgl.io/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)

If this project helped you build something, consider giving it a star.

</div>

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

**Optional debug build** (verbose serial logging):

```bash
pio run -t upload -e esp32p4-debug
```

5. **Connect hardware:** Wire per the wiring diagram below.

---

## Demo

Watch the system in action: **UI interaction, Motor rotation, Screen navigation, E-STOP, Direction switch, and Pot control.**

[![DIY Welding Positioner Controller Demo](https://img.youtube.com/vi/GygLl6XY-TM/maxresdefault.jpg)](https://youtu.be/GygLl6XY-TM)

---

## Features

- **5 welding modes:** Continuous, Jog, Pulse, Step, and Timer
- **Live speed control:** On-the-fly RPM adjustment via potentiometer and touchscreen buttons
- **Foot pedal support:** Analog speed + digital start switch
- **Direction switch:** Physical CW/CCW toggle (GPIO29)
- **LVGL 9.x touch UI:** Glove-safe, high-contrast industrial dark theme with 8 accent colors
- **BLE remote control:** Phone app via Nordic UART Service (NUS) with arm/start/stop/direction/RPM
- **WiFi connectivity:** Connect to networks, save credentials, OTA updates for C6 co-processor
- **Program presets:** Save/load up to 16 welding parameter sets to LittleFS flash
- **Motor configuration:** Microstepping (1/4-1/32), acceleration, calibration, direction switch toggle
- **Display settings:** Brightness slider, dim timeout, theme colors
- **System info:** Live CPU core load, free heap, PSRAM, WiFi status, uptime
- **Hardware safety:** NC E-STOP interrupt (<0.5ms), software watchdog, CAS state transitions
- **Thread-safe:** Mutex-protected stepper access, atomic cross-core variables, pending-flag patterns

---

## UI Screens (23 screens)

| Screen | Description |
|--------|-------------|
| **Main** | RPM gauge, start/stop, mode quick-access |
| **Menu** | Advanced mode selection and settings |
| **Continuous / Jog / Pulse / Step / Timer** | Mode-specific controls |
| **Programs** | Preset list with save/load/delete |
| **Program Edit** | Full preset editor with keyboard input |
| **Settings** | Hub: WiFi, Bluetooth, Display, System Info, Calibration, Motor Config, About |
| **WiFi** | Toggle, scan, connect, hidden networks, on-screen keyboard |
| **Bluetooth** | Toggle, scan, device list, on-screen keyboard |
| **Display** | Brightness slider, dim timeout |
| **System Info** | Core load, heap, PSRAM, WiFi, uptime |
| **Calibration** | Motor calibration factor |
| **Motor Config** | Microstepping, acceleration, direction switch, pedal enable |
| **About** | Firmware version, hardware info |
| **E-STOP Overlay** | Full-screen red overlay on any screen |

---

## Industrial RTOS Architecture

Dual-core **FreeRTOS** separating realtime motor control from UI rendering.

```
Core 0 (Realtime)          Core 1 (UI)
------------------          ------------------
safetyTask  (pri 5, 2KB)    lvglTask   (pri 2, 64KB)
motorTask   (pri 4, 5KB)    storageTask (pri 1, 12KB)
controlTask (pri 3, 4KB)
```

- **Core 0:** Motor pulses via RMT, ADC polling, state machine, safety ISR
- **Core 1:** LVGL rendering, touch input, screen updates, storage flush, WiFi/BLE

### Design Principles
1. **Task isolation:** UI cannot block motor pulses
2. **Hardware timers:** RMT peripheral for jitter-free micro-stepping
3. **Fail-safe:** E-STOP ISR cuts ENA pin in <0.5ms
4. **Thread safety:** Mutex on stepper access, atomic cross-core variables, pending-flag patterns for UI callbacks
5. **Live speed:** `applySpeedAcceleration()` for immediate RPM changes during rotation

---

## Wiring Diagram

<div align="center">
  <img src="docs/images/Wiring_diagram.v2.svg" width="800" alt="Wiring Diagram">
</div>

### Pinout

| ESP32-P4 Pin | Function | Notes |
|-------------|----------|-------|
| **GPIO 50** | STEP (Output) | RMT pulse to TB6600 PUL+ |
| **GPIO 51** | DIR (Output) | Direction to TB6600 DIR+ |
| **GPIO 52** | ENABLE (Output) | Active LOW to TB6600 EN+ |
| **GPIO 49** | POT (ADC Input) | 10k speed potentiometer |
| **GPIO 29** | DIR SWITCH (Input) | CW/CCW toggle, INPUT_PULLUP |
| **GPIO 34** | E-STOP (Input, ISR) | NC contact, active LOW |
| **GPIO 35** | PEDAL POT (ADC Input) | Foot pedal speed |
| **GPIO 33** | PEDAL SW (Input) | Foot pedal switch, active LOW |
| GPIO 7/8 | Touch I2C | GT911 (internal) |
| GPIO 28/32 | C6 UART | SDIO to ESP32-C6 (do not use) |
| GPIO 14-19, 54 | C6 SDIO | ESP-Hosted transport (do not use) |

> **Important:** GPIO 28 (C6_U0TXD) and GPIO 32 (C6_U0RXD) are PCB-routed to the ESP32-C6 co-processor. They cannot be used as general-purpose I/O when WiFi or BLE is active.

---

## BLE Remote Control

Connect via phone BLE apps (e.g., nRF Connect) using the Nordic UART Service:

| Character | Command |
|-----------|---------|
| `0x00` | **Arm** (must arm within 5s before start) |
| `F` | Start continuous rotation |
| `S` / `X` | Stop |
| `R` | Set direction CW |
| `L` | Set direction CCW |
| `B` | Reverse + Start |

- **Service:** `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **Passkey:** `123456`
- **Notifications:** State + RPM pushed at 500ms intervals (rate-limited to avoid SDIO saturation)

---

## Bill of Materials (BOM)

| Component | Model / Specs | Qty |
|-----------|---------------|-----|
| **MCU Board** | GUITION JC4880P443C (800x480, ESP32-P4 + ESP32-C6, MIPI-DSI) | 1 |
| **Stepper Driver** | TB6600 (DM542T planned upgrade) | 1 |
| **Stepper Motor** | NEMA 23 (3 Nm torque) | 1 |
| **Gearbox** | Worm gear reducer (~200:1) | 1 |
| **Power Supply** | 24V DC, 5A+ | 1 |
| **Speed Pot** | 10k potentiometer | 1 |
| **Direction Switch** | SPDT toggle switch | 1 |
| **E-STOP** | NC mushroom button | 1 |
| **Foot Pedal** | Analog pot + momentary switch | 1 (optional) |

---

## Performance Specifications

| Parameter | Value |
|-----------|-------|
| **Output RPM Range** | 0.02 - 1.0 RPM (TB6600), up to 5.0 RPM (DM542T) |
| **Gear Ratio** | 199.5:1 (60 x 133 / 40) |
| **Microstepping** | 1/4, 1/8, 1/16, 1/32 (configurable) |
| **Motor Torque** | 3.0 Nm (NEMA 23) |
| **Control Resolution** | 0.01 RPM |
| **Display** | 800x480 landscape, LVGL 9.x |
| **Flash Usage** | ~27% (1.8 MB / 6.5 MB) |
| **RAM Usage** | ~13% (41 KB / 320 KB) |

---

## Welding Modes

| Mode | Description |
|------|-------------|
| **Continuous** | Runs at set RPM. Start with ON, stop with STOP. |
| **Jog** | Touch-and-hold rotation for manual positioning. |
| **Pulse** | Rotate ON ms, pause OFF ms, repeat. For tack welding. |
| **Step** | Rotate exact angle (e.g., 90 deg), then stop. |
| **Timer** | Rotate at set speed for exact duration, then auto-stop. |

---

## Configuration

Open `src/config.h` to adjust:

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

- **E-STOP:** NC contact hardware interrupt cuts motor enable in <0.5ms
- **Power Sequencing:** Never power motor without driver connected to coils
- **Motor Coils:** Never connect/disconnect coils while driver is powered
- **Voltage:** Verify 24V supply before connecting

---

## Troubleshooting

### Motor does not move
- Check STEP/DIR/ENA wiring
- Verify ENA logic (LOW = enabled)
- Confirm driver power supply is active

### Wrong rotation direction
- Swap DIR polarity in firmware, or reverse A+/A- coil wiring

### WiFi or BLE causes crashes
- GPIO 28/32 are reserved for C6 co-processor — do not use for other purposes
- BLE notify rate is limited to 500ms to avoid SDIO bus saturation

### Settings not saving
- Check LittleFS partition is flashed (upload filesystem)
- storageTask handles writes with debounce — allow 1-2 seconds

---

## Known Limitations

- Single-axis control only
- TB6600 has no anti-resonance DSP — DM542T recommended for higher RPM
- WiFi/BLE share SDIO bus to C6 — simultaneous heavy traffic may cause latency
- GPIO 28/32 unavailable for GPIO use (C6 UART)

---

## Roadmap

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
- [ ] Countdown before start (3-2-1 on screen, gives welder time to position)
- [ ] Increase MAX_RPM to 5.0 with DM542T
- [ ] Enclosure design (3D printable)
- [ ] Assembly guide

---

## Project Structure

```
src/
  main.cpp              # Setup, FreeRTOS tasks
  config.h              # Pinouts, gear ratio, RPM limits
  control/              # State machine + welding modes
    control.cpp         # Core state machine with CAS transitions
    modes/              # continuous, jog, pulse, step_mode, timer
  motor/                # Stepper driver
    motor.cpp           # FastAccelStepper init, run, stop
    speed.cpp           # ADC pot, pedal, direction, RPM control
    acceleration.cpp    # Acceleration ramps
    microstep.cpp       # Microstepping config
    calibration.cpp     # Calibration factor
  safety/               # E-STOP + watchdog
    safety.cpp          # ISR, UI reset, state guard
  storage/              # LittleFS persistence
    storage.cpp         # Settings/presets JSON, WiFi process, mutex
  ble/                  # Bluetooth Low Energy
    ble.cpp             # NimBLE NUS service, scanning, OTA
  ui/                   # LVGL display
    display.cpp         # MIPI-DSI ST7701 init
    lvgl_hal.cpp        # Flush callback, dim, touch polling
    theme.cpp/h         # Color themes, font definitions
    screens.cpp/h       # Screen management, lazy creation
    screens/            # 23 screen files
docs/images/            # Wiring diagrams, UI mockups
```

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

## Keywords

DIY welding positioner, ESP32-P4 welding controller, Rotary welding table, Pipe welding rotator, TB6600 stepper driver, NEMA 23, BLE remote, WiFi connected, LVGL touch UI, FreeRTOS, ESP32-C6 co-processor
