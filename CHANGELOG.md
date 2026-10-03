# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.2.0] - 2026-10-03

### Idle screen saver

- Add a flat graphite/orange screen saver with slow rotator-marker movement, English text and no shadows.
- Reuse the saved dim timeout as OFF/30s/1m/2m/5m, add Display PREVIEW and restore configured brightness on wake.
- Consume the complete wake touch through release on touchscreen, USB mirror and simulator. A held wake touch cannot reach START.
- Keep motion, commissioning, editors, overlays, stale status and pending/failed saves visible. Fault/E-STOP takes priority; waking preserves the previous page.
- Run the shared production inactivity policy in the simulator, with real LVGL pointer, timeout, brightness-command and lifecycle regression checks. See [screen-saver behavior](docs/SCREEN_SAVER.md).

### LVGL 9.6 UI refinement

- Trim unused text leading on Calibration and New Program/Edit Program, including their input overlays, while retaining descender space and the V5 layout.
- Bind main-screen text, speed-bar values and button visibility/availability through LVGL 9.6 typed observers. Skip unchanged text updates and keep all subject operations in the UI task.
- Bind Diagnostics to the same control snapshot used by the operating panel. Show `STATUS STALE` and suppress stale RPM values; keep GPIO readings separate from calculated motion status.
- Release subjects and observer callbacks during screen reconstruction. Keep direct update fallbacks for allocation failure and clear widget references on deletion.
- Add simulator coverage for observer notification, copied text, reset/rebind/delete, repeated screen reconstruction and stale-status recovery. Make the layout audit account for text leading trim without skipping clipping checks.
- Apply shared text leading trim across screen labels while retaining compound widget metrics. Keep fonts without cap-height metadata untrimmed. Dim unavailable buttons/sliders, show button availability immediately and retain the flat UI without shadows.
- Unify program/calibration/motor full-screen input panels, add visible Cancel, and clean up on close, navigation, deletion and theme reconstruction.
- Keep the fault/E-STOP overlay above open input panels and report interrupted calibration input explicitly.
- Rework Motor Configuration into larger cards with exact RPM/acceleration entry, integer/range/precision validation, isolated drafts, pending-save edit locks and fresh snapshot status. Bind calibration and motor text updates through typed observers.
- See [LVGL 9.6 UI implementation notes](docs/LVGL_9_6_UI_IMPROVEMENTS.md).

### Motion, input and persistence corrections

- Cancel countdown starts across STOP, faults/reset, stale control and navigation using the original control generation. Reset never renews a pending START.
- Re-clamp Jog against the current RPM limit at execution. Reject non-finite inputs, overflowing signed move counts and invalid program/configuration requests.
- Supervise Pulse deceleration and finite Step moves independently of the control loop, with wrap-safe completion budgets. Calculate move budgets from the commanded rate and acceleration.
- Keep the 20 Hz operational floor, expose a geometry-dependent minimum RPM and reject infeasible commands instead of silently increasing the requested rate. Apply fine RPM adjustments consistently in Step and Jog.
- Preserve requested/effective direction semantics in new programs and sub-editors. Capture the active analog input baseline so a stationary pedal cannot cancel a screen speed override.
- Validate complete JSON serialization and exact NVS byte counts; bound blobs and program counts, validate object types, normalize program IDs and validate both legacy files before migration. Preserve valid legacy UTF-8 names; new editor names use printable ASCII supported by the screen font.
- Show program and Display save receipts, lock program edits during pending writes, preserve the dim-time draft through theme reconstruction, review sequence/timing before program START and identify stale motion actions by explicit markers.
- Bound USB receive work and partial-frame lifetime, release the viewer pointer on focus loss and close failed transmit links. Replace unsupported LVGL CLIB heap monitoring with ESP-IDF heap statistics.
- Add real production speed/ADS and storage/migration test environments to CI, including allocation and short-write failures. See [the review and resolution report](docs/CODE_LOGIC_REVIEW_2026-10-03.md).

### Release packaging

- Distinguish pioarduino's generated application and combined `.factory.bin` images. Exclude combined factory images from application-only release bundles and reject ambiguous or missing application outputs.
- Apply the same filtering when re-packaging validated CI artifacts for an existing tag, keeping its original firmware commit and tag immutable.
- Include the reviewed wiring SVG as a separate release download and identify the documentation source commit separately from the tagged firmware/simulator provenance.

### Project entry documentation

