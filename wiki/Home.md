# DIY Welding Positioner Controller

**ESP32-P4 / C6 GUITION JC4880P443C** | Open-source firmware for TIG/MIG welding rotators

## Current Status: v0.4.0 — Hardware Validated

All core features have been tested and confirmed working on real hardware:

- Motor rotation with live RPM adjustment (pot + buttons)
- All 5 welding modes (Continuous, Jog, Pulse, Step, Timer)
- Hardware E-STOP with <0.5ms response
- Program preset save/load (LittleFS)
- Touch UI on 4.3" MIPI-DSI display

## Quick Links

| Page | Description |
|------|-------------|
| [[Getting Started]] | Build, flash, and first run |
| [[Architecture]] | FreeRTOS dual-core design, state machine, module breakdown |
| [[Hardware Setup]] | Wiring diagram, TB6600 config, BOM |
| [[Troubleshooting]] | Common issues and solutions |
| [[Roadmap]] | Planned features and future direction |

## Project Overview

Open-source welding positioner controller for rotary welding tables, pipe welding rotators, and automated fabrication systems. Controls a NEMA 23 stepper motor through a 200:1 worm gear with precise RPM control from 0.05 to 5.0 RPM.

### Key Specs

| Parameter | Value |
|-----------|-------|
| MCU | ESP32-P4 (360MHz, dual-core RISC-V) |
| Display | GUITION JC4880P443C, 480x800, MIPI-DSI ST7701S |
| Touch | GT911 capacitive |
| UI Framework | LVGL 9.5.0 |
| Motor Driver | FastAccelStepper 0.33.14 (RMT hardware pulses) |
| Gear Ratio | 200:1 worm gear |
| Microstepping | 1/4, 1/8, 1/16, 1/32 (selectable) |
| RPM Range | 0.05 - 5.0 (current max 1.0 with TB6600) |
| Storage | LittleFS + ArduinoJson (16 presets) |

## Repository Structure

```
src/
  main.cpp              - FreeRTOS task setup
  config.h              - All hardware pins and motor parameters
  motor/                - FastAccelStepper wrapper, speed control, microstepping
  control/              - State machine, 5 welding modes
  safety/               - E-STOP ISR + watchdog
  storage/              - LittleFS preset/settings persistence
  ui/                   - LVGL screens, display init, touch, theme
docs/
  images/               - SVG wiring diagrams, UI mockups
  HARDWARE_SETUP.md     - Detailed wiring guide
  SAFETY_SYSTEM.md      - Safety architecture
  EMI_MITIGATION.md     - EMI shielding guide
```
