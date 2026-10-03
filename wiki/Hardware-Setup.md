# Hardware Setup

## Bill of Materials

| Component | Model | Qty | Notes |
|-----------|-------|-----|-------|
| **MCU Board** | GUITION JC4880P443C 4.3" touch display board | 1 | ESP32-P4 + ESP32-C6, 800×480 landscape, MIPI-DSI, GT911 touch |
| **Stepper Driver** | PUL/DIR or DM542T | 1 | Motor Config: Standard vs DM542T |
| **Stepper Motor** | NEMA 23 (3 Nm) | 1 | |
| **Gearbox** | NMRV030 + spur | 1 | **1:108** total (60:1 x 72/40) |
| **Power Supply** | 24–36V DC, >=5A (**36V optimal**, **24V** works) | 1 | Dedicated for stepper driver |
| **Potentiometer** | 10k linear (LA42DWQ-22) | 1 | Speed control, IP65 |
| **E-STOP Button** | NC (Normally Closed) | 1 | Mushroom button |
| **Direction Switch** | SPDT toggle | 1 | CW/CCW |
| **Foot Pedal** | Analog pot + momentary switch | 1 | Optional |
| **DC-DC Converter** | 36V or 24V -> 5V 2A | 1 | If not using USB-C for ESP32 |
| **Metal enclosure** | Grounded aluminum/steel | 1 | Required for TIG HF-start welding |

## TIG HF Enclosure Requirement

Earlier hardware was exercised during TIG welding with the ESP32-P4 screen, stepper driver and motor PSU inside the same grounded metal enclosure. This is historical field experience; it does not qualify v2.2.0 or establish its physical stop response.

Do not weld with open bench wiring near an HF-start TIG machine. HF noise can reset the ESP32-P4, freeze touch/I2C, or create false GPIO/ADC inputs.

- Bond the enclosure to protective earth / welding chassis ground.
- Keep motor wiring away from E-STOP, pedal, pot, STEP/DIR/ENA, and I2C wiring.
- Use shielded external cables where practical.
- Terminate shields at the controller enclosure side.
- Add ferrites if HF start still causes resets or glitches.

## DIP stepper driver configuration

Set microstepping to match the Settings screen on the device:

| Setting | Value |
|---------|-------|
| Microsteps | Must match UI Settings > Microstepping |
| Current | Match your NEMA 23 rating (typically 2.5-3.0A) |

Default firmware uses **1/16** (3200 pulses/rev with a 200-step motor). Motor Config also offers 1/4, 1/8 and 1/32. Driver current is set by SW1–SW3 and microstep by SW5–SW8; follow the actual driver label.

## Wiring Connections

[![Reviewed wiring diagram in the V5 UI design](https://raw.githubusercontent.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/master/docs/images/Wiring_diagram.v2.svg?rev=2.6)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/images/Wiring_diagram.v2.svg)

Revision 2.6 retains the original hardware layout, corrects pedal endpoints and pin labels, and uses the V5 graphite/orange design. Dashed blocks specify interface functions, not verified installed components. Open the SVG at full size for terminal details. See the [complete electrical guide](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/HARDWARE_SETUP.md) and [diagram review](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/WIRING_DIAGRAM_REVIEW.md).

### ESP32-P4 to driver (signal wires)

Most PUL/DIR boards use optically isolated inputs. The diagram retains common-cathode routing; verify signal voltage/current and **ENA HIGH inhibits** behavior for the actual driver. Use a suitable 3.3 V-to-driver interface where required. ESP32 GPIO is not 5 V tolerant.

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

Wire the conditioned interface so **healthy = HIGH** and **pressed / fault = LOW** at GPIO34. A bare NC contact to GND gives the opposite logic. Verify cable-break and supply-loss behavior, plus ENA HIGH disable, before motor power. See the [repository hardware guide](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/HARDWARE_SETUP.md) and [E-STOP input truth table and measurement procedure](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/estop_timing.md).

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

GPIO 28, GPIO 14-19, and GPIO 54 may be PCB-routed toward the ESP32-C6 co-processor. Do **not** repurpose without the vendor schematic. GPIO 32 is intentionally used by this firmware for DM542T ALM; do not reuse it for other signals.

## Power Sequencing

1. Power ESP32-P4 first (USB-C or DC-DC)
2. Then power the motor supply (24V or 36V on driver VM)
3. Firmware requests inhibit with ENA HIGH on boot; verify that the actual interface establishes the driver-disabled state, including when controller power is absent.

## Historical hardware experience

| Component | Status |
|-----------|--------|
| GUITION JC4880P443C | Tested |
| PUL/DIR driver (1/4 and 1/8 microstepping) | Tested |
| NEMA 23 (3 Nm) | Tested |
| NMRV030 + spur (**1:108** total) | Tested |
| 24–36V DC PSU (36V optimal) | Tested |
| 10k Pot (LA42DWQ-22) | Tested |
| E-STOP circuit | Earlier build experience; current input contract requires physical verification |
| Direction switch (GPIO 29) | Tested |
| Foot pedal | Earlier build experience; new failure/release behavior needs bench checks |

### Known limitations (basic PUL/DIR drivers)

- Motor resonance at 100-300 motor RPM causes stalling at coarse microstepping (1/4)
- Use 1/8 or finer for stable operation
- DM542T is supported; set Motor Config -> Driver to DM542T and match the DIP microstep to the UI

## Owner build status - 2026-10-03

On 2026-10-03 the owner confirmed that all currently assembled controller functions work. Pedal/ADS1115 wiring and commissioning are unfinished and intentionally deferred. This is owner-reported functional confirmation; no quantitative stop-time, calibration-accuracy, rendering or HF test measurements were supplied.
