<div align="center">

# DIY Welding Positioner Controller (ESP32-P4/C6-GUITION)
**Precision Multi-Mode Welding Rotator for TIG, MIG, and Pipe Welding**

**Firmware Version:** v0.4.0
*(See Git tags for release history)*

<img src="docs/images/main_screen.svg" width="800" alt="TIG Rotator Controller — Main Screen">

**Open-source ESP32-P4 based welding positioner controller designed for rotary welding tables, pipe welding rotators, and automated fabrication systems.**

[![Status](https://img.shields.io/badge/Status-Active%20Development-brightgreen)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-P4](https://img.shields.io/badge/Platform-ESP32--P4-blue.svg)](https://espressif.com/)
[![Framework: ESP-IDF](https://img.shields.io/badge/Framework-ESP--IDF-green.svg)](https://docs.espressif.com/)
[![Graphics: LVGL](https://img.shields.io/badge/LVGL-9.x-blue.svg)](https://lvgl.io/)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange.svg)](https://platformio.org/)

</div>

---

## ⚡ Quick Start

1. **Clone the repository:**

```bash
git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
cd DIY-Welding-Positioner-ESP32-P4
```

2. **Open in VS Code** with the **PlatformIO** extension installed.
3. **Select board environment:** `esp32p4-release` (GUITION JC4880P443C 4.3")
4. **Build and flash:** Click the PlatformIO "Upload" button (➔), or run:

```bash
pio run -t upload
```

*(Since `default_envs` is configured, adding `-e esp32p4-release` is optional).*

**Optional debug build:**

```bash
pio run -t upload -e esp32p4-debug
```

*This enables verbose logging and debug diagnostics.*

5. **Connect hardware:** Wire your TB6600 driver, NEMA 23 stepper motor, and external power supply.

---

## 🎥 Demo

Watch the system in action: **UI interaction, Motor rotation, and Simulated welding passes.**

[![DIY Welding Positioner Controller Demo](https://img.youtube.com/vi/GygLl6XY-TM/maxresdefault.jpg)](https://youtu.be/GygLl6XY-TM)

## 🖥️ UI Screen Overview

<div align="center">
<img src="docs/images/ui_screens.svg" width="1440" alt="All UI Screens">
</div>

---

## 🔎 What Is This Project?

This project is a **DIY welding positioner controller** built using the powerful **ESP32-P4 microcontroller**. It is designed to control rotary welding tables, welding turntables, pipe welding rotators, and automated fabrication systems. 

Driven by a NEMA 23 stepper motor and a 200:1 worm gear, it ensures ultra-smooth low-RPM rotation ideal for circular weld seams and continuous TIG/MIG passes.

### 🎯 Project Goals
- Build a reliable, industrial-grade DIY welding positioner.
- Provide accessible open-source firmware.
- Create a beautiful, glove-safe industrial user interface.
- Enable massive customization for different fabrication setups.

---

## 🧠 Industrial RTOS Architecture

This project is built on a professional dual-core **FreeRTOS** architecture, separating critical realtime motor control from the heavy graphics processing.

```mermaid
graph TD
    subgraph Core 1: User Interface
        UI[LVGL 9.x GUI Task] -->|Events| Q[State Machine Queue]
        Touch[GT911 I2C Touch] --> UI
        Disp[MIPI-DSI ST7701S] --- UI
    end

    subgraph Core 0: Realtime Control
        SM[State Machine Task] -->|Target Hz| Motor[Motor Task]
        Q --> SM
        Pot[ADC Potentiometer] --> Motor
        
        subgraph Hardware Safety Layer
            ESTOP[NC E-STOP Button] -->|GPIO 33 ISR < 1ms| ENA[Hardware Disable]
            WDT[Task Watchdog Timer] -.->|Monitors| SM
            WDT -.->|Monitors| Motor
        end
    end

    Motor -->|RMT Hardware Pulses| TB[TB6600 Driver]
    ENA --> TB
    TB --> N23[NEMA 23 3Nm Stepper]
    GEAR --> TABLE((Rotary Table))

    style UI fill:#003366,stroke:#00E5FF,stroke-width:2px,color:#fff
    style Motor fill:#660000,stroke:#FF3333,stroke-width:2px,color:#fff
    style SM fill:#333333,stroke:#fff,stroke-width:1px,color:#fff
    style Hardware Safety Layer fill:#1a0000,stroke:#FF0000,stroke-width:2px,color:#fff
    style TB fill:#333,stroke:#00E5FF,stroke-width:1px,color:#fff
    style N23 fill:#333,stroke:#00E5FF,stroke-width:1px,color:#fff
    style GEAR fill:#555,stroke:#fff,stroke-width:1px,color:#fff
    style TABLE fill:#00E5FF,stroke:#000,stroke-width:2px,color:#000
```

### Core Design Principles
1. **Task Isolation:** UI rendering (Core 1) cannot block motor pulses (Core 0).
2. **Hardware Timers:** Motor steps are generated using the ESP32 RMT peripheral, ensuring jitter-free micro-stepping regardless of CPU load.
3. **Fail-Safe Safety:** The Emergency Stop is tied directly to a zero-latency hardware interrupt (ISR) that cuts the driver's enable pin in <0.5ms. A 2.0-second hardware watchdog (TWDT) monitors the control tasks.
4. **Acceleration Ramps:** Linear acceleration via `FastAccelStepper` with `setLinearAcceleration()` for smooth resonance-zone traversal prevents motor stalls and jerky weld starts.
5. **Live Speed Control:** RPM can be adjusted on-the-fly via potentiometer or touchscreen buttons during continuous rotation, using `applySpeedAcceleration()` for immediate effect.

---

## 📸 Real Hardware

<p align="center">
  <img src="docs/images/board_overview.jpg" width="45%" alt="ESP32-P4 Board" />
  <img src="docs/images/stepper_setup.png" width="45%" alt="Hardware setup" />
</p>

---

## ✨ Features

- **Multi-mode rotation:** Continuous, Jog, Pulse, Step, and Timer modes.
- **Live speed control:** On-the-fly RPM adjustment via potentiometer and touchscreen buttons during rotation.
- **LVGL touch interface:** Glove-safe, high-contrast industrial dark UI.
- **Hardware safety:** Dedicated NC E-STOP interrupt and software watchdog.
- **Smooth motion:** FastAccelStepper utilizing RMT hardware pulses with ISR IRAM-safe flags for PSRAM compatibility.
- **Program Presets:** Save and load custom welding parameters to onboard LittleFS flash memory.
- **Microstepping:** Supports 1/4, 1/8, 1/16, and 1/32 microstepping (configurable).

---

## 📊 Performance Specifications

| Parameter | Value |
|-----------|-------|
| **Output RPM Range** | 0.05 – 5.0 RPM (current: 1.0 RPM max with TB6600, pending DM542T upgrade) |
| **Gear Ratio** | 200:1 Worm Gear |
| **Microstepping** | 1/4, 1/8, 1/16, 1/32 (Selectable in Settings) |
| **Motor Torque** | 3.0 Nm (NEMA 23) |
| **Control Resolution**| 0.01 RPM |
| **Max Table Load** | Depends on bearing & frame design |

---

## 🧩 Requirements

- **PlatformIO:** Core 6.x or newer
- **ESP-IDF:** v5.5+ (via pioarduino core, pinned release)
- **LVGL:** 9.x
- **FastAccelStepper:** 0.33.14 (pinned)
- **Display:** ESP-IDF native MIPI-DSI panel driver (ST7701S-class). **Not LovyanGFX.**

### Hardware Check
- GUITION JC4880P443C ESP32-P4 4.3" Touch Display (480x800, MIPI-DSI)
- TB6600 stepper driver
- NEMA 23 motor
- 24–36V power supply

---

## 🧰 Bill of Materials (BOM)

| Component | Model / Specs | Qty |
|-----------|---------------|-----|
| **MCU Board** | GUITION JC4880P443C (480x800, ESP32-P4, MIPI-DSI) | 1 |
| **Stepper Driver** | TB6600 (DM542T planned upgrade) | 1 |
| **Stepper Motor** | NEMA 23 (3 Nm torque) | 1 |
| **Gearbox** | 200:1 Worm Gear Reducer | 1 |
| **Power Supply** | 24V DC, ≥5A recommended for NEMA 23 (3Nm) | 1 |
| **Controls** | 10k Potentiometer (Speed) & NC E-STOP Button | 1 |

---

## 🎯 Target Hardware (Designed & Configured For)

This firmware is designed and configured for the following hardware.
All components have been validated on real hardware.

- GUITION JC4880P443C ESP32-P4 4.3" Touch Display (480x800)
- TB6600 Stepper Driver
- NEMA 23 (3 Nm torque)
- 200:1 Worm Gear Reducer
- 24V / 5A DC Power Supply
- 10k Potentiometer
- NC Emergency Stop Button

---

## 🔧 Supported Drivers

- **TB6600** (Current standard configuration)
- **DM542** (Planned / Drop-in replacement)

---

## 🔌 Wiring Diagram

<div align="center">
  <img src="docs/images/Wiring_diagram.v2.svg" width="800" alt="Wiring Diagram">
</div>

> **See also:** [Detailed Hardware Setup Guide](docs/HARDWARE_SETUP.md) · [EMI Mitigation Guide](docs/EMI_MITIGATION.md) · [Safety System](docs/SAFETY_SYSTEM.md)

---

## ⚙️ Worm Gear Assembly

<div align="center">
  <img src="docs/images/motor.worm.svg" width="600" alt="Worm Gear Assembly">
</div>

> The 200:1 worm gear reducer converts the stepper motor's high-speed, low-torque output into the ultra-smooth, high-torque rotation needed for welding positioners.

---

## 📂 Project Directory Structure

```text
DIY-Welding-Positioner-ESP32-P4/
├── src/
│   ├── main.cpp
│   ├── motor/          # FastAccelStepper logic & RMT pulses
│   ├── control/        # Welding modes (continuous, jog, pulse, step, timer)
│   ├── safety/         # E-STOP interrupt & hardware watchdog
│   ├── ui/             # LVGL 9.x dashboards & themes
│   └── config.h        # Pinouts & physical gear ratios
├── test/               # Logic verification & simulation suites
├── docs/
│   └── images/         # Wiring diagrams & UI mockups
├── platformio.ini      # Build environment for ESP32-P4
└── README.md           # This master documentation
```

---

## 📍 Pinout

<div align="center">
  <img src="docs/images/pinout.jpg" width="500" alt="ESP32 Pinout Diagram">
</div>

| ESP32 Pin | Function | Notes |
|-----------|----------|-------|
| GPIO 50 | **STEP (Output)** | Step pulse output |
| GPIO 51 | **DIR (Output)** | Direction control (CW/CCW) |
| GPIO 52 | **ENABLE (Output)** | Active LOW to enable motor |
| GPIO 49 | **ADC (Input)** | Potentiometer speed control (verify ADC capability on your board revision) |
| GPIO 33 | **E-STOP (Input, Interrupt)** | NC Contact (Active LOW halt) |

*(Note: Touch screen I2C is wired internally to GPIO 7/8. Display MIPI-DSI uses dedicated lanes).*

---

## ⚙️ Configuration

Key physical parameters can be adjusted right at the top of the header files to perfectly match your specific mechanical build. 

Open `src/config.h` to tweak:

```cpp
#define MICROSTEPS      8      // Must match dip-switches on your TB6600
#define GEAR_RATIO      200    // Worm gear ratio
#define MAX_RPM         1.0    // Upper limit of the UI gauge
#define MIN_RPM         0.05   // Lower limit
#define ACCELERATION    7000   // Stepper acceleration (steps/s²)
```

---

## 📝 Welding Modes

| Mode | Description |
|------|-------------|
| **Continuous** | Standard rotation. Starts spinning continuously at the set RPM when you press "ON", and stops when you hit "STOP". |
| **Jog** | Manual override. The motor spins only as long as your finger is touching the screen icon. Perfect for aligning start points. |
| **Pulse** | Specialized for tack-welding. Rotates a specific distance, pauses for the tack to fuse, and automatically rotates again. |
| **Step** | Rotates an exact degree amount (e.g., 90° for a quarter-turn) and stops. |
| **Timer** | Rotates at a set speed for an exact duration (e.g., 30 seconds). |

---

## 🧪 First Bench Test Checklist

Before connecting mechanical load:

- [ ] Display boots successfully
- [x] Touch input responds correctly
- [x] Motor rotates at target RPM
- [x] E-STOP halts motion immediately
- [x] No watchdog resets observed
- [x] Live RPM adjustment works (pot + buttons) during rotation
- [x] All 5 welding modes tested (Continuous, Jog, Pulse, Step, Timer)
- [x] Program preset save/load verified

---

## ⚠️ Safety Notice

- **E-STOP:** The E-STOP uses an external hardware interrupt. Breaking the NC circuit instantly sets the motor speed and acceleration to 0 and cuts the enable pin.
- **Power Sequencing:** Never power the motor without the TB6600 driver properly connected to the coils.
- **Motor Coils (CRITICAL):** Never connect or disconnect motor coils while the stepper driver is powered. This will destroy the driver.
- **Voltage Verification:** Always verify your 24V-36V power supply output before connecting it to the system.
- **Testing:** Always test new configurations with low motor current settings first to prevent mechanical damage.

---

## 🛠️ Troubleshooting

### Motor does not move
- Check STEP/DIR wiring continuity.
- Verify ENABLE pin logic (try tying it directly to LOW/GND if motor lacks holding torque).
- Confirm driver power supply is active and outputs 24V–36V at ≥5A.

### Wrong rotation direction
- Swap the DIR pin polarity in firmware, OR physically reverse one motor coil wiring pair (A+ and A-).

### No display on boot
- Verify ESP32-P4 drivers installed correctly via ESP-IDF.
- If flashing from VS Code, ensure you are not missing the PSRAM init settings in platformio.ini.

---

## ⚠️ Known Limitations

- Single-axis control only.
- TB6600 driver has no anti-resonance DSP — motor may stall at certain RPM ranges due to resonance. DM542T upgrade recommended.
- Requires manual configuration of your specific gear ratio in `config.h`.
- Not tested with external servo motors (pure step/dir steppers only).

---

## 🛣️ Roadmap

- [x] Basic rotation and UI setup
- [x] All 5 welding modes (Continuous, Pulse, Step, Jog, Timer)
- [x] Program Preset Storage (LittleFS + ArduinoJson)
- [x] Live RPM adjustment (pot + buttons during rotation)
- [x] Microstepping selection (1/4, 1/8, 1/16, 1/32)
- [x] Accent color theme system (8 colors)
- [x] DM542T driver support
- [ ] Increase MAX_RPM to 5.0 with DM542T
- [ ] Enclosure design files (3D printable)
- [ ] Assembly guide

---

## 📄 License

This project is licensed under the MIT License.
See the LICENSE file in the root directory for more details.

---

## 🔎 Keywords

DIY welding positioner, ESP32 welding controller, Rotary welding table, Welding rotator controller, Pipe welding turntable, TB6600 stepper driver, NEMA 23 welding motor, Welding automation controller, Industrial DIY welding, ESP32-P4 LVGL controller
