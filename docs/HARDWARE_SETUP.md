# TIG Welding Positioner — Hardware Setup Guide

This guide details the physical electrical wiring and structural assembly required to build the welding positioner safely.

## 1. Electrical Component Selection
- **ESP32-P4 Dev Board:** Must be the 4.3" Waveshare/Guition variant with MIPI-DSI interface.
- **Micro-Stepper Driver:** TB6600 (or equivalent like DM542). Must be capable of handling 24V-36V and 3A-4A output for NEMA 23.
- **Motor:** NEMA 23 Stepper Motor (approx 3.0 Nm holding torque recommended for welding tables).
- **Power Supply (PSU):** 24V DC, minimum 5.0A (120W+). Dedicated for the stepper driver.
- **DC-DC Converter:** Step-down buck converter (24V -> 5V 2A) to power the ESP32-P4 if not using USB-C.
- **Enclosure:** **Metallic (Aluminum/Steel) grounded enclosure**. Essential for EMI mitigation.

## 2. TB6600 Driver Configuration
Before powering on, configure the DIP switches on the side of your TB6600.

| Setting | Value | Required for firmware |
|---------|-------|-----------------------|
| **Microsteps** | 8 (1600 pulse/rev) | Matches `MOTOR_MICROSTEPS 8` in `config.h` |
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
  - Pin 1 -> `GPIO 33`
  - Pin 2 -> GND
  - **CRITICAL:** Add an external 10k pull-up resistor between 3.3V and `GPIO 33`.

_Note: The NC E-STOP button connects `GPIO 33` to GND constantly. When pressed, the circuit breaks, the external pull-up resistor pulls `GPIO 33` `HIGH`, and the hardware interrupt fires instantly._

## 4. Safety & Thermal Management
- **Heat Sinking:** Stepper drivers get hot during long welding runs. Mount the TB6600 to the metal enclosure for thermal dissipation or add a 40mm fan.
- **Power Sequencing:** Always power the logic (USB-C or DC-DC) first, then the motor 24V supply. The firmware holds the motor ENA pin HIGH (Disabled) on boot for safety.