- Shorten the README while retaining the full previous builder material in `docs/BUILDER_GUIDE.md`. Keep the exact GUITION board identity, hardware images, wiring overview, Setup Wizard, TIG HF enclosure requirement and key commissioning details visible.
- Correct the original wiring diagram in place and restyle it to match the V5 graphite/orange UI. Retain the hardware layout and common-cathode routing, correct the pedal switch endpoint to GPIO33 and its supply to the ADS1115 VDD net, separate the panel-pot wiper from the GND bus, show the NC input-interface requirement, add GPIO32 ALM, correct current/microstep notes and RPM limits, and remove the unmeasured 0.5 ms claim. Keep the firmware input contract unchanged.

## [2.1.1] - 2026-10-03

### FastAccelStepper re-audit and release

- Verify installed FastAccelStepper HEAD against upstream tag 1.4.0. Retain asynchronous force-stop drain handling, explicit RMT selection and typed movement results.
- Allocate the RMT channel in a bounded Core 0 boot task; engine task affinity alone does not select the interrupt allocation core. Keep ENA inhibited throughout initialization and stop boot on allocation/timeout failure.
- Correct IRQ-affinity and jitter claims. Publish firmware and the portable simulator from the exact green master CI commit with English release notes and complete download checksums.
- Keep the release-build check name aligned with the existing master branch protection so validated pull requests can merge without bypassing required checks.

### Dependency refresh and API migration

- Upgrade LVGL 9.5.0 to 9.6.0, use RGB565 default-format configuration, public driver/snapshot headers and dedicated object setters; retain the V5 screen design and English labels.
- Upgrade pioarduino to 55.03.312-1 (Arduino 3.3.12 / ESP-IDF 5.5.5), and pin PlatformIO Core 6.2.0 and native platform 1.2.1.
- Upgrade FastAccelStepper 0.33.14 to the upstream 1.4.0 tag commit `f24a659`; the registry artifact omits the corrected IDF5 RMT implementation. Select RMT explicitly and use typed movement results. Keep fault cleanup/reset pending until queued pulses drain, and reject DIR reconfiguration while moving.
- Upgrade Unity to the upstream 2.7.0 commit; prevent PlatformIO's registry runner from replacing it with 2.6.1.
- Refresh vendored GT911 1.2.1, ST7701 2.0.2~2 and EK79007 2.0.2~1 metadata/source. Use the official LDO header and C++-compatible touch configuration.
- Keep already-current ArduinoJson 7.4.3, esp_lcd_touch 1.2.1 and compatible SDL2; update packaging for LVGL's moved font-license directory.
- Add upstream RMT encoder regression to Linux/Windows CI and three production-control migration cases. Document the full 0.33.14-to-1.4.0 analysis and outstanding device measurements.

### Guided calibration and updated library features

- Replace the crowded calibration page with fixed Align → Measure → Verify → Save stages, same-page diameter entry and a full-screen numeric editor. Retain the V5 palette and visible normal STOP.
- Require completed moves for measurement; reject interruption, stale status, context changes, invalid input and out-of-range corrections. Keep unverified corrections outside saved settings; Restart restores the saved factor. Confirm the verified save receipt, including retry and STOP during saving.
- Inhibit unrelated mode/program and pedal starts during calibration. Add eight production regressions and the complete LVGL interaction flow (438 host cases total).
- Adopt LVGL 9.6 dedicated state queries/setters, explicit style caching, simulator argument/widget-tree validation and bounded label lines. Cache FastAccelStepper driver/DIR budgets for Diagnostics.
- Fix program editor positioning by resolving LVGL layout before shifting controls; add footer/mode-position audit checks and refresh runtime images.

### Program editor and touch API

- Replace the program editor's layout-offset workaround with fixed 800×480 positions, separate run-mode and availability buttons, a visible mode-settings summary and separate full-screen name/RPM editors.
- Keep the active run mode available, make mode selection idempotent, validate complete numeric input and UTF-8 byte capacity, retain drafts across mode editors and clean up keyboards on navigation.
- Restore low-RPM display precision in Continuous settings; use 0.001 RPM increments below 0.1 and 0.01 otherwise in all mode editors. Show Step duration at the minimum valid RPM.
- Apply explicit LVGL checked-state contrast and valid zero-duration transitions; skip repeated text/style updates for unchanged drafts.
- Replace deprecated touch-coordinate arrays with the typed `esp_lcd_touch_get_data()` API while retaining release-on-error and rotation handling.
- Add actual LVGL regressions for every sub-editor's precision, invalid entries, UTF-8 limits, mode availability, save/cancel and modal cleanup; refresh runtime screenshots and the complete library-feature review.

### Control ownership and guided setup

