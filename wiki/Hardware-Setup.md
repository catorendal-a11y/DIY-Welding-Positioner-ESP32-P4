# Hardware Setup

## Bill of Materials

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| **MCU Board** | GUITION JC4880P443C (800x480) | 1 | ESP32-P4 + ESP32-C6, MIPI-DSI, GT911 touch |
| **Stepper Driver** | PUL/DIR or DM542T | 1 | Motor Config: Standard vs DM542T |
| **Stepper Motor** | NEMA 23 (3 Nm) | 1 | |
| **Gearbox** | NMRV030 + spur | 1 | **1:108** total (60:1 x 72/40) |
| **Power Supply** | 24–36V DC, >=5A (**36V optimal**, **24V** works) | 1 | Dedicated for stepper driver |
| **Potentiometer** | 10k linear (LA42DWQ-22) | 1 | Speed control, IP65 |
| **E-STOP Button** | NC (Normally Closed) | 1 | Mushroom button |
| **Direction Switch** | SPDT toggle | 1 | CW/CCW |
| **Foot Pedal** | Analog pot + momentary switch | 1 | Optional |
| **DC-DC Converter** | 36V or 24V -> 5V 2A | 1 | If not using USB-C for ESP32 |

## DIP stepper driver configuration

Set microstepping to match the Settings screen on the device:

| Setting | Value |
|---------|-------|
| Microsteps | Must match UI Settings > Microstepping |
| Current | Match your NEMA 23 rating (typically 2.5-3.0A) |

Default firmware uses **1/16** (3200 pulses/rev with DM542T). Motor Config also offers 1/8 and 1/32.

## Wiring Connections

### ESP32-P4 to driver (signal wires)

Most PUL/DIR boards use optically isolated inputs. Wire in **Common Ground** configuration:

1. Connect **PUL-**, **DIR-**, and **ENA-** together -> ESP32-P4 **GND**
2. **PUL+** (Step) -> **GPIO 50**
3. **DIR+** (Direction) -> **GPIO 51**
4. **ENA+** (Enable) -> **GPIO 52**

### Potentiometer

| Pot Pin | Connect To |
|---------|------------|
| Pin 1 (CCW) | GND |
| Pin 2 (Wiper) | GPIO 49 |
| Pin 3 (CW) | 3.3V |

Measured ADC range: 0-3315 (with 11dB attenuation on ESP32-P4).

### E-STOP (GPIO 34)

Wire so **released (safe) = HIGH** and **pressed / fault = LOW** on GPIO 34 (matches `safety.cpp`: `INPUT_PULLUP`, **FALLING** ISR). See repository `docs/HARDWARE_SETUP.md` for a full discussion and EMI options.

### Direction Switch (CW/CCW)

| Switch Pin | Connect To |
|------------|------------|
| Pin 1 | GPIO 29 |
| Pin 2 | GND |

Uses internal INPUT_PULLUP. Enable via Settings > Motor Config > Direction Switch.

### Foot Pedal (optional)

| Pedal Pin | Connect To |
|-----------|------------|
| Pot wiper | **ADS1115** AIN0 on touch I2C (SDA 7 / SCL 8) when `ENABLE_ADS1115_PEDAL` is enabled — not GPIO 35 |
| Start switch | GPIO 33 (`INPUT_PULLUP`, LOW = pressed) |

### Stepper motor to driver

| Motor Wire | Driver terminal |
|-------------|-----------------|
| A+ | A+ |
| A- | A- |
| B+ | B+ |
| B- | B- |

**WARNING:** Never connect or disconnect motor coils while the driver is powered. This will destroy the driver.

### Reserved Pins

GPIO 28 (C6_U0TXD) and GPIO 32 (C6_U0RXD) are PCB-routed to the ESP32-C6 co-processor. Do **not** repurpose without the vendor schematic.

## Power Sequencing

1. Power ESP32-P4 first (USB-C or DC-DC)
2. Then power the motor supply (24V or 36V on driver VM)
3. Firmware holds ENA HIGH (motor disabled) on boot — safe by default

## Validated Hardware

| Component | Status |
|-----------|--------|
| GUITION JC4880P443C | Tested |
| PUL/DIR driver (1/4 and 1/8 microstepping) | Tested |
| NEMA 23 (3 Nm) | Tested |
| NMRV030 + spur (**1:108** total) | Tested |
| 24–36V DC PSU (36V optimal) | Tested |
| 10k Pot (LA42DWQ-22) | Tested |
| NC E-STOP Button | Tested |
| Direction switch (GPIO 29) | Tested |
| Foot pedal | Tested |

### Known limitations (basic PUL/DIR drivers)

- Motor resonance at 100-300 motor RPM causes stalling at coarse microstepping (1/4)
- Use 1/8 or finer for stable operation
- DM542T upgrade planned for built-in anti-resonance DSP
