# Instructables Guide Content
# DIY Welding Positioner Controller (ESP32-P4)

---

## PROJECT BASICS

**Title:** DIY Welding Positioner Controller (ESP32-P4 + LVGL Touch UI)

**Category:** Workshop

**Channel:** Metalworking

---

## INTRODUCTION

Build your own welding positioner controller with a 4.3" touchscreen, ESP32-P4 microcontroller, and off-the-shelf stepper motor hardware. This open-source project provides precise rotation control for TIG, MIG, and pipe welding — with 5 welding modes, real-time RPM adjustment, BLE remote, WiFi connectivity, and E-STOP safety.

### Why Build This?

Commercial welding rotators cost $500-5000. This controller gives you professional features for under $100 in electronics, using a NEMA 23 stepper motor and worm gear you may already have.

### Features
- 5 welding modes: Continuous, Pulse, Step, Jog, Timer
- 0.02-1.0 RPM range with live pot and button adjustment
- 4.3" capacitive touchscreen (800x480, LVGL 9.x UI)
- 8 selectable accent color themes
- BLE remote control (phone app via NUS)
- WiFi connectivity (connect to networks, OTA updates)
- Foot pedal support (analog speed + digital start)
- Direction switch (physical CW/CCW toggle)
- E-STOP safety with <0.5ms ISR response
- 16 program presets saved to flash storage
- Dual-core FreeRTOS (real-time motor + UI)
- 23 UI screens including settings, system info, calibration

### Demo Video