- Single control-task owner of runtime stepper calls; input task publishes ADC/pedal inputs separately. Physical E-STOP directly inhibits ENA without waiting for the executor.
- Try-only coherent control snapshots, 40 ms UI refresh and 100 ms freshness checks; stale control blocks new motion while STOP remains available.
- Four-stage Setup Wizard in the existing V5 design with actual physical-switch observations, verified manual calibration and durable completion receipt. Existing valid settings migrate without forcing setup.
- Precise fault titles and save receipts, shared production dispatcher/motor/modes in the simulator, commissioning UI regression tests and runtime screenshots.

### v2.1.1 maintenance

- Bounded motor access, rejected-command faults and independently supervised STOP acknowledgement/deceleration deadlines; physical latency still requires measurement.
- Correct ESP-IDF stack units, actual NVS partition size and entry statistics, I2C timeout units and ADS1115 conversion-ready handling.
- Adaptive RPM precision on all screens, effective program direction preview, estimated-speed labels and generation-specific calibration save confirmation.
- Explicit pulse phases/completed counts, modular step progress and retryable event snapshots with visible drop counts.
- Actual production dispatcher/motor/four-mode host tests, simulator fault scenarios and LVGL text/layout audit.
- Embedded simulator build identity, clean staging, required license checks, Windows CI packaging and release artifact integration.
- Serial-port autodetection and stricter application warnings; unused UI helpers removed.

## [2.1.0] - 2026-10-02

### V5 integration

- Graphite/orange UI across 22 screens, large native numeric font, main RPM +/−, direction selection and a prominent fault overlay with guarded reset.
- Separate STOP latch and generation checks, bounded JOG renewal, pedal release/freshness interlocks and critical-task readiness gate.
- Control-task parameter snapshots, retained program drafts, deferred motor configuration and pulse pause timing after motor stop.
- NVS validation/retries and save status, 300-second dimming persistence, touch release on read failure and bounded USB mirror writes.
- README preserves the original builder content and adds actual LVGL runtime images, design references and validation evidence. Hardware documentation states input polarity and unmeasured stop latency.
- Validation: 403 native tests, LVGL simulator self-test, release/debug/mirror builds and verified release upload. Physical motor/safety testing remains pending for this update.

### Added
- **Simulator screenshot dump** - `simulator/run.ps1 -Screenshots <dir>` builds the SDL simulator and exports one BMP per registered screen for UI review.

### Fixed
- **USB-C mirror throughput** - mirror firmware now uses LVGL partial render mode so the USB mirror sends dirty rectangles instead of full-screen refresh traffic.
- **Calibration save guard** - calibration settings cannot be saved until the verify step passes, reducing the chance of storing an unmeasured factor.
- **Calibration screen readability** - larger calibration factor, measured-angle input, action buttons, result/status text, and tighter layout constants.
- **Motion-start safety race** - continuous, jog-backed starts, and step-mode moves now re-check E-STOP / DM542T ALM immediately after ENA is pulled LOW. If a fault appears in that narrow window, ENA is driven HIGH again before any stepper command is issued.
- **Safety-task stepper access** - driver alarm and E-STOP handling no longer call `FastAccelStepper::forceStop()` without `g_stepperMutex`; ENA is already hardware-disabled and stepper API access stays serialized.
- **Storage load order** - settings now load before presets so preset RPM values are clamped against the saved `max_rpm` setting instead of the firmware default.
- **Invalid microstep in NVS** - persisted `microstep` now accepts only 4, 8, 16, or 32 and falls back to 16 for corrupt/old values.
- **Step screen rebuild cleanup** - removed an async-delete path immediately before `lv_obj_clean()` to avoid a possible LVGL double-delete edge during screen rebuild.

### Changed
- **ADS1115 runtime I2C timeout** - optional pedal ADC runtime reads use a short motor-task timeout while init/probe still keeps the longer timeout window.

### Removed
- **Obsolete root analysis docs** - removed `DIY-Welding-Positioner-ESP32-P4_UI_logic_function_improvements.md` and `DIY-Welding-Positioner-ESP32-P4_bug_logic_analysis.md` from the repository.

### Tests
- Verified USB mirror build, native tests, simulator self-test, simulator screenshot export, and flashing mirror firmware to ESP32-P4.
- Native test coverage extended for microstep storage validation.
- Verified with `pio test -e native`, release build, debug build, LVGL API scan, non-ASCII label scan, and `git diff --check`.

## [2.0.9] - 2026-04-30

