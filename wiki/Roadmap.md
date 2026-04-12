# Roadmap

## v2.0.4 — Current Release (main UI, jog, SMP)

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
- [x] 19 LVGL root screens + E-STOP overlay (settings, system info, calibration, motor config, etc.)
- [x] Direction switch (GPIO29, toggle via settings)
- [x] Foot pedal support (analog speed + digital start)
- [x] Thread-safe stepper access (FreeRTOS mutex + atomic variables)
- [x] CAS state transitions (race-free)
- [x] ESTOP reset via Core 0 pending pattern
- [x] Storage reliability (mutex, debounce, copy-based API)
- [x] 8 accent color themes with live switching
- [x] Display settings (brightness slider, dim timeout)
- [x] System info screen (core load, heap, PSRAM, uptime)
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
