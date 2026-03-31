# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-03-21 (MVP Release)

### Added
- Complete platform migration from ESP32-S3 (WT32-SC01 Plus) to the cutting-edge ESP32-P4 (Waveshare/Guition 4.3" MIPI-DSI).
- High-performance MIPI-DSI ST7701S driver integration.
- FastAccelStepper integration using hardware RMT for precision NEMA 23 3Nm torque delivery.
- Redesigned "Dark Industrial" LVGL 8.x UI with center RPM gauge and glove-safe buttons.
- Modes: Continuous, Jog, Pulse, Step, and Timer modes dynamically linked to UI.
- Safety: Hardware E-STOP integration tied directly to FastAccelStepper `forceStop()`.
- Initial automated GitHub Action CI (`pio-build.yml`) for automated C/C++ compilation checks. 

### Changed
- RPM calculation formulas updated to perfectly reflect new 60:1 gear ratio hardware.
- Real-time UI RPM feedback switched to FastAccelStepper `getCurrentSpeedInMilliHz()`. 
- `platformio.ini` environment settings split into `[env:esp32p4-debug]` and `[env:esp32p4-release]`.

### Fixed
- Incorrect display driver dependencies removed from `idf_component.yml`.
- `qio_opi` memory flag injected into `platformio.ini` to properly structure the PSRAM cache block on ESP32-P4.
- Removed legacy WT32-SC01 planning files and tech debt to clean up the repository.

## [0.4.0] - 2026-03-31 (Motor Control & Hardware Validation)

### Added
- **Live RPM adjustment during rotation** — potentiometer and touchscreen +/- buttons work while motor is running.
- **Thread-safe cross-core speed updates** — `volatile` shared variables + `speed_request_update()` flag pattern prevents cache coherency issues between Core 0 (motor) and Core 1 (UI).
- **`applySpeedAcceleration()`** call in `speed_apply()` — required by FastAccelStepper 0.33.x for speed changes to take effect during ongoing rotation.
- **ISR IRAM-safe build flags** — `CONFIG_RMT_ISR_IRAM_SAFE=1` and `CONFIG_GPTIMER_ISR_IRAM_SAFE=1` prevent PSRAM cache miss in RMT/GPTIMER interrupts on ESP32-P4.
- **Linear acceleration phase** — `setLinearAcceleration(200)` provides smooth ramp through NEMA 23 resonance zone (100-300 motor RPM).
- **Microstepping selection** — UI supports 1/4, 1/8, 1/16, and 1/32 microstepping (configurable from Settings screen).
- **MIN_RPM lowered to 0.05** — gauge and RPM display now support 2-decimal precision for fine low-speed control.
- **RPM buttons enabled by default** — `rpm_buttons_enabled` defaults to `true` in storage settings.
- **ADC pot-override threshold increased** — from 80 to 200 ADC counts to prevent false override from potentiometer noise.

### Changed
- **ADC potentiometer reference** — calibrated to 3315 (matching measured 10k pot range on ESP32-P4 ADC with 11dB attenuation).
- **RPM gauge range** — now dynamically calculated from `MAX_RPM * 100` for proper 2-decimal display.
- **MAX_RPM** — temporarily set to 1.0 RPM pending DM542T driver upgrade (TB6600 resonance limitations).
- **Gear ratio** — corrected from 60:1 to 108:1 throughout documentation.
- **FastAccelStepper** — upgraded from 0.31.x to 0.33.14.
- **LVGL** — upgraded from 8.x to 9.5.0.
- **Firmware version** — bumped to v0.4.0.

### Fixed
- **Speed not updating during rotation** — root cause was missing `applySpeedAcceleration()` call after `setSpeedInMilliHz()`.
- **Cross-core cache coherency** — `sliderRPM`, `buttonsActive`, `lastPotAdc` marked `volatile` for visibility between Core 0 and Core 1.
- **Potentiometer dead zone** — ADC reference was 3000 (27% of range unusable). Calibrated to match actual pot output.
- **RPM buttons ignored** — `speed_slider_set()` guard on `rpm_buttons_enabled` prevented activation when setting was false.
- **`-mfix-esp32-psram-cache-issue` build error** — removed Xtensa-specific flag incompatible with ESP32-P4 RISC-V architecture.
- **`DRIVER_RMT` compile error** — not available on ESP32-P4 (only RMT supported, selected automatically).