### Documentation
- **TIG HF field validation** - documented the real-world welding test result: TIG welding works reliably, but HF start/noise requires the ESP32-P4 screen, stepper driver, and motor PSU to be installed inside the same grounded metal enclosure.
- **EMI / hardware guides** - strengthened enclosure, grounding, cable shielding, ferrite, and troubleshooting guidance for TIG HF environments.
- **CODE_OF_CONDUCT.md** — Contributor Covenant 2.1 for GitHub Community Standards checklist.

## [2.0.8] - 2026-04-29

### Changed
- **`FW_VERSION`** and bundled documentation, wiki, SVG mockups, and GitHub issue template placeholders updated to **v2.0.8** (canonical successor tag after v2.0.7).

## [2.0.7] - 2026-04-27

### Added
- **Dark / Light UI mode** — **Settings > Display** row **UI MODE** switches **DARK** (POST industrial neutral palette) vs **LIGHT** (warm cream HMI reference). Stored as `color_scheme` in NVS JSON `cfg` (`0` = dark, `1` = light). `theme_set_scheme()` / `theme_sync_colors()` apply `NEUT_DARK` / `NEUT_LIGHT` in `theme.cpp` (runtime `COL_*` globals + LVGL default theme).
- **`COL_HDR_MUTED`** — secondary text on the dark header strip (settings headers and main header right caption) uses a dedicated muted gray per scheme so it stays readable on `#090909` / `#1A1A1A` bars (instead of reusing `COL_TEXT_DIM`).

### Documentation
- README, STATUS, `docs/PROJECT_IMPLEMENTATION.md`, wiki (Home, Roadmap, Getting Started, Architecture) — synced with UI mode persistence and header muted color.

## [2.0.6] - 2026-04-26

### Added
- **Diagnostics screen** - `SCREEN_DIAGNOSTICS` shows live ESTOP, DM542T ALM, DIR switch, pedal switch, ENA, direction, target RPM, actual RPM and motion-block state for field troubleshooting.
- **Pedal Settings screen** - `SCREEN_PEDAL_SETTINGS` arms/disarms the GPIO33 foot pedal and shows live switch, ADS1115 and analog-source status.
- **RAM event log** - Diagnostics now shows the latest START/STOP, pedal, program, state and fault events so blocked starts can be debugged without serial output.
- **Workpiece diameter per preset** - STEP programs now store `workpiece_diameter_mm`, show it on program cards/edit summaries and apply it through ProgramExecutor before motion starts.

### Fixed
- **Main START race** — queued mode requests now clear stale pending STOP requests so a STOP tap while idle cannot swallow the next START; quick JOG release also cancels pending JOG start before motion can begin.
- **Low-speed floor** — `START_SPEED` is now 20 Hz so default `MIN_RPM` at 1/16 microstep remains reachable.

### Changed
- **Settings hub** - now includes Motor Config, Calibration, Display, Pedal Settings, Diagnostics, System Info and About.
- **Documentation sync** — README, wiki, test docs, Instructables, status, contributing, implementation, process analysis, and hardware setup now match current NVS storage, DM542T ALM, ADS1115 I2C, `.pio/build-fw`, and generic clone/build commands.

## [2.0.5] - 2026-04-17

### Changed
- **Cross-core atomics centralised** — new `src/app_state.h` / `src/app_state.cpp` are the single source of truth for `g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending`, `g_flashWriting`, `g_screenRedraw`, `g_dir_switch_cache`, `motorConfigApplyPending`. `safety.cpp` / `safety.h`, `storage.cpp` / `storage.h`, and `main.cpp` no longer redeclare them.
- **`volatile` → `std::atomic`** on all cross-core globals (RISC-V SMP safety): `g_estopPending`, `g_estopTriggerMs`, `estopLocked`, `estopResetPending`, `g_uiResetPending` now use `std::atomic<>` with explicit `memory_order_acquire` / `memory_order_release`.
- **`LOG_E` is now always compiled in** (release + debug) so field failures show on serial even in silent release builds. `LOG_W`, `LOG_I`, `LOG_D` remain debug-only.
- **`MIPI_DSI_LANE_BITRATE_MBPS` in `config.h`** — renamed and set to `500u` to reflect the value hardcoded in `display.cpp`; flagged as informational (actual PHY negotiation runs from ESP-IDF / board config).
- **`platformio.ini`** — removed an unused/non-existent private ESP-IDF include path.
- **`lvglTask` boot sequence (`main.cpp`)** — all `lv_timer_handler()` calls and screen updates during boot are now wrapped in `lvgl_lock()` / `lvgl_unlock()`.
- **`speed.cpp` → `motor_set_target_milli_hz()`** — stepper mutex locking for speed updates is now encapsulated inside `motor.cpp` instead of being taken directly from `speed.cpp`.
- **`docs/images/main_screen.svg`** — matches v2.0.4 UI: larger gauge (`MAIN_GAUGE_*`), no main-screen RPM +/-, FONT_HUGE-scale RPM + unit, scale 0–3.0 RPM, **X STOP**, pot hint.

