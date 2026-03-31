# Hardware Setup

## Bill of Materials

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| **MCU Board** | GUITION JC4880P443C (480x800) | 1 | ESP32-P4, MIPI-DSI, GT911 touch |
| **Stepper Driver** | TB6600 | 1 | DM542T upgrade planned |
| **Stepper Motor** | NEMA 23 (3 Nm) | 1 | |
| **Gearbox** | 200:1 Worm Gear | 1 | |
| **Power Supply** | 24V DC, >=5A | 1 | Dedicated for stepper driver |
| **Potentiometer** | 10k linear (LA42DWQ-22) | 1 | Speed control, IP65 |
| **E-STOP Button** | NC (Normally Closed) | 1 | |
| **DC-DC Converter** | 24V -> 5V 2A | 1 | If not using USB-C for ESP32 |

## TB6600 DIP Switch Configuration

Set microstepping to match the Settings screen on the device:

| Setting | Value |
|---------|-------|
| Microsteps | Must match UI Settings > Microstepping |
| Current | Match your NEMA 23 rating (typically 2.5-3.0A) |

Default firmware uses 1/8 microstepping. Other options: 1/4, 1/16, 1/32.

## Wiring Connections

### ESP32-P4 to TB6600 (Signal Wires)

The TB6600 uses optically isolated inputs. Wire in **Common Ground** configuration:

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

### E-STOP Button (NC)

| Button Pin | Connect To |
|------------|------------|
| Pin 1 | GPIO 33 |
| Pin 2 | GND |

**CRITICAL:** Add an external 10k pull-up resistor between 3.3V and GPIO 33. The NC button holds GPIO 33 LOW. When pressed, the circuit breaks, the pull-up pulls it HIGH, and the ISR fires instantly.

### Stepper Motor to TB6600

| Motor Wire | TB6600 Terminal |
|-------------|-----------------|
| A+ | A+ |
| A- | A- |
| B+ | B+ |
| B- | B- |

**WARNING:** Never connect or disconnect motor coils while the driver is powered. This will destroy the driver.

## Power Sequencing

1. Power ESP32-P4 first (USB-C or DC-DC)
2. Then power the 24V motor supply
3. Firmware holds ENA HIGH (motor disabled) on boot — safe by default

## Validated Hardware

| Component | Status |
|-----------|--------|
| GUITION JC4880P443C | Tested |
| TB6600 (1/4 and 1/8 microstepping) | Tested |
| NEMA 23 (3 Nm) | Tested |
| 200:1 Worm Gear | Tested |
| 24V DC PSU | Tested |
| 10k Pot (LA42DWQ-22) | Tested |
| NC E-STOP Button | Tested |

### Known TB6600 Limitations

- Motor resonance at 100-300 motor RPM causes stalling at coarse microstepping (1/4)
- Use 1/8 or finer for stable operation
- DM542T upgrade planned for built-in anti-resonance DSP
