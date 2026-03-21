# 🔧 TIG Welding Rotator Controller

**Precision rotator controller for TIG pipe welding** — built on ESP32-P4 with a 4.3" MIPI-DSI touch display and LVGL interface.

> Glove-safe touch UI · Variable-speed stepper motor · Emergency stop · Multiple welding modes

---

## ✨ Features

| Feature | Details |
|---------|---------|
| **Touch Display** | 4.3" IPS 800×480 MIPI-DSI (EK79007) + GT911 capacitive touch |
| **Motor Control** | NEMA 23 stepper via TB6600 driver, 0.1–3.0 RPM range |
| **Gearbox** | 20:1 worm gear reduction for ultra-smooth rotation |
| **Safety** | Hardware E-STOP (NC contact), watchdog timer, fail-safe motor disable |
| **Modes** | Continuous · Jog · Pulse · Step · Timer · Programmable sequences |

## 🖥️ Hardware

### Controller Board
- **MCU**: [Waveshare ESP32-P4](https://www.waveshare.com/) 4.3" Touch Display Dev Board
- **Processor**: ESP32-P4 dual-core RISC-V @ 400 MHz
- **Memory**: 32 MB PSRAM, 16 MB Flash
- **Display**: MIPI-DSI interface, 800×480 IPS

### Motor System
| Component | Specification |
|-----------|--------------|
| Motor | NEMA 23 (1–2 Nm) |
| Driver | TB6600 (8 microsteps) |
| Gearbox | 20:1 worm gear |
| Workpiece | ⌀300 mm max |
| Rollers | ⌀80 mm |

### Wiring (GPIO Header)

| Function | GPIO | Notes |
|----------|------|-------|
| Step | 50 | RMT pulse output |
| Direction | 51 | CW=HIGH, CCW=LOW |
| Enable | 52 | Active LOW |
| E-STOP | 33 | NC contact, active LOW |
| Potentiometer | 49 | ADC speed control |

## 🔧 Building

### Prerequisites
- [PlatformIO](https://platformio.org/) (VS Code extension recommended)
- USB-C cable

### Compile & Flash

```bash
# Compile
pio run -e esp32p4-touch-43

# Upload to board
pio run -t upload -e esp32p4-touch-43

# Monitor serial output
pio device monitor -b 115200
```

## 📁 Project Structure

```
├── platformio.ini              # Build configuration (pioarduino ESP32-P4)
├── src/
│   ├── main.cpp                # Entry point, FreeRTOS tasks
│   ├── config.h                # GPIO pins, motor params, display config
│   ├── ui/
│   │   ├── display.cpp/h       # MIPI-DSI + GT911 touch driver
│   │   ├── lvgl_hal.cpp/h      # LVGL 8.x HAL (flush, touch input)
│   │   ├── theme.h             # Colors, sizes, layout constants
│   │   ├── screens.cpp/h       # Screen manager
│   │   └── screens/            # 11 individual screen implementations
│   ├── motor/
│   │   ├── motor.cpp/h         # FastAccelStepper motor control
│   │   └── speed.cpp/h         # ADC filtering, RPM conversion
│   ├── control/
│   │   ├── control.cpp/h       # State machine, mode dispatch
│   │   └── modes/              # Continuous, Jog, Pulse, Step, Timer
│   └── safety/
│       └── safety.cpp/h        # E-STOP interrupt, watchdog
├── lib/
│   ├── lv_conf.h               # LVGL configuration
│   ├── esp_lcd_ek79007/        # EK79007 MIPI-DSI panel driver
│   ├── esp_lcd_touch/          # Touch abstraction layer
│   └── esp_lcd_touch_gt911/    # GT911 touch driver
└── docs/                       # EMI test, E-STOP timing, load test docs
```

## 🛡️ Safety Design

- **E-STOP**: Normally-closed contact on GPIO 33 — motor disabled instantly on interrupt
- **Watchdog**: 500 ms hardware watchdog — motor stops if software hangs
- **Startup**: Motor always starts disabled, requires explicit user activation
- **Fail-safe**: Motor enable is active-LOW — power loss = motor stops

## 📝 Welding Modes

| Mode | Description |
|------|-------------|
| **Continuous** | Constant speed rotation, adjustable via pot or UI |
| **Jog** | Press-and-hold for manual positioning |
| **Pulse** | Fixed-angle increments (e.g., tack spacing) |
| **Step** | Single-step mode for precise alignment |
| **Timer** | Rotate for a set duration, then auto-stop |
| **Programs** | Save & recall custom welding sequences |

## 📜 License

This project is provided as-is for personal and educational use.

---

*Built with ❤️ for pipe welders who demand precision.*
