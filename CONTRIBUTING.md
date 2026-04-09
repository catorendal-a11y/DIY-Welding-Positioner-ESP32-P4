# Contributing to DIY Welding Positioner

Thank you for considering contributing to this open-source project!

## Development Environment

- **PlatformIO** with `pioarduino` platform (ESP-IDF 5.5.x)
- **Board:** GUITION JC4880P443C (ESP32-P4 + ESP32-C6, 4.3" MIPI-DSI)
- **Python:** System Python 3.14 is incompatible. Use bundled `pio.exe` which ships Python 3.12

### Build Commands

```bash
# Build release (default, silent)
PYTHONIOENCODING=utf-8 "C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run

# Build debug (verbose serial logging)
PYTHONIOENCODING=utf-8 "C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-debug

# Flash to device
PYTHONUTF8=1 PYTHONIOENCODING=utf-8 "C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run --target upload

# Run native tests (Unity framework)
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" test -e native
```

> **Windows note:** `PYTHONUTF8=1` is required for flash to prevent esptool Unicode crash in cp1252.

## Development Workflow

1. **Fork** the repository on GitHub
2. **Clone** your fork locally
3. **Branch** off `master` for your feature/bugfix (e.g., `git checkout -b feature/my-feature`)
4. **Build** locally using the commands above — only build errors matter, LSP errors are false (missing PlatformIO headers)
5. **Commit** with clear, descriptive messages following conventional commits format
6. **Push** your branch to your fork
7. **Submit a Pull Request** to the original repository

## Architecture Overview

```
Core 0 (Realtime)          Core 1 (UI)
------------------          ------------------
safetyTask  (pri 5, 4KB)    lvglTask   (pri 2, 64KB)
motorTask   (pri 4, 5KB)    storageTask (pri 1, 12KB)
controlTask (pri 3, 4KB)
```

### Module Directories

| Directory | Purpose |
|-----------|---------|
| `src/control/` | State machine, welding modes |
| `src/motor/` | FastAccelStepper driver, speed, acceleration, microstep, calibration |
| `src/safety/` | E-STOP interrupt, watchdog |
| `src/storage/` | NVS persistence (`Preferences`), presets + settings as JSON blobs, optional LittleFS migration |
| `src/ui/` | LVGL display, screens, theme |

## Coding Standards

### Module Isolation
- **UI code** must reside in `src/ui/` — never mix hardware logic with GUI rendering
- **Motor logic** stays in `src/motor/` exclusively
- **Screen files** in `src/ui/screens/` — one `.cpp` per screen

### Threading Rules
- All `lv_*` calls must come from Core 1 (`lvglTask`) only
- `speed_apply()` must ONLY be called from Core 0 (`motorTask`)
- UI callbacks must set `volatile` flags — never call motor functions directly
- Shared state between cores: use `std::atomic` or `volatile` flags
- Mutex-protected data (e.g., `g_presets`, `g_stepperMutex`): always `xSemaphoreGive` before ANY return path
- **`g_stepperMutex` is a FreeRTOS mutex** (`SemaphoreHandle_t`) — NOT a spinlock. Uses `xSemaphoreTake`/`xSemaphoreGive`
- **Never use `lv_obj_delete()` inside event callbacks** for the triggering object — use `lv_obj_delete_async()`
- Screens with static widget pointers must implement `screen_*_invalidate_widgets()` called from `screens_reinit()`

### LVGL Rules
- Use LVGL 9 API names only (e.g., `lv_button_create`, not `lv_btn_create`)
- `lv_style_t` must be static/global — never stack-allocated
- Canvas is 800x480 landscape — coordinates must satisfy `x + width <= 800`, `y + height <= 480`
- ASCII only in labels (0x20-0x7E) — no Unicode, no arrows, no degree sign
- Font max is `montserrat_40` — `montserrat_48` crashes ESP32-P4
- Do NOT use `lv_display_set_rotation()` — manual rotation in flush callback only
- `lv_display_flush_ready()` must be called exactly once per flush

### Style
- **Indentation:** 2 spaces (no tabs)
- **Braces:** K&R — opening brace on same line
- **Header guards:** `#pragma once` exclusively
- **No namespaces** — module prefixes (`motor_`, `safety_`, `control_`, `screen_`)
- **No classes** — free functions with file-scope statics
- **Error handling:** `LOG_E()`, `LOG_W()`, `LOG_I()`, `LOG_D()` macros (suppressed in release)
- **Safe strings:** `strlcpy()` (not `strcpy` or `strncpy`)

### Naming Conventions

| Category | Style | Examples |
|----------|-------|----------|
| Module functions | `snake_case` with prefix | `motor_init()`, `safety_is_estop_active()` |
| Screen functions | `screen_name_create/update` | `screen_main_create()`, `screen_pulse_update()` |
| Event callbacks | `snake_case` with `_cb` | `start_event_cb()`, `back_event_cb()` |
| Local variables | `camelCase` | `stepper`, `adcFiltered`, `mainScreenPtr` |
| Global variables | `g_` prefix + `camelCase` | `g_settings`, `g_presets` |
| Constants/macros | `UPPER_SNAKE_CASE` | `PIN_ENA`, `MAX_RPM`, `GEAR_RATIO` |
| FreeRTOS tasks | `camelCase` + "Task" | `controlTask`, `motorTask`, `lvglTask` |

## Known Platform Constraints

- ESP32-P4 uses RISC-V — `-mfix-esp32-psram-cache-issue` will crash
- `DRIVER_RMT` macro doesn't exist on P4
- `LittleFS.rename()` crashes on ESP32-P4 — use direct `FILE_WRITE`
- GPIO 28/32 are PCB-routed to C6 co-processor — cannot use as GPIO

## Documentation

Before modifying code, read:
- `AGENTS.md` (local only, not in repo) — full project context and discovered constraints
- `docs/` — hardware setup, safety system, EMI mitigation, implementation notes
- `wiki/` — getting started, troubleshooting, architecture
