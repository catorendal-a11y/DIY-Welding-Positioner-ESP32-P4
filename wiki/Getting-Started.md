# Getting Started

## Prerequisites

- **PlatformIO** Core 6.x or newer (VS Code extension recommended)
- **Python** PlatformIO bundled Python (system Python 3.14 is incompatible)
- **GUITION JC4880P443C** ESP32-P4 4.3" Touch Display dev board
- **USB-C cable** for power and serial
- **Hardware:** Stepper driver, NEMA 23 motor, 24–36V motor PSU (**36V optimal**, **24V** works), 10k pot, NC E-STOP button

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

**Note:** If esptool crashes with Unicode encoding error on Windows, set `PYTHONUTF8=1`:
```bash
set PYTHONUTF8=1 && pio run --target upload
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
- [ ] Turn pot on main screen — workpiece RPM changes during rotation (main has no +/-; use **Jog** screen +/- for jog speed if needed)
- [ ] Toggle direction switch — motor reverses
- [ ] Press STOP — motor decelerates and stops
- [ ] Press E-STOP — motor halts instantly, red overlay appears
- [ ] Reset E-STOP — tap overlay to dismiss
- [ ] Try each mode: JOG, PULSE, STEP, TIMER
- [ ] Navigate to Settings > Motor Config — enable direction switch
- [ ] Navigate to Settings > Display — adjust brightness

## Configuration

All parameters are in `src/config.h`:

```cpp
#define MIN_RPM         0.001f    // Minimum workpiece RPM (see src/config.h)
#define MAX_RPM         3.0f      // Absolute ceiling for max RPM / firmware clamp
#define GEAR_RATIO      (60.0f * 72.0f / 40.0f)   // = 108, total 1:108
#define START_SPEED     20        // Hz — minimum step-frequency floor (config.h)
// Default acceleration steps/s² is 7500 from factory SystemSettings (storage.cpp), editable in Motor Config (NVS)
```

Settings can also be changed from the touchscreen via **Settings > Motor Config** and are persisted to **NVS** (namespace `wrot`, key `cfg`; see repository README *Persistence (NVS)*).

## Wiring Summary

| ESP32 Pin | Function | Connect To |
|-----------|----------|------------|
| GPIO 50 | STEP (Output) | Driver PUL+ |
| GPIO 51 | DIR (Output) | Driver DIR+ |
| GPIO 52 | ENABLE (Output) | Driver ENA- |
| GPIO 49 | POT (ADC Input) | 10k Pot wiper |
| GPIO 29 | DIR SW (Input) | CW/CCW toggle |
| GPIO 34 | E-STOP (Input, ISR) | NC E-STOP button |
| GPIO 32 | DRIVER ALM (Input) | DM542T alarm, LOW = fault |
| GPIO 33 | PEDAL SW (Input) | Foot pedal switch (optional) |
| I2C (GPIO 7/8) | ADS1115 | Pedal pot ADC, addr 0x48-0x4B |

**Reserved:** GPIO 28, 14-19, and 54 may be routed to the ESP32-C6 on the PCB — do not use as application GPIO without the GUITION schematic. GPIO 32 is intentionally used by this firmware for DM542T alarm input. See `docs/HARDWARE_SETUP.md` §Reserved pins.

All driver minus pins (PUL-, DIR-, ENA-) connect to ESP32 GND.

See [[Hardware Setup]] for full details with diagrams.