### Added
- **`fatal_halt(const char* reason)`** (`src/app_state.h/cpp`) — logs reason via `LOG_E`, drains serial, then reboots. Replaces direct `ESP.restart()` in `storage.cpp` and `motor.cpp` init paths with diagnosable error strings (e.g. `"storage: NVS namespace open"`, `"motor: FastAccelStepper init"`).
- **Non-blocking ADS1115 pedal ADC** — new `ads_poll_and_start()` state machine in `src/motor/speed.cpp` so `motorTask` never blocks on I2C during its 5 ms loop. Blocking helper reserved for `speed_init()`.
- **Boot-time ESTOP de-floating** — `safety_init()` samples `PIN_ESTOP` three times with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle, requiring ≥2/3 LOW before treating ESTOP as pressed. Protects against false boot ESTOPs from floating GPIO34 (no internal pull-ups on this pin).
- **`motor_set_target_milli_hz()`** public API in `motor.h` (handles `g_stepperMutex` internally + `applySpeedAcceleration()`).
- **Native tests** — 8 new cases for `milli_hz_floor_testable`: zero, negative, NaN, values at / below / above floor, and `UINT32_MAX` saturation. All native tests pass.

### Fixed
- **`motor_disable()`** — added `g_stepperMutex != nullptr` guard (previously could dereference null if called before `motor_init()`).
- **`speed_apply()`** — `motor_is_running()` was called twice per cycle, each acquiring `g_stepperMutex`; now cached in a local at the top of the function.

### Removed
- **Build artefacts / IDE scratch** — `.cache/`, `.venv/`, `.ruff_cache/`, and `compile_commands.json` (11 MB) are now gitignored. Stale `build_log*.txt`, `nm*.txt`, `commit_out.txt`, etc. removed from repo root.

### Documentation
- **STATUS.md**, **README.md**, **AGENTS.md**, **CLAUDE.md**, **CONTRIBUTING.md**, **wiki/Troubleshooting.md**, **wiki/Home.md**, **wiki/Roadmap.md**, **wiki/Architecture.md**, **docs/SAFETY_SYSTEM.md**, **docs/estop_timing.md**, **docs/PROJECT_IMPLEMENTATION.md**, **PROCESS_ANALYSIS.md** updated to reflect `std::atomic` cross-core model, `app_state.h` single source of truth, `fatal_halt()`, ESTOP boot sampling, non-blocking pedal ADC, and the `MIPI_DSI_LANE_BITRATE_MBPS` rename.

## [2.0.4] - 2026-04-12

### Added
- **Theme layout for main gauge** — `MAIN_GAUGE_*`, `MAIN_RPM_TAG_Y`, `MAIN_RPM_VALUE_GAP` / `MAIN_RPM_VALUE_LIFT` / `MAIN_RPM_VALUE_ZOOM` in `theme.h` (no magic numbers in `screen_main.cpp` for gauge geometry).
- **Jog RPM row constants** — `JOG_RPM_*` in `theme.h` for title/value/bar and right-aligned +/- buttons with explicit gap.

### Changed
- **Main screen (`screen_main.cpp`)** — RPM is **pot-only** on this screen: removed `-` / `+` buttons and all `speed_slider_set` from those controls; removed `rpm_buttons_enabled` toggling. Semicircular gauge is larger; `MAIN_GAUGE_CY` adjusted so the arc stays on-screen; RPM numeric label uses `LV_ALIGN_TOP_MID` stacking above **RPM** unit label; value uses `FONT_HUGE` (40 pt max) plus LVGL `transform_zoom` + pivot for a slightly larger readout without loading `montserrat_48`.
- **Jog screen (`screen_jog.cpp`)** — removed the small top-right label that showed `MIN_RPM`–`MAX_RPM` (looked like stray numbers). **+** / **-** are re-laid out (`JOG_RPM_BTN_PLUS_X` / `JOG_RPM_BTN_MINUS_X`) so they no longer overlap; progress bar width tweaked to leave margin before the buttons.
- **Programs list (`screen_programs.cpp`)** — `programsDirty` is `std::atomic<bool>`; `screen_programs_invalidate_widgets()` sets dirty so card list rebuilds after `screens_reinit()`.
- **`control.cpp`** — `currentState` / `previousState` transitions use `memory_order_acquire`, `acq_rel` on CAS, `release` on stores; `controlTask` reads state with `acquire`; transition log line uses the pre-CAS `expected` state (not the atomic object).
- **`SystemSettings` reads** — motor, storage apply/save, calibration, speed direction, theme, display init, LVGL HAL, boot/sysinfo paths take **`g_settings_mutex`** where `g_settings` is touched from UI or across cores (see motor snapshot pattern in `motor.cpp`).
- **Shared UI (`screens.h` / `screens.cpp`)** — reusable header/action-bar helpers and safer child access patterns (e.g. pulse action bar) used across multiple screens.
- **Timer screen** — countdown delay changes do not write settings to NVS until **BACK** (avoids wear and matches operator expectation).

