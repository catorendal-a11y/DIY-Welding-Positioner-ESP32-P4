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
