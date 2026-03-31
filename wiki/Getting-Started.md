# Getting Started

## Prerequisites

- **PlatformIO** Core 6.x or newer (VS Code extension recommended)
- **Python** 3.11 (PlatformIO's bundled Python — system Python 3.14 is incompatible)
- **GUITION JC4880P443C** ESP32-P4 4.3" Touch Display dev board
- **USB-C cable** for power and serial
- **Hardware:** TB6600 driver, NEMA 23 motor, 24V PSU, 10k pot, NC E-STOP button

See [[Hardware Setup]] for full wiring details.

## 1. Clone and Open

```bash
git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
cd DIY-Welding-Positioner-ESP32-P4
```

Open the folder in VS Code with the PlatformIO extension installed.

## 2. Build

```bash
# Release build (silent, max performance)
pio run

# Debug build (verbose logging)
pio run -e esp32p4-debug
```

## 3. Flash

```bash
pio run --target upload
```

## 4. Serial Monitor

```bash
pio device monitor
```

Expected boot output:
```
[I] BOOT OK — ENA=HIGH (motor disabled)
[I] TIG Rotator Controller v2.0
[I] Hardware: ESP32-P4 4.3" Touch Display (Waveshare/Guition)
[I] Safety init: ESTOP=OK
[I] Speed control init: pot=3327 (pin=49)
[I] Display init: ESP32-P4 MIPI-DSI ST7701S 800x480
[I] ST7701S MIPI-DSI init complete!
[I] GT911 touch OK!
[I] FastAccelStepper init OK
[I] System ready — ESP32-P4 + MIPI-DSI display
```

## 5. First Run Checklist

- [ ] Display boots and shows main screen
- [ ] Touch responds to taps
- [ ] Press START — motor rotates at set RPM
- [ ] Turn potentiometer — RPM changes during rotation
- [ ] Press +/- buttons — RPM changes during rotation
- [ ] Press STOP — motor decelerates and stops
- [ ] Press E-STOP — motor halts instantly
- [ ] Try each mode: JOG, PULSE, STEP, TIMER

## Configuration

All parameters are in `src/config.h`:

```cpp
#define MIN_RPM         0.05f     // Minimum workpiece RPM
#define MAX_RPM         1.0f      // Maximum workpiece RPM (temporary)
#define GEAR_RATIO      200       // Worm gear ratio
#define MICROSTEPS      8         // Default microstepping
#define ACCELERATION    7000      // Steps/s^2
```

**Microstepping** can also be changed from the Settings screen on the device. Remember to set the TB6600 DIP switches to match.

## Wiring Summary

| ESP32 Pin | Function | Connect To |
|-----------|----------|------------|
| GPIO 50 | STEP (Output) | TB6600 PUL+ |
| GPIO 51 | DIR (Output) | TB6600 DIR+ |
| GPIO 52 | ENABLE (Output) | TB6600 EN+ |
| GPIO 49 | ADC (Input) | 10k Pot wiper |
| GPIO 33 | E-STOP (Input) | NC E-STOP button |
| GPIO 28 | DIR SW (Input) | CW/CCW toggle switch |

All TB6600 minus pins (PUL-, DIR-, EN-) connect to ESP32 GND.

See [[Hardware Setup]] for full details with diagrams.
