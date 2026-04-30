# Project Status

**Last Updated:** 2026-04-30
**Firmware:** v2.0.9
**Build:** SUCCESS (Release & Debug, 0 errors 0 warnings) - verified 2026-04-30 after motor/safety/storage audit

---

## Completed

### Core Motor Control
- [x] **ESP32-P4 MIPI-DSI display** (ST7701S 480x800, RGB565, landscape rotation)
- [x] **GT911 capacitive touch** (I2C, coordinate mapping)
- [x] **LVGL 9.5.x UI framework** (800x480 landscape, 21 active `ScreenId` roots + E-STOP overlay)
- [x] **FastAccelStepper motor control** (hardware RMT pulses, v0.33.x)
- [x] **FreeRTOS dual-core architecture** (Core 0: Motor/Safety, Core 1: UI)
- [x] **5 welding modes:** Continuous, Jog, Pulse, Step, Timer
- [x] **Live RPM adjustment** (potentiometer; touch +/- on Jog and in program/settings flows; main RPM from pot)
- [x] **Thread-safe cross-core speed updates** (atomic + request flag pattern, FreeRTOS mutex for stepper)
- [x] **`applySpeedAcceleration()`** for immediate speed changes during running
- [x] **Linear acceleration phase** (resonance-zone traversal)
- [x] **Microstepping selection** (1/4, 1/8, 1/16, 1/32)
- [x] **ISR IRAM-safe RMT/GPTIMER flags**
- [x] **ADC potentiometer** (IIR filtering, 0-3315 ADC range, 200-count override threshold)

### Safety
- [x] **Hardware E-STOP** (GPIO34, active LOW fault, <0.5ms ISR; wakes dimmed display)
- [x] **Motion-start safety re-checks** (ENA is re-disabled if E-STOP/ALM appears during the final start window)
- [x] **Task Watchdog Timer** (motor & safety tasks)
- [x] **Boot-safe ENA pin** (motor disabled on startup)
- [x] **CAS state transitions** (race-free between safetyTask and controlTask)
- [x] **E-STOP UI overlay** (full-screen red, blocks all interaction)
- [x] **UI reset from ESTOP** (via Core 0 pending flag pattern)

### UI/UX
- [x] **21 active root screens** with lazy creation pattern + ESTOP overlay
- [x] **8 accent color themes** (switchable from Display Settings; combines with dark/light neutral UI mode)
- [x] **Dark / Light UI mode** (Display Settings **UI MODE**, persisted as `color_scheme` in NVS `cfg`)
- [x] **Settings hub** (Motor Config, Calibration, Display, Pedal Settings, Diagnostics, System Info, About)
- [x] **Display Settings** (brightness slider, dim timeout, UI MODE dark/light, accent theme)
- [x] **System Info** (CPU core load, heap, PSRAM, uptime)
- [x] **Diagnostics** (live ESTOP, ALM, DIR switch, pedal switch, ENA, direction, RPM, motion-block state, recent event log)
- [x] **Pedal Settings** (pedal arm/disarm, GPIO33 switch status, ADS1115 analog status)
- [x] **Workpiece diameter per preset** (`workpiece_diameter_mm`, 0 = default reference diameter)
- [x] **Motor Config** (microstepping, acceleration, direction switch, pedal enable)
- [x] **About screen** (firmware version, hardware info)
- [x] **Program Edit** (full preset editor with on-screen keyboard)
- [x] **Consistent footer navigation** and back buttons

### Storage
- [x] **Program preset storage** (ArduinoJson blobs in **NVS** `wrot`/`prs`, 16 slots; one-time LittleFS migration)
- [x] **Settings persistence** (motor config, display settings, and related fields in NVS `wrot`/`cfg`)
- [x] **Settings-before-presets load order** (preset RPM clamp uses saved `max_rpm`)
- [x] **Microstep validation on load** (only 4/8/16/32 accepted; invalid NVS falls back to 16)
- [x] **Mutex-protected presets** (`g_presets_mutex`)
- [x] **Debounced flash writes** (500ms presets, 1000ms settings)

### Hardware
- [x] **Foot pedal support** (analog speed via ADS1115 I2C ADC, digital switch GPIO33)
- [x] **Direction switch** (GPIO29, CW/CCW toggle)
- [x] **Gear ratio 1:108** total (60 x 72/40, NMRV030 + spur)
- [x] **TIG HF field validation** (welding works with ESP32-P4 screen, driver, and PSU inside one grounded metal enclosure)

