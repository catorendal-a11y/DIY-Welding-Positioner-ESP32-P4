# Project Status

**Last Updated:** 2026-11-10
**Published firmware:** v2.4.0
**Source:** master - reworked screens, fault screen, screen saver and logic fixes (released as v2.4.0)
**Build:** 492 native/control/speed/storage/safety test cases, packaging regressions, Linux/Windows simulator self-tests/layout audits and release/debug/mirror builds. **The owner reported v2.3.0 tested on the assembled machine on 2026-11-04.** TIG HF works on the owner's machine. The owner reported the v2.3.1 build working on the same machine the same day. Stop time and calibration accuracy have not been measured.
**v2.4.0:** a heartbeat timestamp race fix, eight screen-logic fixes and the reworked screens, fault overlay and screen saver. The owner flashed the v2.4.0 code on 2026-11-10 and confirmed that it boots and that the reworked screens, the fault screen and the screen saver display correctly. Motion, pedal and calibration were not re-tested on it. See the [release notes](docs/releases/v2.4.0.md).

---

## Completed

### Core Motor Control

Runtime stepper calls belong to controlTask; inputTask samples ADC/pedal separately. See [implementation and validation](docs/CONTROL_SETUP_IMPLEMENTATION.md).
- [x] **ESP32-P4 MIPI-DSI display** (ST7701S 480x800, RGB565, landscape rotation)
- [x] **GT911 capacitive touch** (I2C, coordinate mapping)
- [x] **LVGL 9.6.0 UI framework** (800x480 landscape, 23 registered `ScreenId` roots + E-STOP overlay)
- [x] **FastAccelStepper motor control** (hardware RMT pulses, v1.4.0, pinned upstream commit)
- [x] **FreeRTOS dual-core architecture** (Core 0: Input/Control/Safety, Core 1: UI/Storage)
- [x] **4 rotation modes:** Continuous, Jog, Pulse, Step, plus a start countdown
- [x] **RPM adjustment** (live potentiometer input; idle main-screen +/− beside the speed value, 0.001 RPM steps below 0.1 RPM; Jog and program/edit controls)
- [x] **Thread-safe cross-core speed updates** (atomic + request flag pattern, FreeRTOS mutex for stepper)
- [x] **`applySpeedAcceleration()`** for immediate speed changes during running
- [x] **Linear acceleration phase** (resonance-zone traversal)
- [x] **Microstepping selection** (1/4, 1/8, 1/16, 1/32)
- [x] **Flash writes only at rest** (the prebuilt framework has no IRAM-safe RMT/GPIO ISRs, so saves wait for IDLE/ESTOP)
- [x] **ADC potentiometer** (IIR filtering, 0-3315 ADC range, 200-count override threshold)

### Safety
- [x] **E-STOP input** (GPIO34 HIGH healthy / LOW fault; ISR drives ENA HIGH and wakes dimmed display; physical latency requires measurement)
- [x] **Motion-start safety re-checks** (ENA is re-disabled if E-STOP/ALM appears during the final start window)
- [x] **Task Watchdog Timer** (input, control and safety tasks; checked setup/feed)
- [x] **Boot-safe ENA pin** (motor disabled on startup)
- [x] **CAS state transitions** (race-free between safetyTask and controlTask)
- [x] **Fault overlay** (full screen, blocks all interaction; states the next action and the cause; red while reset is blocked, amber when only the reset is missing)
- [x] **UI reset from ESTOP** (via Core 0 pending flag pattern)
- [x] **Task, UI and touch supervision** (CONTROL STALE, INPUT STALE, HMI STALE; a stamp written just after the checker read the clock counts as fresh)
- [x] **Speed-pot break detection** (SPEED POT after three samples at the rail)
- [x] **Visible crash and storage faults** (UNEXPECTED RESET, STORAGE)
- [x] **Per-diameter RPM cap** (`MAX_STEP_HZ`, `MOTOR_RPM_LIMIT`)

### UI/UX