### Fixed
- **Gauge clipping** — first enlarged gauge used a center Y that placed the arc partly above `y=0`; fixed via theme `MAIN_GAUGE_CY`.
- **Jog RPM row** — overlapping +/- touch targets.

### Removed
- **`rpm_buttons_enabled`** — removed from `SystemSettings`, NVS JSON save/load, and native storage roundtrip test; legacy key in an old NVS blob is ignored on deserialize.

### Documentation
- **README**, **wiki** (Home, Roadmap, Architecture, Getting-Started, Troubleshooting), **docs/INSTRUCTABLES.md**, **CONTRIBUTING.md**, **AGENTS.md**, and **CLAUDE.md** updated so operator and contributor text matches v2.0.4 behaviour (main = pot RPM; jog = +/- for jog speed; mutexes, theme constants).

> **Note:** Older sections of this file (e.g. **[0.4.0]** “touchscreen +/- while motor is running”) describe behaviour **before** v2.0.4; the main screen no longer provides those buttons.

## [2.0.3] - 2026-04-12

### Added
- **E-STOP wakes backlight** when the MIPI panel was dimmed (`g_wakePending` in `estopISR` / boot path; `dim_reset_activity()` when the ESTOP overlay is shown).

### Changed
- **Documentation** — README, wiki, and `docs/*.md` aligned with `src/config.h` and current firmware (workpiece RPM limits, `ScreenId` count + ESTOP overlay, E-STOP GPIO sense, NVS-first storage wording, default acceleration from `storage.cpp`, MIPI lane bitrate note in project implementation doc).
- **Documentation & UI copy** — gear notation standardized to **1:108** (motor revolutions per output revolution), matching `GEAR_RATIO` (108) and `docs/images/motor.worm.svg`.

## [2.0.2] - 2026-04-05 (Stability & IWDT Crash Fix)

### Fixed
- **CRITICAL: Interrupt WDT timeout on Core 0** — `g_stepperMutex` was a `portMUX_TYPE` spinlock that disabled interrupts during cross-core contention (Core 1 UI calling `motor_is_running()` while Core 0 motorTask held lock). Replaced with `SemaphoreHandle_t` FreeRTOS mutex across `motor.cpp`, `speed.cpp`, `step_mode.cpp`, `jog.cpp`, and `pulse.cpp`
- **Program Edit crashes on "New Program"** — out-of-bounds array access in mode button loop (`i < 4` instead of `i < modeCount`)
- **Program Edit crashes on SAVE** — dangling static widget pointers after `screens_reinit()`, stale screen data on re-entry
- **Confirm screen crashes on CONFIRM** — invalid `returnScreen` ID, added range validation
- **Settings keyboard self-deletion crash** — `cleanup_kb()` called synchronously from a keyboard ready-callback, deleting the keyboard while LVGL processed its event. Deferred via `kbClosePending` flag
- **BT/Step keyboard crashes** — synchronous `lv_obj_delete()` in event callbacks replaced with `lv_obj_delete_async()`
- **Dangling pointers after `screens_reinit()`** — added `screen_*_invalidate_widgets()` for Step, Programs, ProgramEdit, and other screens with static widgets

### Added
- **Widget invalidation pattern** — each screen with static pointers has `screen_*_invalidate_widgets()` called from `screens_reinit()`
- **`screen_needs_rebuild()`** — forces full screen recreation for edit screens on re-entry
- **Deferred keyboard cleanup** — `*ClosePending` flags in settings-related screens, cleanup runs in `screen_*_update()` cycle

## [2.0.1] - 2026-04-03

### Added
- **Invert Direction** — Motor Config INVERT DIRECTION toggle persists in settings and applies in `speed_get_direction()` for all modes (continuous, jog, pulse, step)

