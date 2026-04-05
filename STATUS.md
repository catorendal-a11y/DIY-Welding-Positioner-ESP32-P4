# Project Status

**Last Updated:** 2026-04-05
**Firmware:** v2.0.2
**Build:** SUCCESS (Release & Debug, 0 errors 0 warnings)

---

## Completed

### Core Motor Control
- [x] **ESP32-P4 MIPI-DSI display** (ST7701S 480x800, RGB565, landscape rotation)
- [x] **GT911 capacitive touch** (I2C, coordinate mapping)
- [x] **LVGL 9.6 UI framework** (800x480 landscape, 23 screens)
- [x] **FastAccelStepper motor control** (hardware RMT pulses, v0.33.x)
- [x] **FreeRTOS dual-core architecture** (Core 0: Motor/Safety, Core 1: UI)
- [x] **5 welding modes:** Continuous, Jog, Pulse, Step, Timer
- [x] **Live RPM adjustment** (potentiometer + touchscreen buttons during rotation)
- [x] **Thread-safe cross-core speed updates** (atomic + request flag pattern, FreeRTOS mutex for stepper)
- [x] **`applySpeedAcceleration()`** for immediate speed changes during running
- [x] **Linear acceleration phase** (resonance-zone traversal)
- [x] **Microstepping selection** (1/4, 1/8, 1/16, 1/32)
- [x] **ISR IRAM-safe RMT/GPTIMER flags**
- [x] **ADC potentiometer** (IIR filtering, 0-3315 ADC range, 200-count override threshold)

### Safety
- [x] **Hardware E-STOP** (GPIO34, NC contact, <0.5ms response)
- [x] **Task Watchdog Timer** (motor & safety tasks)
- [x] **Boot-safe ENA pin** (motor disabled on startup)
- [x] **CAS state transitions** (race-free between safetyTask and controlTask)
- [x] **E-STOP UI overlay** (full-screen red, blocks all interaction)
- [x] **UI reset from ESTOP** (via Core 0 pending flag pattern)

### Connectivity
- [x] **BLE remote control** (NimBLE NUS via ESP32-C6 co-processor)
  - [x] Arm/start/stop/direction/reverse commands
  - [x] Passkey security (123456)
  - [x] Rate-limited notifications (500ms)
  - [x] Pending-flag pattern (no direct motor calls from BLE thread)
- [x] **WiFi connectivity** (network scanning, credential storage, hidden networks)
  - [x] On-screen keyboard for SSID/password
  - [x] WiFi status in System Info
  - [x] C6 co-processor OTA updates (v2.3.2 to v2.11.6)
- [x] **ESP-Hosted SDIO transport** (WiFi + BLE share bus to C6)

### UI/UX
- [x] **23 screens** with lazy creation pattern
- [x] **8 accent color themes** (switchable from Display Settings)
- [x] **Settings hub** (WiFi, Bluetooth, Display, System Info, Calibration, Motor Config, About)
- [x] **Display Settings** (brightness slider, dim timeout)
- [x] **System Info** (CPU core load, heap, PSRAM, WiFi status, uptime)
- [x] **Motor Config** (microstepping, acceleration, direction switch, pedal enable)
- [x] **About screen** (firmware version, hardware info)
- [x] **Program Edit** (full preset editor with on-screen keyboard)
- [x] **Consistent footer navigation** and back buttons

### Storage
- [x] **Program preset storage** (ArduinoJson + LittleFS, 16 slots)
- [x] **Settings persistence** (motor config, WiFi credentials, display settings)
- [x] **Mutex-protected presets** (`g_presets_mutex`)
- [x] **Debounced flash writes** (500ms presets, 1000ms settings)

### Hardware
- [x] **Foot pedal support** (analog speed GPIO35, digital switch GPIO33)
- [x] **Direction switch** (GPIO29, CW/CCW toggle)
- [x] **Gear ratio 199.5:1** (60 x 133 / 40)

### Documentation
- [x] **README** (v2.0.0, complete feature list, wiring diagram, BOM)
- [x] **Wiki** (Home, Getting Started, Hardware Setup, Troubleshooting, Roadmap, Architecture)
- [x] **docs/** (Hardware Setup, Safety System, EMI Mitigation, Implementation, Instructables)
- [x] **Wiring diagram v2** (SVG, GPIO29 on correct side, clean cable routing)

---

### Stability & Robustness (v2.0.2)
- [x] **FreeRTOS mutex for stepper** (replaced portMUX_TYPE spinlock — fixed IWDT crash on cross-core contention)
- [x] **LVGL async object deletion** (lv_obj_delete_async for keyboard/numpad cleanup from event callbacks)
- [x] **Screen widget invalidation** (invalidate_widgets pattern for WiFi, BT, Step, Programs, ProgramEdit screens)
- [x] **Deferred keyboard cleanup** (*ClosePending flags, actual cleanup in update cycle)
- [x] **Screen reinit safety** (screens_reinit calls invalidate_widgets for all screens with static pointers)
- [x] **Confirm dialog validation** (returnScreen range check prevents invalid screen navigation)

## In Progress

- [ ] **DM542T driver integration** (anti-resonance DSP for wider RPM range, up to 5.0 RPM)
- [ ] **Field testing with real weld parameters**

## Planned

- [ ] **Enclosure design** (3D printable)
- [ ] **Assembly guide**
- [ ] **WiFi SoftAP mode** (standalone network without router)
- [ ] **Web-based remote control** (hosted on C6 co-processor)

---

## Build Info

| Metric | Value |
|--------|-------|
| **Platform** | pioarduino (ESP-IDF 5.5.x) |
| **Board** | GUITION JC4880P443C (ESP32-P4 + ESP32-C6) |
| **RAM Usage** | ~13% (41 KB / 320 KB) |
| **Flash Usage** | ~27% (1.8 MB / 6.5 MB) |
| **FastAccelStepper** | 0.33.x |
| **LVGL** | 9.5.0 (RGB565) |
| **ArduinoJson** | 7.4.3 |
| **BLE** | NimBLE via ESP-Hosted (C6 co-processor) |
| **WiFi** | ESP-Hosted SDIO transport |

---

## Pinout

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 50 | STEP | RMT pulse to TB6600 PUL+ |
| GPIO 51 | DIR | Direction to TB6600 DIR+ |
| GPIO 52 | ENABLE | Active LOW to TB6600 EN+ |
| GPIO 49 | POT | 10k speed potentiometer (ADC) |
| GPIO 29 | DIR SWITCH | CW/CCW toggle, INPUT_PULLUP |
| GPIO 34 | E-STOP | NC contact, interrupt, active LOW |
| GPIO 35 | PEDAL POT | Foot pedal speed (ADC) |
| GPIO 33 | PEDAL SW | Foot pedal switch, active LOW |
| GPIO 7/8 | Touch I2C | GT911 (internal) |
| GPIO 28/32 | C6 UART | Reserved -- do not use |
| GPIO 14-19, 54 | C6 SDIO | ESP-Hosted transport -- do not use |
