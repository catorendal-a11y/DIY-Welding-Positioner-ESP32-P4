# Roadmap

## v2.0.7 — Current Release (dark/light UI + header contrast)

### v2.0.7 (2026-04-27)
- [x] **Display Settings > UI MODE** — DARK (POST industrial) vs LIGHT (warm cream HMI reference), persisted as NVS `cfg` JSON **`color_scheme`** (`0`/`1`)
- [x] Runtime neutral palettes (`NEUT_DARK` / `NEUT_LIGHT`) + **`COL_HDR_MUTED`** for secondary header text on the dark header bar
- [x] README, STATUS, wiki, implementation docs, UI SVG sync

## v2.0.6 — diagnostics and preset polish

### v2.0.6 (2026-04-26)
- [x] Diagnostics screen with live GPIO/fault/motion-block state
- [x] RAM event log for START/STOP, pedal, program, state and fault events
- [x] Pedal Settings screen for GPIO33 arm/disarm and ADS1115 status
- [x] ProgramExecutor applies direction, workpiece diameter, pulse cycles, step repeats/dwell, soft start and auto-stop timer fields
- [x] Main START race fix with overwrite command queue and stale STOP cleanup
- [x] Workpiece diameter stored per STEP preset
- [x] README, wiki, status and UI SVG documentation sync

## v2.0.5 — cross-core & error-handling cleanup

### v2.0.5 (2026-04-17)
- [x] All cross-core `std::atomic` flags centralised in `src/app_state.h` / `app_state.cpp` (single source of truth)
- [x] `fatal_halt("<context>")` utility replaces raw `ESP.restart()` in storage/motor init paths (logs reason before reboot)
- [x] `LOG_E` always compiled in (release + debug) so field failures are serial-visible
- [x] Remaining `volatile` cross-core flags converted to `std::atomic` with explicit memory ordering
- [x] Non-blocking ADS1115 pedal ADC path (state-machine poll, never stalls 5 ms motor loop)
- [x] Boot-time ESTOP de-floating (3-sample majority vote on `INPUT_PULLUP` settle)
- [x] `motor_set_target_milli_hz()` encapsulates stepper mutex for non-motor callers
- [x] `lvglTask` boot sequence wrapped in `lvgl_lock()` / `lvgl_unlock()`
- [x] Native tests extended (`milli_hz_floor` edge cases)
- [x] Repo hygiene (`compile_commands.json`, `.cache/`, `.venv/` gitignored)

## v2.0.4 — main UI, jog, SMP

### v2.0.4 (2026-04-12)
- [x] Main screen pot-only RPM gauge (no +/-), larger gauge, centered RPM display
- [x] Jog screen RPM row cleanup (no stray range label; non-overlapping +/-)
- [x] Control state memory ordering; `programsDirty` atomic; `g_settings_mutex` hardening; `rpm_buttons_enabled` removed

## v2.0.3 — Operator UX + docs

### v2.0.3 (2026-04-12)
- [x] E-STOP wakes dimmed display (`g_wakePending`, `dim_reset_activity()` on overlay)
- [x] Documentation pass — README / wiki / `docs` aligned with `config.h` and NVS storage

## v2.0.2 — Stability & IWDT Crash Fix

### v2.0.2 Fixes (2026-04-05)
- [x] FreeRTOS mutex for stepper (replaced spinlock — fixed IWDT crash)
- [x] LVGL async object deletion (lv_obj_delete_async for keyboard/numpad)
- [x] Screen widget invalidation pattern (Step, Programs, ProgramEdit)
- [x] Deferred keyboard cleanup (*ClosePending flags)
- [x] Confirm dialog validation (returnScreen range check)
- [x] Program Edit crash fixes (out-of-bounds, dangling pointers)

### v2.0.1 Fixes (2026-04-03)
- [x] Invert direction toggle (persisted in settings)
- [x] Jog screen CW/CCW redesign
- [x] Display/Motor Config UI polish

### v2.0.0 Features (2026-04-03)
- [x] 21 active LVGL root screens + E-STOP overlay (settings, diagnostics, pedal settings, system info, calibration, motor config, etc.)
- [x] Direction switch (GPIO29, toggle via settings)
- [x] Foot pedal support (analog speed + digital start)
- [x] Thread-safe stepper access (FreeRTOS mutex + atomic variables)
- [x] CAS state transitions (race-free)
- [x] ESTOP reset via Core 0 pending pattern
- [x] Storage reliability (mutex, debounce, copy-based API)
- [x] 8 accent color themes with live switching
- [x] Display settings (brightness slider, dim timeout)
- [x] System info screen (core load, heap, PSRAM, uptime)
- [x] Diagnostics screen (live ESTOP/ALM/DIR/pedal/ENA/RPM status)
- [x] Pedal settings screen (GPIO33 arm/disarm + ADS1115 status)
- [x] Motor configuration UI (microstepping, acceleration, calibration)
- [x] Countdown before start (configurable 1-10s delay with visual countdown)
- [x] All 5 welding modes tested on hardware
- [x] E-STOP validated
- [x] Program presets (16 slots, full CRUD from UI)
- [x] Live RPM adjustment (pot + buttons + pedal during rotation)

### Historical
- [x] v0.4.0: Live speed control, ISR IRAM-safe flags, microstepping
- [x] v1.3.0: Accent theme system, settings grid, motor tuning

## Next

- [ ] Increase MAX_RPM to 5.0 with DM542T
- [ ] Enclosure design files (3D printable)
- [ ] Assembly guide
- [ ] Bidirectional state sync (phone mirrors display)