### Changed
- **Jog screen redesign** — replaced single JOG button with CW/CCW hold buttons and RPM progress bar
- **Display Settings scaling** — rows 40→52px, buttons 28→36px, fonts bumped up for readability
- **Motor Config** — renamed ENABLE ON IDLE to DIR SWITCH
- **UI polish** — consistent fonts/sizes across all settings sub-screens

### Fixed
- **Countdown back button** — removed unnecessary pending pattern that could cause blue flash on screen switch

## [2.0.0] - 2026-04-03 (Full UI redesign)

### Added
- **Foot pedal support** — analog speed control (ADS1115 I2C ADC) + digital start switch (GPIO33)
- **Direction switch** — physical CW/CCW toggle on GPIO29
- **Settings hub screen** — Display, System Info, Calibration, Motor Config, About
- **Display Settings** — brightness slider, dim timeout configuration
- **System Info screen** — live CPU core load, free heap, PSRAM usage, uptime
- **Motor Config screen** — microstepping, acceleration, direction switch toggle, pedal enable
- **About screen** — firmware version, hardware info
- **8 accent color themes** — switchable from Display Settings
- **Program Edit screen** — full preset editor with on-screen keyboard
- **Large screen set** with lazy creation pattern (only boot/main/confirm created at init)
- **Board bring-up** — GUITION ESP32-P4 + companion MCU pin routing documented in hardware guides
- **`sdkconfig.defaults`** — display, touch, motor, and storage-oriented defaults for the P4 build

### Changed
- **UI redesign** — consistent footer navigation, settings grid layout, accent theme system
- **RTOS task priorities** — lvglTask raised to pri 2, storageTask lowered to pri 1 (prevents blue flash during flash writes)
- **storageTask stack** — increased from 8KB to 12KB (serializeJson + LittleFS write needs more stack)
- **`storage_flush()` debounce** — 500ms for presets, 1000ms for settings
- **`control_transition_to()`** — CAS (compare-and-swap) pattern for race-free state transitions
- **`currentState`** — now `std::atomic<SystemState>` for cross-core safety
- **`g_presets`** — protected by FreeRTOS mutex `g_presets_mutex`
- **`storage_get_preset()`** — returns copy via output parameter (never pointer into vector)
- **Settings screen items** — scaled up: 52px height, font 18, gap 5px
- **Gear ratio (docs, superseded)** — interim text used 199.5:1; current model is **1:108** total (NMRV030 60:1 x spur 72/40, `GEAR_RATIO`=108 in `config.h`)
- **LVGL** — upgraded from 9.5.0 to 9.6.0
- **FastAccelStepper** — upgraded from 0.33.14 to 0.33.x (latest)
- **Pinout** — ESTOP moved from GPIO33 to GPIO34 (internal pull-up), DIR SWITCH moved from GPIO28 to GPIO29 (C6 UART conflict)
- **GPIO 28/32** — documented as reserved for C6 co-processor UART (cannot be used as GPIO)

### Fixed
- **Companion-bus traffic** — periodic notifications to the on-board secondary MCU were too aggressive; rate-limited to 500ms to avoid garbled callbacks
- **Off-UI command path** — volatile pending flags required; no direct motor calls from I/O callbacks
- **Subsystem startup ordering** — corrected initialization order for P4 + companion MCU interface bring-up
- **Keyboard crash** — creating textarea/keyboard inside scroll panel caused crash. Must create on screen root AND hide scroll panel.
- **`lv_obj_set_flex_gap`** — does not exist in LVGL 9, removed
- **`lv_obj_set_flex_align`** — requires 3 args in LVGL 9 (main, cross, track_align)
- **`lv_font_montserrat_22`** — does not exist, use `lv_font_montserrat_20`
- **`lv_display_set_rotation()`** — crashes ESP32-P4, manual rotation in flush callback only
- **`ledc_set_duty_and_update()`** — crashes ESP32-P4 backlight, use separate `ledc_set_duty()` + `ledc_update_duty()`
- **`LittleFS.rename()`** — crashes on ESP32-P4, reverted to direct FILE_WRITE
- **`lv_obj_set_style_overflow_visible`** — does not exist in LVGL 9
- **LVGL keyboard cleanup from callback** — causes crash, must use deferred flag pattern
- **`lv_slider_set_value()`** triggers `LV_EVENT_VALUE_CHANGED` — added `ignoreSliderCb` flag
- **`control_transition_to()` hard block** — prevented ALL transitions FROM ESTOP, removed
- **`g_dir_switch_cache`** — was not updated when dir switch toggled in Motor Config
- **`screens_reinit()`** — must destroy estop overlay before deleting screen roots
- **`screens_init()`** — must create SCREEN_CONFIRM root before `screen_confirm_create_static()`
- **`step_execute()`** — only valid from STATE_IDLE, not STATE_STEP

