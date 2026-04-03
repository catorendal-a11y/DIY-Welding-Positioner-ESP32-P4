# AGENTS.md

Instructions for agentic coding agents working in this repository.

## Tool Usage

Always use Context7 MCP when needing library/API documentation, code generation, setup or configuration steps — without being explicitly asked.

## Project Overview

TIG welding rotator controller on ESP32-P4 with dual-core FreeRTOS, MIPI-DSI display (800x480 landscape), LVGL 9.6 UI, and FastAccelStepper motor control.

## Build Commands

System Python is 3.14 (incompatible with PlatformIO). Always use bundled `pio.exe`:

```bash
# Build release (default, silent)
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run

# Build debug (verbose serial logging)
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-debug

# Flash to device
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run --target upload

# Serial monitor
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" device monitor
```

There is no linter or formatter configured. There are no single-test commands — tests use PlatformIO native:

```bash
# Run all native tests (Unity framework)
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" test -e native
```

## LVGL Code Rules

Before writing ANY LVGL code, read `.claude/SKILL.md` (971 lines of correctness rules). Key points:

- Use LVGL 9 API names only (e.g., `lv_button_create`, not `lv_btn_create`; `lv_obj_delete`, not `lv_obj_del`; `lv_screen_load`, not `lv_scr_load`)
- All `lv_*` calls must come from `lvglTask` on Core 1 only
- `lv_style_t` must be static/global — never stack-allocated
- Canvas is 800x480 landscape; coordinates must satisfy `x + width <= 800`, `y + height <= 480`
- ASCII only in labels (0x20-0x7E). No Unicode minus, arrows, degree sign
- Font max is montserrat_40 (montserrat_48 crashes ESP32-P4)
- Do NOT use `lv_display_set_rotation()` — manual rotation in flush callback only
- `lv_display_flush_ready()` must be called exactly once per flush

## Architecture

| Core 0 (Real-time) | Core 1 (UI) |
|---|---|
| safetyTask (pri 5, 2KB) | lvglTask (pri 1, **64KB**) |
| motorTask (pri 4, 5KB) | storageTask (pri 1, 4KB) |
| controlTask (pri 3, 4KB) | |

Module directories: `src/control/` (state machine), `src/motor/` (stepper driver), `src/safety/` (ESTOP), `src/storage/` (LittleFS + presets), `src/ui/` (display, LVGL, screens), `src/ble/` (BLE remote).

## Code Style

### Formatting
- **Indentation:** 2 spaces (no tabs)
- **Braces:** K&R — opening brace on same line as statement
- **Header guards:** `#pragma once` exclusively
- **Line length:** No strict limit, but keep reasonable (~100-120 chars)

### Naming Conventions

| Category | Style | Examples |
|---|---|---|
| Module functions | `snake_case` with module prefix | `motor_init()`, `safety_is_estop_active()`, `control_stop()` |
| Mode functions | `snake_case` without prefix | `continuous_start()`, `pulse_start()`, `jog_press_cb()` |
| Screen functions | `screen_name_create()` / `screen_name_update()` | `screen_main_create()`, `screen_pulse_update()` |
| Event callbacks | `snake_case` with `_cb` suffix | `start_event_cb()`, `back_event_cb()` |
| Local variables | `camelCase` | `stepper`, `adcFiltered`, `mainScreenPtr` |
| Global variables | `g_` prefix + `camelCase` | `g_settings`, `g_estopPending`, `g_presets` |
| Constants/macros | `UPPER_SNAKE_CASE` | `PIN_ENA`, `MAX_RPM`, `GEAR_RATIO`, `COL_BG` |
| Enums (type) | `PascalCase` | `SystemState`, `Direction`, `ScreenId` |
| Enums (values) | `UPPER_SNAKE_CASE` | `STATE_IDLE`, `DIR_CW`, `SCREEN_MAIN` |
| Structs (type) | `PascalCase` | `SystemSettings`, `Preset` |
| Struct members | `snake_case` | `calibration_factor`, `pulse_on_ms` |
| FreeRTOS tasks | `camelCase` + "Task" suffix | `controlTask`, `motorTask`, `lvglTask` |
| LVGL widget pointers | `camelCase` with type suffix | `startBtn`, `rpmLabel`, `stateLabel` |

### Include Ordering

1. Own header first
2. Same-directory sibling headers
3. Cross-module headers (relative paths with `../`)
4. Third-party/system headers in angle brackets

```c
#include "control.h"
#include "modes.h"
#include "../config.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include "esp_task_wdt.h"
#include <atomic>
```

### Comments

- File-level comment at top of every `.cpp`: `// Module Name - Description`
- Section separators: `// ────` (78-char wide box lines)
- Inline comments with `//` only — no `/* */` block comments
- No Doxygen/Javadoc-style documentation

### Error Handling

- Use `LOG_E()`, `LOG_W()`, `LOG_I()`, `LOG_D()` macros (defined in `config.h`, suppressed in release builds)
- Guard clauses with early return for precondition checks
- `ESP.restart()` for fatal initialization errors
- Return `false` for non-fatal failures
- Null-check stepper pointer before all motor operations

### Concurrency

- Shared state between cores: use `std::atomic` with explicit memory ordering, or `volatile` flags
- UI callbacks must set `volatile` flags — never call `speed_apply()` or motor functions directly from UI
- Use `xSemaphoreTake`/`xSemaphoreGive` for mutex-protected data (e.g., `g_presets_mutex`)
- `strlcpy()` for safe string copies (not `strcpy` or `strncpy`)

### Design Patterns

- No namespaces — module prefixes replace them (`motor_`, `safety_`, `control_`, `speed_`, `storage_`, `screen_`)
- No classes — free functions with file-scope statics (C-style procedural)
- Pending request pattern: UI sets `volatile` flag, Core 0 task executes within cycle
- Screen files follow structure: static widget pointers → event callbacks → `screen_name_create()` → `screen_name_update()`
- Widget creation: absolute positioning with `lv_obj_set_pos()`, no flex/grid layouts
- Style selector `0` used for simple properties (not `LV_STATE_DEFAULT`)

## Critical Constraints

- **ENA pin HIGH = motor disabled** — never drive LOW without ESTOP check
- `speed_apply()` must ONLY be called from Core 0 (motorTask)
- `applySpeedAcceleration()` must follow `setSpeedInMilliHz()` for live speed changes
- LVGL task needs 64KB stack — arc/scale rendering is stack-heavy
- PSRAM for LVGL buffers — internal RAM causes overflow with rotation buffer
- Pot ADC reference is 3315, override threshold is 200 ADC counts
- Hardware config in `src/config.h`: Motor STEP(50), DIR(51), ENA(52), ESTOP(34)

## UI Module Isolation

- All LVGL UI code must reside in `src/ui/` — never mix hardware logic with GUI rendering
- Motor logic stays in `src/motor/` exclusively
- Screen files in `src/ui/screens/` — each screen is one file pair (no formal pairs, single `.cpp` per screen)
