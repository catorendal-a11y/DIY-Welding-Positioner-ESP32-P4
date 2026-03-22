# 🔧 DIY Welding Positioner Controller (ESP32-P4)
**Precision Multi-Mode Welding Rotator for TIG, MIG, and Pipe Welding**

**Firmware Version:** v0.3.1-beta
*(See Git tags for release history)*

<img src="docs/images/ui_mockup.svg" width="600" alt="DIY Welding Positioner UI Design">

## 🚀 Quick Start
1. **Clone the repo:** `git clone https://github.com/catorendal-a11y/tig-rotator-esp32p4.git`
2. **Open in VS Code + PlatformIO.**
3. **Select board environment:** `esp32p4-release` (Waveshare ESP32-P4 4.3")
4. **Build and flash:** 
```bash
pio run -t upload
```
*(Since `default_envs` is configured, adding `-e esp32p4-release` is optional).*

---

## 🧠 Industrial Architecture (RTOS-Native)
Built on a **High-Performance Multi-Core RISC-V Architecture** for mission-critical reliability:

- **Dual-Core Task Isolation:**
  - **Core 0 (High Priority):** Real-Time Motor Control & [Safety Interrupts](docs/SAFETY_SYSTEM.md).
  - **Core 1 (Low Priority):** LVGL GUI, Touch Input, & Storage.
- **Fail-Safe Operation:**
  - **Hardware Watchdog (TWDT):** 2.0s hardware reset if motor or safety tasks stall.
  - **Instant E-STOP ISR:** Microsecond-response hardware interrupt on `GPIO 33`.
  - **Acceleration Ramping:** Linear & S-Curve ramps to prevent step loss and mechanical shock.
- **Health Monitoring:** Real-time stack watermarking and heap analytics (accessible via Serial Debug).

---

## 🛠️ Hardware Requirements
- **MCU:** ESP32-P4 (360MHz Dual RISC-V).
- **Display:** 4.3" MIPI-DSI LCD (ST7701S).
- **Stepper Driver:** TB6600, DM542, or any Opto-isolated STEP/DIR driver.
- **Power:** 24V 5A PSU + DC-DC Buck (for ESP32-P4).

### 🎯 Target Hardware & [Setup Guide](docs/HARDWARE_SETUP.md)
- Waveshare ESP32-P4 4.3" Touch Display
- TB6600 Stepper Driver
- NEMA 23 (3 Nm torque)
- 24V / 5A DC Power Supply
- **Metal Case mandatory** (See [EMI Mitigation Guide](docs/EMI_MITIGATION.md))

---

## 🖥️ UI & Control Modes
The system features a professional **System Status Panel** on all screens displaying:
- **RPM / SFM:** Current calculated surface speed.
- **Direction:** (CW/CCW) visualization.
- **State:** (IDLE, RUN, FAULT, E-STOP).

| Mode | Use Case | Features |
|------|----------|----------|
| **Continuous** | Standard pipe welding | Real-time RPM adjustment (0.1–3.0) |
| **Jog** | Part setup / Tack welding | Momentary CW/CCW overrides |
| **Pulse** | Heat control (thin-wall) | Configurable On/Pause timing |
| **Step** | Flanges / Bolt patterns | Precise angle increments |
| **Timer** | Production runs | Auto-stop after set duration |

---

## 📂 Project Directory Structure
├── src/
│   ├── main.cpp        # FreeRTOS task orchestration & watchdog
│   ├── motor/          # FastAccelStepper logic & RMT pulses
│   ├── control/        # [State Machine](src/control/control.h) (Idle/Run/Fault/E-Stop)
│   ├── safety/         # Hard interrupt logic & hardware watchdog
│   ├── ui/             # LVGL 8.x dashboards & themes
│   └── config.h        # Pinouts & physical gear ratios
├── test/               # Logic verification & simulation suites
└── docs/               # [Safety](docs/SAFETY_SYSTEM.md), [EMI](docs/EMI_MITIGATION.md), [Setup](docs/HARDWARE_SETUP.md)

---

## 📍 Pinout
| Function | Pin | Note |
|----------|-----|------|
| **STEP+** | GPIO 50 | 3.3V Logic |
| **DIR+** | GPIO 51 | 3.3V Logic |
| **ENA+** | GPIO 52 | Motor Enable |
| **POT Wiper**| GPIO 49 | 12-bit ADC |
| **E-STOP** | GPIO 33 | **NC** (Hardware Interrupt) |

---

## ⚖️ License
MIT License - Developed by @catorendal-a11y.
