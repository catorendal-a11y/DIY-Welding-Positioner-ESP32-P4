# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Documentation & UI copy** — gear notation standardized to **1:108** (motor revolutions per output revolution), matching `GEAR_RATIO` (108) and `docs/images/motor.worm.svg`.

## [2.0.2] - 2026-04-05 (Stability & IWDT Crash Fix)

### Fixed
- **CRITICAL: Interrupt WDT timeout on Core 0** — `g_stepperMutex` was a `portMUX_TYPE` spinlock that disabled interrupts during cross-core contention (Core 1 UI calling `motor_is_running()` while Core 0 motorTask held lock). Replaced with `SemaphoreHandle_t` FreeRTOS mutex across `motor.cpp`, `speed.cpp`, `step_mode.cpp`, `jog.cpp`, and `pulse.cpp`
- **Program Edit crashes on "New Program"** — out-of-bounds array access in mode button loop (`i < 4` instead of `i < modeCount`)
- **Program Edit crashes on SAVE** — dangling static widget pointers after `screens_reinit()`, stale screen data on re-entry
- **Confirm screen crashes on CONFIRM** — invalid `returnScreen` ID, added range validation
- **WiFi keyboard self-deletion crash** — `cleanup_kb()` called synchronously from `wifi_kb_cb()`, deleting the keyboard while LVGL processed its event. Deferred via `kbClosePending` flag
- **BT/Step keyboard crashes** — synchronous `lv_obj_delete()` in event callbacks replaced with `lv_obj_delete_async()`
- **Dangling pointers after `screens_reinit()`** — added `screen_*_invalidate_widgets()` functions for WiFi, BT, Step, Programs, and ProgramEdit screens

### Added
- **Widget invalidation pattern** — each screen with static pointers has `screen_*_invalidate_widgets()` called from `screens_reinit()`
- **`screen_needs_rebuild()`** — forces full screen recreation for edit screens on re-entry
- **Deferred keyboard cleanup** — `*ClosePending` flags in WiFi/BT/Step screens, cleanup runs in `screen_*_update()` cycle

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

## [2.0.0] - 2026-04-03 (BLE, WiFi, Full UI Redesign)

### Added
- **BLE remote control** via Nordic UART Service (NUS) on ESP32-C6 co-processor
  - Arm/start/stop/direction/reverse commands from phone BLE apps
  - Security with passkey 123456, ARM command required before START
  - State + RPM notifications rate-limited to 500ms (prevents SDIO saturation)
- **WiFi connectivity** with network scanning, credential storage, hidden network support
  - On-screen keyboard for SSID/password entry
  - WiFi status in System Info screen
  - OTA firmware updates for C6 co-processor
- **Foot pedal support** — analog speed control (ADS1115 I2C ADC) + digital start switch (GPIO33)
- **Direction switch** — physical CW/CCW toggle on GPIO29
- **Settings hub screen** with 7 sub-screens: WiFi, Bluetooth, Display, System Info, Calibration, Motor Config, About
- **Display Settings** — brightness slider, dim timeout configuration
- **System Info screen** — live CPU core load, free heap, PSRAM usage, WiFi status, uptime
- **Motor Config screen** — microstepping, acceleration, direction switch toggle, pedal enable
- **About screen** — firmware version, hardware info
- **8 accent color themes** — switchable from Display Settings
- **Program Edit screen** — full preset editor with on-screen keyboard
- **23 total screens** with lazy creation pattern (only boot/main/confirm created at init)
- **ESP-Hosted SDIO transport** — C6 co-processor firmware updated from v2.3.2 to v2.11.6
- **`sdkconfig.defaults`** — complete esp-hosted, BLE (NimBLE), WiFi remote configuration

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
- **BLE data corruption on P4** — `ble_update()` was flooding SDIO with notify() every loop, causing garbled write callbacks. Rate-limited to 500ms.
- **BLE write callbacks** — must use volatile pending flags, not direct control function calls
- **WiFi init order** — `WiFi.begin()` must be called before `BLEDevice::init()` (shared SDIO transport)
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