### Documentation
- [x] **README** (v2.0.9, feature list, wiring diagram, BOM, TIG HF enclosure requirement; synced with `config.h`)
- [x] **Wiki** (Home, Getting Started, Hardware Setup, Troubleshooting, Roadmap, Architecture)
- [x] **docs/** (Hardware Setup, Safety System, EMI Mitigation, Implementation, Instructables)
- [x] **Wiring diagram v2** (SVG, GPIO29 on correct side, clean cable routing)

---

### Stability & Robustness (v2.0.2+)
- [x] **FreeRTOS mutex for stepper** (replaced portMUX_TYPE spinlock — fixed IWDT crash on cross-core contention)
- [x] **LVGL async object deletion** (lv_obj_delete_async for keyboard/numpad cleanup from event callbacks)
- [x] **Screen widget invalidation** (invalidate_widgets pattern for Step, Programs, ProgramEdit, and other screens with static widgets)
- [x] **Deferred keyboard cleanup** (*ClosePending flags, actual cleanup in update cycle)
- [x] **Screen reinit safety** (screens_reinit calls invalidate_widgets for all screens with static pointers)
- [x] **Confirm dialog validation** (returnScreen range check prevents invalid screen navigation)
- [x] **E-STOP display wake** (v2.0.3 — `g_wakePending` + `dim_reset_activity()` so dimmed MIPI panel shows fault UI)
- [x] **Safety-task stepper serialization** (E-STOP/ALM `forceStop()` path uses `g_stepperMutex`; no unsynchronized FastAccelStepper calls)
- [x] **Step-screen rebuild cleanup** (no async object delete immediately before `lv_obj_clean()`)

### v2.0.5 — Cross-core & Error-handling Cleanup
- [x] **Centralised cross-core atomics** in `src/app_state.h`/`app_state.cpp` (single source of truth; no more scattered `std::atomic` definitions across safety/storage/main)
- [x] **`fatal_halt()`** utility (logs reason via `LOG_E`, drains serial, reboots) — replaces scattered `ESP.restart()` calls in storage and motor init paths
- [x] **`LOG_E` always compiled in** (release + debug) so field failures are serial-visible; `LOG_W/I/D` stay debug-only
- [x] **Boot-time ESTOP de-floating** (3-sample majority vote, 500 µs spacing, on `INPUT_PULLUP` settle) — avoids false ESTOP from unstabilised GPIO34 at power-on
- [x] **Non-blocking ADS1115 pedal ADC** (state-machine `ads_poll_and_start()` in `motorTask`; blocking helper reserved for `speed_init()`)
- [x] **`motor_set_target_milli_hz()`** encapsulates stepper mutex + speed + acceleration (keeps `g_stepperMutex` inside the motor module)
- [x] **`lvglTask` boot sequence** now runs all `lv_timer_handler()` calls under `lvgl_lock()`/`lvgl_unlock()`
- [x] **Native tests extended** (`milli_hz_floor_testable` edge cases: zero / negative / NaN / floor / saturation)
- [x] **Repo hygiene** (`compile_commands.json`, `.cache/`, `.venv/`, `.ruff_cache/` gitignored; build-log artefacts removed)

## In Progress

- [ ] **Higher-RPM DM542T tuning** (DM542T driver mode and ALM input exist; wider tested RPM range is still field work)

## Planned

- [ ] **Enclosure design** (3D printable)
- [ ] **Assembly guide**
---

## Build Info

| Metric | Value |
|--------|-------|
| **Platform** | pioarduino (ESP-IDF 5.5.x) |
| **Board** | GUITION JC4880P443C (ESP32-P4 + ESP32-C6) |
| **RAM Usage** | ~10.0% (32 KB / 320 KB, release build) |
| **Flash Usage** | ~16.1% (1.06 MB / 6.5 MB, release build) |
| **FastAccelStepper** | 0.33.x |
| **LVGL** | 9.5.0 (RGB565) |
| **ArduinoJson** | 7.4.3 |

---

## Pinout

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 50 | STEP | RMT pulse to driver PUL+ |
| GPIO 51 | DIR | Direction to driver DIR+ |
| GPIO 52 | ENABLE | Active LOW to driver ENA |
| GPIO 49 | POT | 10k speed potentiometer (ADC) |
| GPIO 29 | DIR SWITCH | CW/CCW toggle, INPUT_PULLUP |
| GPIO 34 | E-STOP | NC contact, interrupt, active LOW |
| GPIO 32 | DRIVER ALM | DM542T alarm input, active LOW |
| GPIO 33 | PEDAL SW | Foot pedal switch, active LOW |
| GPIO 7/8 | Touch I2C | GT911 + ADS1115 pedal ADC (shared bus) |
| GPIO 28 | Reserved | Routed to on-board ESP32-C6 per GUITION — do not repurpose without schematic |
| GPIO 14-19, 54 | Board bus | Routed toward ESP32-C6 per GUITION — do not use as application GPIO without schematic |