## [1.3.0] - 2026-03-31 (Accent Themes, Settings, Motor Tuning)

### Added
- **Accent theme system** — 8 color themes switchable from Display Settings
- **Settings grid screen** — hub with icons for all configuration sub-screens
- **Motor tuning UI** — microstepping, acceleration, calibration from touchscreen
- **DM542T driver preparation** — configuration support for higher RPM range

### Changed
- **UI polish** — consistent styling, improved button layout, footer navigation

## [1.0.0] - 2026-03-31 (Stable Release)

### Added
- **Program preset storage** — ArduinoJson + LittleFS, 16 slots, save/load/delete from UI
- **Custom Program Presets from UI** — create/edit/delete programs directly on device
- **Health monitoring** — stack watermark, heap, LVGL memory tracking
- **Boot-safe ENA pin** — motor disabled on startup

### Changed
- **Firmware version** — bumped to v1.0.0 (stable)
- **Documentation** — professional README with wiring diagrams, pinouts, BOM

## [0.4.0] - 2026-03-31 (Motor Control & Hardware Validation)

### Added
- **Live RPM adjustment during rotation** — potentiometer and touchscreen +/- buttons work while motor is running
- **Thread-safe cross-core speed updates** — `volatile` shared variables + `speed_request_update()` flag pattern
- **`applySpeedAcceleration()`** call in `speed_apply()` — required by FastAccelStepper 0.33.x
- **ISR IRAM-safe build flags** — `CONFIG_RMT_ISR_IRAM_SAFE=1` and `CONFIG_GPTIMER_ISR_IRAM_SAFE=1`
- **Linear acceleration phase** — `setLinearAcceleration(200)` for NEMA 23 resonance zone
- **Microstepping selection** — UI supports 1/4, 1/8, 1/16, and 1/32
- **MIN_RPM lowered to 0.05** — 2-decimal precision for fine low-speed control
- **RPM buttons enabled by default** — `rpm_buttons_enabled` defaults to `true`
- **ADC pot-override threshold increased** — from 80 to 200 ADC counts

### Changed
- **ADC potentiometer reference** — calibrated to 3315 (10k pot on ESP32-P4 ADC with 11dB attenuation)
- **RPM gauge range** — dynamically calculated from `MAX_RPM * 100`
- **MAX_RPM** — temporarily set to 1.0 RPM pending DM542T driver upgrade
- **Gear ratio (docs)** — clarified NMRV030 + spur as **1:108** total (`GEAR_RATIO` in `config.h`)
- **FastAccelStepper** — upgraded from 0.31.x to 0.33.14
- **LVGL** — upgraded from 8.x to 9.5.0

### Fixed
- **Speed not updating during rotation** — missing `applySpeedAcceleration()` after `setSpeedInMilliHz()`
- **Cross-core cache coherency** — variables marked `volatile` for Core 0/1 visibility
- **Potentiometer dead zone** — ADC reference was 3000 (27% of range unusable)
- **RPM buttons ignored** — `rpm_buttons_enabled` guard prevented activation
- **`-mfix-esp32-psram-cache-issue`** — removed Xtensa-specific flag incompatible with ESP32-P4 RISC-V
- **`DRIVER_RMT`** — not available on ESP32-P4

## [0.1.0] - 2026-03-21 (MVP Release)

### Added
- Complete platform migration from ESP32-S3 to ESP32-P4 (GUITION JC4880P433C 4.3" MIPI-DSI)
- High-performance MIPI-DSI ST7701S driver integration
- FastAccelStepper integration using hardware RMT for precision NEMA 23 3Nm torque
- Redesigned "Dark Industrial" LVGL 8.x UI with center RPM gauge and glove-safe buttons
- 5 welding modes: Continuous, Jog, Pulse, Step, Timer
- Hardware E-STOP integration with `forceStop()`
- GitHub Actions CI for automated build checks

### Changed
- RPM calculation formulas updated for 60:1 gear ratio
- Real-time UI RPM feedback via `getCurrentSpeedInMilliHz()`
- PlatformIO environments split into release and debug

### Fixed
- Incorrect display driver dependencies removed
- `qio_opi` memory flag for PSRAM cache block on ESP32-P4
- Legacy WT32-SC01 planning files removed
