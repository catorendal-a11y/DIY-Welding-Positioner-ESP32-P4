# DIY Welding Positioner Controller

**ESP32-P4 / C6 GUITION JC4880P443C** | Open-source firmware for TIG/MIG welding rotators

## Current Status: v2.0.6 — diagnostics and preset polish

Release **v2.0.6** adds the Diagnostics event log, per-preset workpiece diameter for STEP programs, Pedal Settings, Diagnostics, ProgramExecutor polish, start-request race fixes and documentation sync. Release **v2.0.5** previously centralised cross-core atomics, added `fatal_halt()`, release-visible `LOG_E`, non-blocking ADS1115 reads and boot-time ESTOP de-floating. See `CHANGELOG.md` for the full list.

All core features tested and confirmed working on real hardware:

- Motor rotation with live RPM adjustment (potentiometer + foot pedal; main screen has no RPM +/-)
- All 5 welding modes (Continuous, Jog, Pulse, Step, Timer)
- Hardware E-STOP with <0.5ms response + UI reset
- 16 program preset save/load (NVS JSON blobs; legacy LittleFS migration on first boot if needed)
- 21 active LVGL root screens (`ScreenId`) plus E-STOP overlay; settings, diagnostics, pedal settings, system info, calibration
- Direction switch (GPIO29), foot pedal support
- 8 accent color themes, **dark or light UI mode** (Display **UI MODE**), brightness control, dim timeout

## Quick Links

| Page | Description |
|------|-------------|
| [[Getting Started]] | Build, flash, and first run |
| [[Architecture]] | FreeRTOS dual-core design, state machine, thread safety |
| [[Hardware Setup]] | Wiring diagram, driver config, BOM |
| [[Troubleshooting]] | Common issues and solutions |
| [[Roadmap]] | Completed features and future plans |

## Project Overview

Open-source welding positioner controller for rotary welding tables, pipe welding rotators, and automated fabrication systems. Controls a NEMA 23 stepper motor through a **1:108** drivetrain (NMRV030 + spur) with precise RPM control (**0.001–3.0 RPM** workpiece absolute limits in `config.h`; Motor Config stores a lower **max RPM** ceiling in NVS if desired).

### Key Specs

| Parameter | Value |
|-----------|-------|
| MCU | ESP32-P4 (360MHz, dual-core RISC-V) |
| Display | GUITION JC4880P443C, 800x480 landscape, MIPI-DSI ST7701S |
| Touch | GT911 capacitive |
| UI Framework | LVGL 9.5.0 |
| Motor Driver | FastAccelStepper 0.33.x (RMT hardware pulses) |
| Gear Ratio | **1:108** total (60 x 72/40, NMRV030 + spur) |
| Microstepping | 1/4, 1/8, 1/16, 1/32 (selectable) |
| RPM Range | 0.001 – 3.0 RPM (`MIN_RPM`/`MAX_RPM`); UI max ≤ cap via Motor Config (NVS). Roadmap: higher limits with DM542T |
| Storage | NVS (`Preferences`) + ArduinoJson blobs `cfg` / `prs` (16 presets, settings); one-time LittleFS import |
| Cross-core | `std::atomic` with explicit memory ordering; single source of truth in `src/app_state.h` |

## Repository Structure

```
src/
  main.cpp              - FreeRTOS task setup
  config.h              - All hardware pins and motor parameters
  motor/                - FastAccelStepper wrapper, speed control, microstepping
  control/              - State machine, 5 welding modes
  safety/               - E-STOP ISR + watchdog
  storage/              - NVS persistence (+ legacy LittleFS migration)
  ui/                   - LVGL screens, display init, touch, theme
docs/
  images/               - SVG wiring diagrams, UI mockups
wiki/                   - GitHub Wiki pages
```
