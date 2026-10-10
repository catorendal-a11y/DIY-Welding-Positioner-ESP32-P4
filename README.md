# DIY Welding Positioner Controller

Touchscreen controller for a single-axis TIG/MIG welding rotator.

**Built for the GUITION JC4880P443C 4.3" touch display board — ESP32-P4, 800×480 landscape, ST7701S MIPI-DSI and GT911 capacitive touch.**

![Main screen — actual LVGL simulator capture](docs/images/ui_runtime_v5/01_MAIN.png)

<p align="center">
  <a href="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml"><img src="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml/badge.svg" alt="Build and tests"></a>
  <a href="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/latest"><img src="https://img.shields.io/github/v/release/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4" alt="Latest release"></a>
  <a href="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/commits/master"><img src="https://img.shields.io/github/last-commit/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4" alt="Last commit"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-22c55e" alt="License: MIT"></a>
  <a href="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/stargazers"><img src="https://img.shields.io/github/stars/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4?style=flat" alt="GitHub stars"></a>
</p>

<p align="center">
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/MCU-ESP32--P4-ef4444" alt="MCU: ESP32-P4"></a>
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/board-GUITION%20JC4880P443C-f97316" alt="Board: GUITION JC4880P443C"></a>
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/display-4.3%E2%80%B3%20%C2%B7%20800%C3%97480-0ea5e9" alt="Display: 4.3-inch, 800 x 480"></a>
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/touch-GT911-8b5cf6" alt="Capacitive touch: GT911"></a>
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/panel-ST7701S%20%C2%B7%20MIPI--DSI-06b6d4" alt="Panel: ST7701S, MIPI-DSI"></a>
</p>

<p align="center">
  <a href="src"><img src="https://img.shields.io/badge/language-C%2B%2B-3b82f6" alt="Language: C++"></a>
  <a href="platformio.ini"><img src="https://img.shields.io/badge/framework-Arduino%20%2F%20ESP--IDF-14b8a6" alt="Framework: Arduino / ESP-IDF"></a>
  <a href="platformio.ini"><img src="https://img.shields.io/badge/LVGL-9.6.0-8b5cf6" alt="LVGL: 9.6.0"></a>
  <a href="platformio.ini"><img src="https://img.shields.io/badge/FastAccelStepper-1.4.0-f59e0b" alt="FastAccelStepper: 1.4.0"></a>
  <a href="platformio.ini"><img src="https://img.shields.io/badge/build-PlatformIO-f97316" alt="Build system: PlatformIO"></a>
</p>

<p align="center">
  <a href="#what-it-does"><img src="https://img.shields.io/badge/rotation-4%20modes-22c55e" alt="Rotation: Continuous, Jog, Pulse, Step"></a>
  <a href="#what-it-does"><img src="https://img.shields.io/badge/programs-16%20saved-10b981" alt="Programs: 16 saved"></a>
  <a href="#what-it-does"><img src="https://img.shields.io/badge/speed-0.001%E2%80%933.0%20RPM-0ea5e9" alt="Configured speed range: 0.001 to 3.0 RPM, subject to geometry and limits"></a>
  <a href="docs/HARDWARE_SETUP.md"><img src="https://img.shields.io/badge/foot%20pedal-optional-8b5cf6" alt="Optional foot pedal"></a>
  <a href="docs/estop_timing.md"><img src="https://img.shields.io/badge/E--STOP-monitored%20input-ef4444" alt="Monitored E-STOP input; see wiring and measurement requirements"></a>
</p>

<p align="center">
  <a href="docs/releases/SIMULATOR.md"><img src="https://img.shields.io/badge/simulator-Windows%20%2F%20Linux-3b82f6" alt="Simulator: Windows and Linux"></a>
  <a href="simulator/README.md#usb-c-live-mirror"><img src="https://img.shields.io/badge/live%20mirror-USB--C-06b6d4" alt="Optional USB-C live mirror"></a>
  <a href="src/storage/storage.cpp"><img src="https://img.shields.io/badge/settings-NVS-14b8a6" alt="Settings and programs: NVS storage"></a>
  <a href="https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/workflows/pio-build.yml"><img src="https://img.shields.io/badge/verification-native%20%2B%20simulator-22c55e" alt="Verification paths: native tests and LVGL simulator"></a>
</p>

<p align="center">
  <a href="#documentation-and-contributions"><img src="docs/images/support-project.svg" width="750" alt="Support this project to keep it open source"></a>
</p>

**[Try the Windows simulator](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/download/v2.2.1/welding-positioner-v2.2.1-simulator-windows-x64.zip)** · **[Firmware downloads](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/tag/v2.2.1)** · **[Builder guide](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/wiki)** · **[Watch the hardware demo](https://youtu.be/GygLl6XY-TM)**

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

**v2.2.1** makes the foot pedal work in every configuration: GPIO33 switch start/stop always, ADS1115 analog pedal speed when present, panel-potentiometer fallback otherwise. [Release notes](docs/releases/v2.2.1.md) · [Changelog](CHANGELOG.md).

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
git checkout v2.2.1
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
- The owner confirms the assembled controller functions work, including the foot pedal: on 2026-10-05 the ADS1115 answered the boot scan at 0x48 and GPIO33 switch + analog pedal speed were verified working on the assembled machine (master build with runtime pedal source selection, unreleased). This is functional feedback; no measured stop-time or HF qualification is claimed.
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