v2.2.0 includes the [idle screen saver](docs/SCREEN_SAVER.md) and [LVGL UI refinements](docs/LVGL_9_6_UI_IMPROVEMENTS.md).
- [x] **23 registered root screens** with lazy creation pattern + ESTOP overlay
- [x] **8 accent color themes** (switchable from Display Settings; combines with dark/light neutral UI mode)
- [x] **Dark / Light UI mode** (Display Settings **Appearance**, persisted as `color_scheme` in NVS `cfg`)
- [x] **Settings hub** (Motor Config, Calibration, Setup Wizard, Display, Pedal Settings, Diagnostics, System Info, About)
- [x] **Display Settings** (brightness slider, screen-saver timeout/preview, Appearance dark/light, accent theme)
- [x] **USB-C live mirror** (mirror firmware + Windows SDL viewer; armed from Display Settings; PC input is LVGL touch only)
- [x] **System Info** (CPU core load, heap, PSRAM, uptime)
- [x] **Diagnostics** (live ESTOP, ALM, DIR switch, pedal switch, ENA, direction, RPM, motion-block state, recent event log)
- [x] **Pedal Settings** (pedal arm/disarm, GPIO33 switch status, ADS1115 analog status)
- [x] **Workpiece diameter per preset** (`workpiece_diameter_mm`, 0 = default reference diameter)
- [x] **Motor Config** (driver type, microstepping, acceleration, max RPM, roller diameter, gear ratio, stop angle at max RPM, direction switch, pedal enable)
- [x] **Guided calibration** (non-scrolling stages, same-page diameter, interrupted-move rejection, isolated draft, verification and storage receipt)
- [x] **About screen** (firmware version, hardware info)
- [x] **Program Edit** (fixed layout, explicit run/available modes, exact RPM within the range for the program's diameter, full-screen input and retained drafts)
- [x] **Guided setup** (motor, direction, verified calibration and physical E-STOP function check; compatible legacy settings)
- [x] **Snapshot freshness** (40 ms UI updates, 100 ms stale motion blocking; STOP stays available)
- [x] **Consistent footer navigation** and back buttons

### Storage
- [x] **Program preset storage** (ArduinoJson blobs in **NVS** `wrot`/`prs`, 16 slots; one-time LittleFS migration)
- [x] **Settings persistence** (motor config, display settings, and related fields in NVS `wrot`/`cfg`)
- [x] **Settings-before-presets load order** (preset RPM clamp uses saved `max_rpm`)
- [x] **Microstep validation on load** (only 4/8/16/32 accepted; invalid NVS falls back to 16)
- [x] **Mutex-protected presets** (`g_presets_mutex`)
- [x] **Debounced flash writes** (500ms presets, 1000ms settings; deferred while the motor runs)

### Hardware
- [x] **Foot pedal firmware support** (analog speed via ADS1115 I2C ADC, digital switch GPIO33)
- [x] **Pedal/ADS1115 hardware commissioning** (2026-10-05: ADS1115 detected at 0x48; GPIO33 switch + analog speed owner-confirmed working; runtime switch-only fallback when no ADS1115 is present)
- [x] **Direction switch** (GPIO29, CW/CCW toggle)
- [x] **Gear ratio 1:108** default (60 x 72/40, NMRV030 + spur), editable in Motor Config
- [x] **TIG HF field validation** (welding works with ESP32-P4 screen, driver, and PSU inside one grounded metal enclosure)

### Documentation
- [x] **README** (source and release v2.4.0, feature list, wiring diagram, BOM, TIG HF enclosure requirement; synced with `config.h`)
- [x] **Wiki** (Home, Getting Started, Hardware Setup, Troubleshooting, Roadmap, Architecture)
- [x] **docs/** (Hardware Setup, Safety System, EMI Mitigation, Implementation, Instructables)
- [x] **Wiring diagram v2** (SVG, GPIO29 on correct side, clean cable routing)
- [x] **Simulator screenshots** (`simulator/run.ps1 -Screenshots <dir>` exports every registered screen)

---

### Stability & Robustness (v2.0.2+)
- [x] **FreeRTOS mutex for stepper** (replaced portMUX_TYPE spinlock — fixed IWDT crash on cross-core contention)
- [x] **LVGL async object deletion** (lv_obj_delete_async for keyboard/numpad cleanup from event callbacks)
- [x] **Screen widget invalidation** (invalidate_widgets pattern for Step, Programs, ProgramEdit, and other screens with static widgets)
- [x] **Deferred keyboard cleanup** (*ClosePending flags, actual cleanup in update cycle)
- [x] **Screen reinit safety** (screens_reinit calls invalidate_widgets for all screens with static pointers)
- [x] **Confirm dialog validation** (returnScreen range check prevents invalid screen navigation)
- [x] **E-STOP display wake** (v2.0.3 — `g_wakePending` + `dim_reset_activity()` so dimmed MIPI panel shows fault UI)
- [x] **Owner-task fault cleanup** (physical E-STOP/ALM disables ENA independently; bounded `forceStop()` cleanup belongs exclusively to controlTask)
- [x] **Step-screen rebuild cleanup** (no async object delete immediately before `lv_obj_clean()`)
- [x] **USB mirror partial flush** (dirty rectangles instead of full 800x480 mirror traffic)

### v2.0.5 — Cross-core & Error-handling Cleanup
- [x] **Centralised cross-core atomics** in `src/app_state.h`/`app_state.cpp` (single source of truth; no more scattered `std::atomic` definitions across safety/storage/main)
- [x] **`fatal_halt()`** utility (logs reason via `LOG_E`) — replaces scattered `ESP.restart()` calls in storage and motor init paths. It rebooted in v2.0.5; it now stays halted with ENA disabled
- [x] **`LOG_E` always compiled in** (release + debug) so field failures are serial-visible; `LOG_W/I/D` stay debug-only
- [x] **Boot-time ESTOP de-floating** (3-sample majority vote, 500 µs spacing, on `INPUT_PULLUP` settle) — avoids false ESTOP from unstabilised GPIO34 at power-on
- [x] **Non-blocking ADS1115 pedal ADC** (state-machine `ads_poll_and_start()` in `inputTask`; blocking helper reserved for `speed_init()`)
- [x] **`motor_set_target_milli_hz()`** encapsulates stepper mutex + speed + acceleration (keeps `g_stepperMutex` inside the motor module)
- [x] **`lvglTask` boot sequence** now runs all `lv_timer_handler()` calls under `lvgl_lock()`/`lvgl_unlock()`
- [x] **Native tests extended** (`milli_hz_floor_testable` edge cases: zero / negative / NaN / floor / saturation)
- [x] **Repo hygiene** (`compile_commands.json`, `.cache/`, `.venv/`, `.ruff_cache/` gitignored; build-log artefacts removed)

## In Progress

- [ ] **Higher-RPM DM542T tuning** (DM542T driver mode and ALM input exist; wider tested RPM range is still field work)

## Planned

- [ ] **Enclosure design** (3D printable)
- [ ] **Assembly guide**

## Known open items

These cannot be solved in firmware alone:

- A floating pot wiper and an open E-STOP wire are not detected, and the analog pedal is not jump-checked.

---

## Build Info

See the [dependency inventory and migration analysis](docs/DEPENDENCY_UPGRADE_2026-10-02.md). Metrics describe the v2.3.0 release build.

| Metric | Value |
|--------|-------|
| **Platform** | pioarduino 55.03.312-1 (Arduino 3.3.12 / ESP-IDF 5.5.5) |
| **Board** | GUITION JC4880P443C (ESP32-P4 + ESP32-C6) |
| **RAM Usage** | 11.1% (36,504 bytes / 327,680 bytes, release build) |
| **Flash Usage** | 17.6% (1,153,124 bytes / 6,553,600 bytes, release build) |
| **FastAccelStepper** | 1.4.0, upstream `f24a659` |
| **LVGL** | 9.6.0 (RGB565) |
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
| GPIO 34 | E-STOP | HIGH healthy / LOW fault, FALLING interrupt; verify conditioned interface and cable-break behavior |
| GPIO 32 | DRIVER ALM | DM542T alarm input, active LOW |
| GPIO 33 | PEDAL SW | Foot pedal switch, active LOW |
| GPIO 7/8 | Touch I2C | GT911 + ADS1115 pedal ADC (shared bus) |
| GPIO 28 | Reserved | Routed to on-board ESP32-C6 per GUITION — do not repurpose without schematic |
| GPIO 14-19, 54 | Board bus | Routed toward ESP32-C6 per GUITION — do not use as application GPIO without schematic |
