# TIG Welding Positioner — Hardware Setup Guide

This guide details the physical electrical wiring and structural assembly required to build the welding positioner safely.

## 1. Electrical Component Selection
- **ESP32-P4 Dev Board:** GUITION JC4880P443C (4.3" MIPI-DSI display, ESP32-P4 + ESP32-C6 co-processor).
- **Micro-Stepper Driver:** TB6600 (or equivalent like DM542). Must handle 24V-36V and 3A-4A output for NEMA 23.
- **Motor:** NEMA 23 Stepper Motor (approx 3.0 Nm holding torque recommended).
- **Power Supply (PSU):** 24V DC, minimum 5.0A (120W+). Dedicated for the stepper driver.
- **DC-DC Converter:** Step-down buck converter (24V -> 5V 2A) to power the ESP32-P4 if not using USB-C.
- **Enclosure:** **Metallic (Aluminum/Steel) grounded enclosure**. Essential for EMI mitigation.

## 2. TB6600 Driver Configuration
Before powering on, configure the DIP switches on the side of your TB6600.

| Setting | Value | Required for firmware |
|---------|-------|-----------------------|
| **Microsteps** | Selectable: 1/4, 1/8, 1/16, 1/32 | Must match Settings > Microstepping in UI |
| **Current** | Match your NEMA 23 rating | Typically 2.5A - 3.0A |

## 3. Wiring Connections

### A. Stepper Motor to TB6600
Identify your stepper motor pairs (use a multimeter on continuity mode if unsure).
- **A+ / A-:** Coil Pair 1
- **B+ / B-:** Coil Pair 2

### B. ESP32-P4 to TB6600 (Signal Wires)
The TB6600 uses optically isolated inputs. Wire them in a **Common Ground** configuration.

1. Connect **PUL-**, **DIR-**, and **ENA-** all together and wire them directly to a **GND** pin on the ESP32-P4.
2. Wire **PUL+ (Step)** to `GPIO 50`.
3. Wire **DIR+ (Direction)** to `GPIO 51`.
4. Wire **ENA+ (Enable)** to `GPIO 52`.

### C. External Controls
- **Speed Potentiometer (10k Linear):**
  - Pin 1 (CCW) -> GND
  - Pin 2 (Wiper) -> `GPIO 49`
  - Pin 3 (CW) -> 3.3V
- **E-STOP (Normally Closed Button):**
  - Pin 1 -> `GPIO 34`
  - Pin 2 -> GND
  - Uses internal INPUT_PULLUP. NC contact holds pin LOW. Press breaks circuit, pin goes HIGH, ISR fires.
- **Direction Switch (CW/CCW Toggle):**
  - Pin 1 -> `GPIO 29`
  - Pin 2 -> GND
  - Uses internal INPUT_PULLUP. Toggle via Settings > Motor Config > Direction Switch.
- **Foot Pedal (Optional):**
  - Potentiometer wiper -> `GPIO 35` (ADC speed input)
  - Start switch -> `GPIO 33` (INPUT_PULLUP, LOW = pressed)

### D. Reserved Pins (Do Not Use)
- **GPIO 28** (C6_U0TXD) and **GPIO 32** (C6_U0RXD) are PCB-routed to the ESP32-C6 co-processor for WiFi/BLE via esp-hosted SDIO transport. They cannot be used as general-purpose I/O when WiFi or BLE is active.

## 4. Safety & Thermal Management
- **Heat Sinking:** Stepper drivers get hot during long welding runs. Mount the TB6600 to the metal enclosure for thermal dissipation or add a 40mm fan.
- **Power Sequencing:** Always power the logic (USB-C or DC-DC) first, then the motor 24V supply. The firmware holds the motor ENA pin HIGH (Disabled) on boot for safety.

## 5. Validated Hardware

| Component | Tested Model | Status |
|-----------|-------------|--------|
| **MCU Board** | GUITION JC4880P443C (ESP32-P4 + C6) | Tested |
| **Stepper Driver** | TB6600 | Tested (1/4 and 1/8 microstepping) |
| **Stepper Motor** | NEMA 23 (3 Nm) | Tested |
| **Gearbox** | Worm gear (~200:1) | Tested |
| **Power Supply** | 24V DC | Tested |
| **Potentiometer** | 10k (LA42DWQ-22) | Tested (ADC range 0-3315) |
| **E-STOP** | NC Button | Tested |
| **Direction Switch** | SPDT toggle on GPIO 29 | Tested |
| **Foot Pedal** | Analog pot + momentary switch | Tested |

### Known Limitations with TB6600
- Motor resonance at 100-300 motor RPM causes stalling at coarse microstepping (1/4)
- Recommended to use 1/8 microstepping or finer for stable operation
- DM542T upgrade planned for anti-resonance DSP support and higher RPM
