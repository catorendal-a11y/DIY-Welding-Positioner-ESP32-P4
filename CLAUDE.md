# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ALWAYS load `.claude/SKILL.md` first

Before writing ANY LVGL code, always read and follow `.claude/SKILL.md`. It contains the authoritative rules for LVGL 9.x on this project — API names, rotation constraints, font limits, style rules, coordinate bounds, and the pre-commit checklist. Violating these rules causes crashes or silent rendering failures.

## Build Commands

```bash
# Build (release - silent, max performance)
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run

# Build debug
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-debug

# Flash
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run --target upload

# Serial monitor
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" device monitor
```

Note: System Python is 3.14 (incompatible with PlatformIO). Always use the bundled pio.exe from `.platformio\penv\Scripts\`.

## Architecture

TIG welding rotator controller on ESP32-P4 with dual-core FreeRTOS.

### Core Assignment

| Core 0 (Real-time) | Core 1 (UI) |
|---|---|
| safetyTask (pri 5, 2KB stack) | lvglTask (pri 1, **64KB stack**) |
| motorTask (pri 4, 5KB stack) | storageTask (pri 1, 4KB stack) |
| controlTask (pri 3, 4KB stack) | |

All `lv_*` calls must come from the lvglTask on Core 1 only.

### Module Responsibilities

- **src/control/** — State machine with modes: continuous, jog, pulse, step, timer. `control.cpp` dispatches to mode implementations in `modes/`.
- **src/motor/** — FastAccelStepper wrapper. TB6600 driver, 200:1 worm gear, selectable microstepping (1/4 to 1/32, default 1/8 = 1600 steps/rev).
- **src/safety/** — Two-layer ESTOP: ISR (<0.5ms hardware disable) + task (state transition, UI feedback).
- **src/storage/** — LittleFS + ArduinoJson. 16 presets max, settings persistence.
- **src/ui/** — MIPI-DSI display init (`display.cpp`), LVGL HAL (`lvgl_hal.cpp`), screen management (`screens.cpp`), 12+ screen files in `screens/`.

### Display Pipeline

Physical panel is 480×800 portrait. LVGL renders at 800×480 landscape with **manual 90° CW rotation** in the flush callback:

```
LVGL (800×480) → flush_cb rotates pixels → DPI panel (480×800)
Rotation: landscape (lx, ly) → portrait (ly, 799-lx)
Touch inverse: portrait (px, py) → landscape (799-py, px)
```

Do NOT use `lv_display_set_rotation()` — it causes Load access fault on ESP32-P4.

### State Machine

`SystemState` enum: IDLE → RUNNING/PULSE/STEP/JOG/TIMER → STOPPING → IDLE. ESTOP interrupts any state. Transitions happen in `controlTask` (10ms cycle).

## Critical Constraints

- **montserrat_48 font crashes ESP32-P4** — use montserrat_40 maximum
- **LVGL rotation crashes** — manual rotation in flush callback only
- **LVGL task needs 64KB stack** — arc/scale rendering is stack-heavy
- **PSRAM for LVGL buffers** — internal RAM causes overflow with rotation buffer
- **ENA pin HIGH = motor disabled** — safety-critical, never drive LOW without ESTOP check
- **ASCII only in labels** — Montserrat fonts cover 0x20-0x7E only. No −, →, °, ±, ×, …

## LVGL 9 API

This project uses LVGL 9.6.0-dev. Use v9 names, not v8 compat aliases:

| v8 alias (avoid) | v9 name (correct) |
|---|---|
| `lv_btn_create` | `lv_button_create` |
| `lv_obj_clear_flag` | `lv_obj_remove_flag` |
| `lv_obj_del` | `lv_obj_delete` |
| `lv_scr_load` | `lv_screen_load` |
| `lv_scr_act` | `lv_screen_active` |

## Key Config

- `speed_apply()` must ONLY be called from Core 0 (motorTask) — never from UI callbacks.
- UI callbacks must use `speed_request_update()` to set a volatile flag; motorTask picks it up within 5ms.
- `applySpeedAcceleration()` must follow `setSpeedInMilliHz()` for live speed changes to take effect.
- Shared variables between cores (`sliderRPM`, `buttonsActive`, `lastPotAdc`) must be `volatile`.
- Pot ADC reference is 3315 (measured range of 10k pot with 11dB attenuation on ESP32-P4).
- Pot override threshold is 200 ADC counts (prevents false override from noise).

Hardware pins and motor parameters in `src/config.h`:
- Motor: STEP(50), DIR(51), ENA(52), ESTOP(33)
- Display: MIPI-DSI 2-lane, ST7701S, GT911 touch (I2C addr 0x5D)
- RPM range: 0.05–5.0, 1600 steps/rev, 200:1 gear ratio
