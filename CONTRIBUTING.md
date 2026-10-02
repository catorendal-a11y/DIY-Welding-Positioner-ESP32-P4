# Contributing to DIY Welding Positioner

Thank you for considering contributing to this open-source project!

Please read our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to uphold it.

## Development Environment

- **PlatformIO Core 6.2.0** from `requirements-dev.txt`, with pinned `pioarduino` 55.03.312-1 (Arduino 3.3.12 / ESP-IDF 5.5.5)
- **Board:** GUITION JC4880P443C (ESP32-P4 + ESP32-C6, 4.3" MIPI-DSI)
- **Python:** CI uses Python 3.11. Use PlatformIO's managed runtime or a project virtual environment. On Linux/macOS/Git Bash, `./install.sh` creates `.venv` and installs project dependencies; `./install.sh --check` checks an existing installation.

Install the pinned CLI with `python -m pip install -r requirements-dev.txt` in the managed/project environment. [Dependency upgrade notes](docs/DEPENDENCY_UPGRADE_2026-10-02.md) explain the Git source pins and LVGL migration.

### Build Commands

```bash
# Build release (default, silent)
pio run

# Build debug (verbose serial logging)
pio run -e esp32p4-debug

# Build USB mirror
pio run -e esp32p4-mirror

# Flash to device
PYTHONUTF8=1 pio run --target upload

# Run native tests (Unity framework)
pio test -e native -e native-control
```

Windows PowerShell: set `$env:PYTHONUTF8='1'` before upload if the terminal uses a legacy encoding. Detect the port with `pio device list`, then pass `--upload-port COM3` (example).

Run the actual LVGL simulator with `.\simulator\run.ps1 -SelfTest`. CI checks native tests, all three firmware variants and simulator navigation. Device tests require an isolated bench with motor power controlled; see [test/README.md](test/README.md).

## Development Workflow

1. **Fork** the repository on GitHub
2. **Clone** your fork locally
3. **Branch** off `master` for your feature/bugfix (e.g., `git checkout -b feature/my-feature`)
4. **Build and test** relevant changes using the commands above. Configure the editor from PlatformIO's compile database before deciding whether diagnostics are genuine.
5. **Commit** with clear, descriptive messages following conventional commits format
6. **Push** your branch to your fork
7. **Submit a Pull Request** to the original repository

## Architecture Overview

```
Core 0 (Realtime)          Core 1 (UI)
------------------          ------------------
safetyTask  (pri 5, 4KB)    lvglTask   (pri 2, 64KB)
inputTask   (pri 4, 5KB)    storageTask (pri 1, 12KB)
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
- `speed_apply()` must ONLY be called from Core 0 (`controlTask`)
- UI callbacks request control actions or deferred saves; never execute motor operations directly. `volatile` does not synchronize cross-core state.
- Shared state between cores: use `std::atomic` with explicit memory ordering where required; `volatile` alone is insufficient on RISC-V SMP
- Mutex-protected data (`g_presets_mutex`, `g_settings_mutex`, `g_stepperMutex`, `g_nvs_mutex`): always `xSemaphoreGive` before ANY return path
- **`g_stepperMutex` is a FreeRTOS mutex** (`SemaphoreHandle_t`) — NOT a spinlock. Uses `xSemaphoreTake`/`xSemaphoreGive`
- **Never use `lv_obj_delete()` inside event callbacks** for the triggering object — use `lv_obj_delete_async()`
- Screens with static widget pointers must implement `screen_*_invalidate_widgets()` called from `screens_reinit()`

### LVGL Rules
- Use LVGL 9 API names only (e.g., `lv_button_create`, not `lv_btn_create`)
- `lv_style_t` must be static/global — never stack-allocated
- Canvas is 800x480 landscape — coordinates must satisfy `x + width <= 800`, `y + height <= 480`
- Use English label text and prefer ASCII for ordinary labels. Use LVGL symbols or additional glyphs only when supported by the selected font and verified in runtime captures; do not assume arbitrary Unicode renders.
- Shared body fonts have a 14 px floor. Main digits use the checked-in 104 px Montserrat numeric subset. Keep generated-font source, simulator/firmware integration and OFL license together; verify any new font on the device.
- Do NOT use `lv_display_set_rotation()` — manual rotation in flush callback only
- `lv_display_flush_ready()` must be called exactly once per flush

### Style
- **Indentation:** 2 spaces (no tabs)
- **Braces:** K&R — opening brace on same line
- **Header guards:** `#pragma once` exclusively
- **No namespaces** — module prefixes (`motor_`, `safety_`, `control_`, `screen_`)
- **No classes** — free functions with file-scope statics
- **Error handling:** `LOG_E()` is **always compiled in** (release + debug) so field failures show up on serial; `LOG_W/I/D` are debug-build only. Use `fatal_halt("<context>")` from `src/app_state.h` for unrecoverable init errors instead of calling `ESP.restart()` directly.
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
- Runtime settings/presets are stored in NVS (`Preferences`), with one-time LittleFS legacy migration only
- GPIO 32 is used for DM542T alarm input; GPIO 28, 14-19, and 54 may be PCB-routed to the C6 co-processor and should not be repurposed without the schematic

## Documentation

Use English for source comments, UI labels, documentation, issue templates, commit messages and release notes. Preserve builder content and hardware photos when updating the README.

Before modifying code, read:
- `AGENTS.md`, if supplied locally; this file is not required for a clone/build
- `docs/` — hardware setup, safety system, EMI mitigation, implementation notes
- `wiki/` — getting started, troubleshooting, architecture

## Release workflow

Update `FW_VERSION`, README, STATUS, CHANGELOG, current wiki pages and English notes in `docs/releases/<version>.md`. Push to master and wait for all CI jobs, then create the matching `vX.Y.Z` tag. The release workflow requires successful native, firmware and actual LVGL CI for that exact commit, downloads its release/debug/mirror binaries, verifies version/partition layout and publishes bundles with licenses and SHA-256 checksums. [Flashing instructions](docs/releases/FLASHING.md) document the current release.
