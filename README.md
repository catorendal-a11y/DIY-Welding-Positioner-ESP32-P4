# DIY Welding Positioner Controller

Touchscreen controller for a single-axis TIG/MIG welding rotator.

**Built for the GUITION JC4880P443C 4.3" touch display board — ESP32-P4, 800×480 landscape, ST7701S MIPI-DSI and GT911 capacitive touch.**

[![Build](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml/badge.svg)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml)
[![Release](https://img.shields.io/github/v/release/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4)](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

![Main screen — actual LVGL simulator capture](docs/images/ui_runtime_v5/01_MAIN.png)

**[Try the Windows simulator](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/download/v2.2.0/welding-positioner-v2.2.0-simulator-windows-x64.zip)** · **[Firmware downloads](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/tag/v2.2.0)** · **[Builder guide](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/wiki)** · **[Watch the hardware demo](https://youtu.be/GygLl6XY-TM)**

The image shows the current V5 interface with example data. The hardware demo shows an earlier UI on the existing machine.

## The display board

**GUITION JC4880P443C 4.3" touch display board** is the project's supported hardware target. The display, touch and expansion-header wiring are configured for this board; another ESP32-P4 display board may need different drivers and pins.

<p>
  <img src="docs/images/board_overview.jpg" width="320" alt="GUITION JC4880P443C 4.3-inch touch display board, front and rear">
  <img src="docs/images/pinout.jpg" width="360" alt="GUITION JC4880P443C expansion header pin definitions">
</p>

| Board detail | Project configuration |
|---|---|
| Controller | ESP32-P4; ESP32-C6 is also fitted on the board |
| Display | 4.3-inch ST7701S, 480×800 physical panel rotated to 800×480 |
| Touch | GT911 capacitive touch, shared GPIO7/8 I2C bus |
| Firmware | Arduino / ESP-IDF, LVGL 9.6.0, FastAccelStepper 1.4.0 |
| Communication | USB-C serial, optional gated USB live mirror |

Board illustration and pin reference above. The product illustration shows vendor software/features; this firmware uses the ESP32-P4 side and has no Wi-Fi/BLE control. Use the [hardware guide](docs/HARDWARE_SETUP.md) for actual project wiring and reserved pins.

## What it does

- **Continuous, Jog, Pulse and Step** rotation, with a configurable countdown before starting.
- Workpiece speed within **0.001–3.0 RPM**, subject to geometry, calibration and the configured ceiling. The screen shows the effective minimum; for example, microstep 4 with a 300 mm part requires at least 0.004 RPM at the retained 20 Hz floor.
- Panel potentiometer, physical direction switch and optional foot pedal control.
- **16 saved programs**, with speed, direction, diameter and mode settings.
- Physical **E-STOP input**, driver-alarm monitoring and guarded reset.
- Guided **setup and calibration**, touch diagnostics and a USB-C live mirror.
- Optional **[idle screen saver](docs/SCREEN_SAVER.md)** in the V5 design, with touch-to-wake and a Display preview.

**v2.2.0** adds the idle screen saver, flat UI refinements and further motion/input/storage fixes, retaining LVGL 9.6 and FastAccelStepper 1.4. [Release notes](docs/releases/v2.2.0.md) · [Changelog](CHANGELOG.md).

## Try it on your PC

1. Download the **Windows x64 simulator ZIP** above.
2. Extract **all files** into one folder.
3. Open **Start Simulator.cmd** and use the mouse as the touchscreen.

No hardware or development tools are required. It runs the actual LVGL UI and production control code with simulated hardware. It cannot drive a real motor. [Simulator instructions](docs/releases/SIMULATOR.md).

## Build your controller

| Part | Project hardware |
|---|---|
| Display/controller | GUITION JC4880P443C, ESP32-P4, 4.3-inch 800×480 touch display |
| Motor/driver | NEMA 23 stepper, suitable PUL/DIR driver or DM542T |
| Drivetrain | NMRV030 60:1 worm stage + 72/40 spur stage, total 1:108 |
| Motor supply | 24–36 V DC, matched to the driver and motor |
| Controls | Speed potentiometer, direction switch, physical E-STOP |
| Optional | Foot pedal with switch/potentiometer and ADS1115 ADC |
| Enclosure | Grounded metal enclosure for TIG HF use |

<p>
  <img src="docs/images/stepper_setup.png" width="400" alt="NEMA 23 stepper, worm gearbox and PUL/DIR driver">
  <img src="docs/images/motor.worm.svg" width="400" alt="Worm and spur drivetrain reference">
</p>

Motor/gearbox and drivetrain references. Match the driver's current and microstep switches to your motor and touchscreen settings; the firmware supports 1/4, 1/8, 1/16 and 1/32 microstepping.

**Before powering the motor:** verify STEP/DIR/ENA wiring, E-STOP input polarity and driver configuration using the [hardware guide](docs/HARDWARE_SETUP.md). Firmware expects **GPIO34 HIGH when healthy, LOW on fault**, and **ENA HIGH to disable**. A bare NC contact to ground does not meet that input contract.

For HF-start TIG welding, put the controller, driver and motor PSU inside the same grounded metal enclosure. [EMI installation guide](docs/EMI_MITIGATION.md).

## Wiring Diagram

[![Wiring diagram in the V5 UI design](docs/images/Wiring_diagram.v2.svg?rev=2.6)](docs/images/Wiring_diagram.v2.svg)

The original project diagram now uses the V5 graphite/orange design, with corrected pin references, DIP notes, RPM limits and input requirements. Follow the [electrical wiring guide](docs/HARDWARE_SETUP.md) for driver terminal selection, logic interface and ground connections. **ESP32 GPIO is 3.3 V logic; never connect 5 V or motor-supply voltage to it.**

The NC E-STOP button is retained in the diagram. Its dashed interface block specifies the **HIGH-healthy / LOW-fault** input required by the existing firmware; a bare NC contact to GND produces the opposite polarity. The driver routing shown is common-cathode. Dashed blocks specify interface requirements, not verified installed circuitry. Confirm the actual interface and ENA polarity on the assembled controller; physical stop time remains unmeasured. [Input truth table and measurement procedure](docs/estop_timing.md).

## TIG HF Noise Requirement

For **HF-start TIG welding**, put the **ESP32-P4 display/controller, stepper driver and motor PSU inside the same grounded metal enclosure**. This is part of the project's installation requirements.

- Bond the enclosure to protective earth using a suitable connection.
- Separate motor/power wiring from GPIO, ADC, I2C, pedal and E-STOP cables.
- Use shielded external cables and enclosure-side shield termination where appropriate; add ferrites if interference remains.
- Repeat HF-start checks after changing firmware, wiring, cable routing or the enclosure.

Earlier hardware testing worked during TIG welding after the shared grounded enclosure was installed. This is historical field experience; it does not qualify v2.2.0 or measure its physical stop response. [EMI installation and test details](docs/EMI_MITIGATION.md).


## Install firmware

Choose a **release**, **debug** or **mirror** flashing bundle from Downloads. Follow [FLASHING.md](docs/releases/FLASHING.md), including first-install versus application-update instructions. Application images belong at **0x10000**, never zero. Check the supplied SHA-256 checksums.

For a source build, install PlatformIO and use:

```sh
git clone https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4.git
cd DIY-Welding-Positioner-ESP32-P4
git checkout v2.2.0
pio run
```

The flashing guide covers upload, serial ports and variant selection. On first use, follow **Setup Wizard**; existing valid settings are retained. [Getting started](wiki/Getting-Started.md) · [Setup instructions](docs/SETUP_WIZARD.md).

## Setup Wizard

New installations open **Setup Wizard** after boot. Existing valid settings and programs are retained; run it again from **Settings → Setup Wizard**.

1. **Motor:** match driver timing and microsteps to the physical driver, set limits, then **SAVE & APPLY**.
2. **Direction:** hold CW/CCW, observe the workpiece, flip direction if needed and confirm it while idle.
3. **Calibration:** align a mark, complete **MOVE 360**, measure, apply the correction, complete **VERIFY 360**, measure again and save only after verification passes.
4. **Function check:** press/release the physical E-STOP, explicitly reset to idle, test START and normal STOP, then confirm the observed physical behavior.

![Setup Wizard — actual LVGL motor stage](docs/images/setup_v1/01_motor.png)

Completion waits for its settings-save receipt. It does not start motion, measure physical stop time or certify wiring. [Every wizard screen](docs/SETUP_WIZARD.md) · [Calibration guide](docs/CALIBRATION_WORKFLOW.md).

## Using the touchscreen

Set speed and direction while idle, choose a mode or saved program, then press **START**. **STOP** requests normal deceleration. The physical E-STOP input inhibits ENA and latches a fault; clearing/resetting a fault never starts rotation.

| Mode | Typical use |
|---|---|
| Continuous | Rotate at the selected workpiece RPM |
| Jog | Hold to move while setting up |
| Pulse | Rotate, pause and repeat for tack cycles |
| Step | Rotate a chosen angle, then stop/dwell/repeat |
| Countdown | Delay the start with a visual countdown |

Calibration guides you through **Align → Measure → Verify → Save**. The correction stays temporary until a completed verification meets tolerance and its save succeeds. New Program separates run mode, available modes, exact RPM and mode settings.

<p>
  <img src="docs/images/program_v2/01_new_program.png" width="400" alt="Actual LVGL New Program screen">
  <img src="docs/images/calibration_v2/01_align.png" width="400" alt="Actual LVGL guided calibration screen">
</p>

[Calibration screens](docs/CALIBRATION_WORKFLOW.md) · [Program editor screens](docs/PROGRAM_EDITOR.md) · [USB mirror](simulator/README.md#usb-c-live-mirror).

## Validation and limitations

v2.2.0 includes cancelled-countdown guards, motion bounds/timeouts, pedal takeover, program direction and verified saves. [Logic review](docs/CODE_LOGIC_REVIEW_2026-10-03.md). [LVGL 9.6 UI refinements](docs/LVGL_9_6_UI_IMPROVEMENTS.md) improve text placement, status updates and exact motor-settings entry while preserving the flat design.

Validation covers **459 native/control/speed/storage tests**, packaging regressions, upstream RMT encoder checks, Linux/Windows simulator checks, a zero-failure layout audit and three firmware builds. Release binaries come from the exact validated master CI commit. [Release validation](docs/releases/v2.2.0.md#validation-and-hardware-status).

- **Single axis; no encoder feedback.** RPM/progress are calculated, so calibration requires real measurement.
- The owner confirms the assembled controller functions work. **Pedal/ADS1115 is unfinished and deferred.** This is functional feedback; no measured stop-time or HF qualification is claimed.
- Earlier TIG field experience supports the grounded-enclosure requirement; it does not qualify the latest firmware or establish a physical stop-time guarantee.

## Documentation and contributions

| Need | Guide |
|---|---|
| Complete builder reference | [Full guide, pin tables, configuration, troubleshooting and diagrams](docs/BUILDER_GUIDE.md) |
| Wiring, components and driver setup | [Hardware](docs/HARDWARE_SETUP.md) |
| Installation and first run | [Flashing](docs/releases/FLASHING.md) · [Getting started](wiki/Getting-Started.md) |
| A problem with your build | [Troubleshooting](wiki/Troubleshooting.md) |
| Motion, RTOS and storage design | [Control architecture](docs/CONTROL_SETUP_IMPLEMENTATION.md) · [Process flow](PROCESS_ANALYSIS.md) |
| Dependency versions and migration | [Library report](docs/DEPENDENCY_UPGRADE_2026-10-02.md) · [FastAccelStepper audit](docs/FASTACCELSTEPPER_1_4_REAUDIT.md) |
| Contribute a fix or report an issue | [Contributing](CONTRIBUTING.md) · [Issues](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/issues) |

Built one? Share your controller photos, driver/motor configuration and firmware version in an issue. That helps other builders reproduce the setup.

**MIT licensed.** See [LICENSE](LICENSE); dependencies and fonts retain their own notices.