IMAGE: docs/images/social_preview.png (or embed YouTube: https://youtu.be/GygLl6XY-TM)

---

## SUPPLIES

### Electronics

| Part | Qty | Link/Source |
|------|-----|-------------|
| GUITION JC4880P443C dev board (ESP32-P4 + C6 + 4.3" display) | 1 | AliExpress: search "JC4880P443C" |
| NEMA 23 stepper motor | 1 | Any NEMA 23 (check current rating matches driver) |
| TB6600 stepper driver (or DM542T) | 1 | AliExpress / Amazon |
| 10k potentiometer (LA42DWQ-22 panel mount) | 1 | AliExpress |
| NC momentary push button (E-STOP) | 1 | Any NC mushroom button |
| SPDT toggle switch (direction CW/CCW) | 1 | Any toggle switch |
| Foot pedal with pot + switch | 1 | Optional |
| 24V DC power supply (5A+) | 1 | For motor driver |
| USB-C cable (for programming) | 1 | USB-C data cable |
| Hookup wire (22 AWG) | Various | Any |

### Mechanical

| Part | Qty | Notes |
|------|-----|-------|
| Worm gear reducer (~200:1 ratio) | 1 | 60T/40T + 133:1 worm = 199.5:1 |
| Drive roller (80mm diameter) | 1 | Rubber-coated |
| Pipe/workpiece support rollers | 2 | Match your pipe size |

IMAGE: docs/images/Wiring_diagram.v2.svg

IMAGE: docs/images/motor.worm.svg

### Software

| Tool | Purpose |
|------|---------|
| PlatformIO (VS Code extension) | Build and flash firmware |
| Python 3.12 | Required by PlatformIO (bundled with PIO) |
| Git | Version control |

GitHub repository: https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4

---

## STEP 1: Build the Firmware

### Clone and Build

Open a terminal and clone the repository:

```
git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
cd DIY-Welding-Positioner-ESP32-P4
```

Open the project folder in VS Code with the PlatformIO extension installed.

Select the environment: `esp32p4-release`

Click **Build** (checkmark icon) or run:

```
pio run
```

IMAGE: Screenshot of VS Code with PlatformIO build success

### Flash to Board

Connect the ESP32-P4 board via USB-C. Click **Upload** (arrow icon) or run:

```
pio run --target upload
```

IMAGE: Screenshot of successful upload output

---

## STEP 2: Wire the Hardware

### Wiring Diagram

IMAGE: docs/images/Wiring_diagram.v2.svg

### Connections

| ESP32-P4 Pin | Connects To | Notes |
|---|---|---|
| GPIO 50 (STEP) | TB6600 PUL+ | Step pulse |
| GPIO 51 (DIR) | TB6600 DIR+ | Direction |
| GPIO 52 (ENA) | TB6600 ENA- | LOW = motor ON |
| GPIO 34 (ESTOP) | E-STOP button | NC contact, pull-up |
| GPIO 29 (DIR SW) | Direction toggle | Pull-up, CW/CCW |
| GPIO 49 (POT) | 10k pot wiper | ADC input |
| GPIO 35 (PEDAL) | Foot pedal pot | Digital only (no ADC on P4) |
| GPIO 33 (PEDAL SW) | Foot pedal switch | Pull-up (optional) |
| I2C (GPIO 7/8) | ADS1115 | Pedal pot ADC (optional, addr 0x48) |
| 5V | TB6600 PUL-/DIR-/ENA+ | Logic power |
| 24V PSU+ | TB6600 VCC | Motor power |
| 24V PSU- | TB6600 GND | Common ground |

### Important Notes

- **Do NOT use GPIO 28 or GPIO 32** — they are PCB-routed to the ESP32-C6 co-processor for WiFi/BLE
- **ENA is active LOW** — HIGH means motor disabled (fail-safe)
- **E-STOP uses NC contact** — breaks connection when pressed
- **All grounds must be connected** — ESP32 GND, PSU GND, motor driver GND
- **Pot ADC range is 0-3315** with 11dB attenuation on ESP32-P4

IMAGE: Photo of wired breadboard/connections

---

## STEP 3: Mechanical Assembly

### Worm Gear Setup

IMAGE: docs/images/motor.worm.svg

The worm gear provides 199.5:1 reduction. With 1/8 microstepping (1600 steps/rev), the system delivers:

| Workpiece RPM | Motor RPM | Step Frequency |
|---|---|---|
| 0.02 | 4.0 | 399 Hz |
| 0.1 | 20.0 | 1,995 Hz |
| 1.0 | 199.5 | 19,950 Hz |

Mount the NEMA 23 motor to the worm gear input shaft. Mount the drive roller on the output shaft. Position support rollers to hold your workpiece.

IMAGE: Photo of mechanical assembly

---

## STEP 4: Power On and First Boot

Connect USB-C for power and programming. The controller will show a boot screen, then transition to the main screen.

IMAGE: Photo of boot screen

### Main Screen Layout

IMAGE: docs/images/main_screen.svg

- **Gauge** — shows current RPM (0.02-1.0 range)
- **CW/CCW button** — toggle rotation direction
- **RPM +/- buttons** — adjust speed
- **START/STOP** — continuous rotation mode
- **Mode buttons** — quick access to Jog, Pulse, Step, Timer

---

## STEP 5: Using the Controller

### Speed Control

Two methods for adjusting speed:

1. **Potentiometer** — turn the knob for analog speed control (0.02-1.0 RPM)
2. **+/- buttons** — digital adjustment
3. **Foot pedal** — analog speed via ADS1115 I2C ADC (if connected)

### 5 Welding Modes

IMAGE: docs/images/ui_screens.svg

| Mode | Description |
|---|---|
| **Continuous** | Steady rotation at set RPM |
| **Pulse** | Timed ON/OFF cycles (configurable) |
| **Step** | Rotate exact angle, then stop |
| **Jog** | Run while button is held |
| **Timer** | Run for set duration, then stop |

### BLE Remote Control

Connect via phone BLE apps (e.g., nRF Connect):
- Passkey: `123456`
- Commands: F=Start, S/X=Stop, R=CW, L=CCW, B=Reverse+Start

### E-STOP Safety

The E-STOP system has two layers:
1. **ISR (<0.5ms)** — hardware disable via interrupt, motor stops immediately
2. **Task (5ms)** — state transition to ESTOP, UI shows red overlay

---

## STEP 6: Settings and Customization

Navigate to **Settings** from the menu screen.

### Available Settings

| Setting | Options |
|---|---|
| WiFi | ON/OFF, scan, connect, hidden networks |
| Bluetooth | ON/OFF, scan, connect |
| Display | Brightness slider (20-100%), dim timeout |
| System Info | Core load, heap, PSRAM, WiFi status, uptime |
| Calibration | Motor calibration factor |
| Motor Config | Microstepping, acceleration, direction switch, pedal enable |
| About | Firmware version, hardware info |

### Program Presets

Save up to 16 presets with mode-specific parameters (RPM, pulse times, step angles, timer duration). Stored in flash memory and survives power cycles.

---

## STEP 7: Configuration Reference

### Key Parameters (config.h)

| Parameter | Value | Notes |
|---|---|---|
| MIN_RPM | 0.02 | Minimum workpiece speed |
| MAX_RPM | 1.0 | Limited by TB6600 (DM542T allows 5.0) |
| GEAR_RATIO | 199.5 | (60 * 133 / 40) worm gear |
| ACCELERATION | 10000 steps/s^2 | Smooth through resonance |
| START_SPEED | 100 Hz | Safe startup below minimum |

### Upgrading to DM542T

The TB6600 has no anti-resonance DSP. Upgrading to DM542T allows:
- MAX_RPM increase to 5.0
- Microstepping up to 1/32 without stalling
- Built-in stall detection

---

## FILES

Attach these files to the Instructable:

1. **docs/images/social_preview.png** — Hero image
2. **docs/images/Wiring_diagram.v2.svg** — Wiring diagram
3. **docs/images/motor.worm.svg** — Worm gear diagram
4. **docs/images/main_screen.svg** — Main screen mockup
5. **docs/images/ui_screens.svg** — All screen mockups
6. **firmware.bin** — Pre-built firmware (from .pio/build/esp32p4-release/)

---

## TIPS & TROUBLESHOOTING

- If the motor stalls at low RPM, increase microstepping to 1/16 or 1/32
- If the pot is noisy, check ADC wiring and grounding
- If the screen flickers, check PSRAM seating and MIPI-DSI cable
- If E-STOP doesn't work, verify NC contact wiring (GPIO 34 should read LOW when released)
- If WiFi/BLE crashes, check that GPIO 28/32 are not used for other purposes
- If direction switch doesn't work, enable it in Settings > Motor Config > Direction Switch

---

## CREDITS

- **ESP32-P4** by Espressif
- **LVGL 9.x** open-source graphics library
- **FastAccelStepper** by hinxx
- **PlatformIO** build system
- **ArduinoJson** by Benoit Blanchon
- **NimBLE** for BLE

License: MIT — https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4
