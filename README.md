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

**Optional debug build:**
```bash
pio run -t upload -e esp32p4-debug
```
*This enables verbose logging and debug diagnostics.*

5. **Connect hardware:** Wire your TB6600 driver, NEMA 23 stepper motor, and external power supply.

---

## 🧠 Industrial Architecture (RTOS-Native)
Unlike basic Arduino projects, this firmware is built on a **High-Performance Multi-Core RTOS Architecture** designed for mission-critical reliability in high-noise welding environments:

- **Dual-Core Task Isolation:**
  - **Core 0 (High Priority):** Dedicated to Real-Time Motor Control, Safety Interrupts, and ADC speed filtering.
  - **Core 1 (Low Priority):** Dedicated to the LVGL GUI, Touch Input, and Storage/Health monitoring.
- **Hardware Safety:**
  - **Hard Reset Watchdog:** ESP32-P4 internal Task Watchdog (TWDT) monitors motor & safety tasks.
  - **Boot-Safe ENA:** Stepper Enable is held HIGH (Disconnected) until all systems are healthy.
  - **Instant E-STOP ISR:** Dedicated hardware interrupt fires in microseconds to freeze motion.
- **Health Monitoring:** Real-time stack watermarking and heap analytics available in Debug builds.

---

## 🛠️ Hardware Requirements
- **MCU:** ESP32-P4 (360MHz Dual RISC-V).
- **Display:** 4.3" MIPI-DSI LCD (ST7701S). *Note: LovyanGFX is NOT supported for MIPI-DSI on P4.*
- **Touch:** GT911 (I2C).
- **Stepper Driver:** TB6600, DM542, or any Opto-isolated STEP/DIR driver (24V-36V).
- **Power:** 24V 5A PSU + DC-DC Buck (for ESP32-P4).

### 🛒 Bill of Materials (BOM)
- [Waveshare ESP32-P4 4.3" Dev Board](https://www.waveshare.com/esp32-p4-touch-4.3.htm)
- [NEMA 23 Stepper Motor](https://www.google.com/search?q=nema+23)
- [TB6600 Stepper Driver](https://www.google.com/search?q=tb6600)
- [K11-100 or K12-100 Rotary Table](https://www.google.com/search?q=welding+rotary+chuck)

---

## 🎯 Target Hardware (Designed & Configured For)
This firmware is designed and configured for the following hardware. Full validation is currently in progress.
- Waveshare ESP32-P4 4.3" Touch Display
- TB6600 Stepper Driver
- NEMA 23 (3 Nm torque)
- RV30 Worm Gear Reducer (60:1)
- 24V / 5A DC Power Supply
- 10k Potentiometer
- NC Emergency Stop Button

---

## ⚠️ Critical: EMI & Safety for TIG/MIG
Industrial environments (especially TIG HF-start) generate extreme Electromagnetic Interference (EMI):
- **Grounded Metal Enclosure:** Required to shield the MCU from RF plasma noise.
- **Star Grounding:** Prevents ground loops that trigger false E-STOPS.
- **Cable Shielding:** Shielded motor and signal wires are mandatory.
- See the full [EMI Mitigation Guide](docs/EMI_MITIGATION.md) and [Hardware Setup Guide](docs/HARDWARE_SETUP.md) for detailed schematics.

---

## 📂 Project Directory Structure
├── src/
│   ├── main.cpp        # FreeRTOS task orchestration & watchdog
│   ├── motor/          # FastAccelStepper logic & RMT pulses
│   ├── control/        # Welding modes (continuous, jog, pulse, step, timer)
│   ├── safety/         # E-STOP interrupt & hardware watchdog
│   ├── ui/             # LVGL 8.x dashboards & themes
│   └── config.h        # Pinouts & physical gear ratios
├── lib/
│   └── esp_lcd_st7701/ # ESP32-P4 Native MIPI Driver
└── docs/               # Schematics & EMI Mitigation

---

## 📍 Pinout
| Function | Pin | Note |
|----------|-----|------|
| **STEP+** | GPIO 50 | 3.3V Logic |
| **DIR+** | GPIO 51 | 3.3V Logic |
| **ENA+** | GPIO 52 | Motor Enable |
| **POT Wiper**| GPIO 49 | 12-bit ADC |
| **E-STOP** | GPIO 33 | **NC** (Hardware Interrupt) |

*(Note: Touch screen I2C is wired internally to GPIO 7/8. Display MIPI-DSI uses dedicated lanes).*

---

## ⚙️ Configuration
Adjust these in `src/config.h`:
```cpp
#define MOTOR_MICROSTEPS 8      // Must match dip-switches on your TB6600
#define MOTOR_GEAR_RATIO 60     // e.g., 60:1 Worm Gear
#define MAX_RPM 3.0             // Upper limit of the UI gauge
#define ACCELERATION 500        // Stepper acceleration (steps/s²)
```

---

## ⚖️ License
MIT License - See [LICENSE](LICENSE) for details. Developed by @catorendal-a11y.
