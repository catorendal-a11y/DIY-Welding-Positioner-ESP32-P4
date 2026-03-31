# Project Status

**Last Updated:** 2026-03-31
**Firmware:** v0.4.0
**Build:** SUCCESS (Release & Debug)

---

## Completed

- [x] **ESP32-P4 MIPI-DSI display fully working** (ST7701S 480x800, RGB565, landscape rotation)
- [x] **GT911 capacitive touch validated** (I2C, coordinate mapping working)
- [x] **LVGL 9.x UI framework integration** (800x480 landscape, all screens responsive)
- [x] **Navigation:** Back buttons added to all sub-screens (Step, Jog, Timer, Programs)
- [x] FastAccelStepper motor control (hardware RMT pulses, v0.33.14)
- [x] FreeRTOS dual-core task architecture (Core 0: Motor/Safety, Core 1: UI)
- [x] 5 welding modes: Continuous, Jog, Pulse, Step, Timer
- [x] Hardware E-STOP interrupt (GPIO 33, NC logic)
- [x] Task Watchdog Timer (TWDT) on motor & safety tasks
- [x] ADC potentiometer speed control with IIR filtering (tested, full range 0-3315 ADC)
- [x] **Live RPM adjustment during rotation** (potentiometer + touchscreen buttons)
- [x] **Thread-safe cross-core speed updates** (volatile shared variables + request flag)
- [x] **applySpeedAcceleration()** for immediate speed changes during running state
- [x] Health monitoring (stack watermark, heap, LVGL memory)
- [x] Boot-safe ENA pin (motor disabled on startup)
- [x] Professional README with wiring diagrams, pinouts, BOM
- [x] EMI Mitigation Guide (docs/EMI_MITIGATION.md)
- [x] External hardware setup guide (docs/HARDWARE_SETUP.md)
- [x] Safety System Documentation (docs/SAFETY_SYSTEM.md)
- [x] Full Code Audit & v0.3.0-beta Release (Critical bugs & warnings fixed)
- [x] Program Preset Storage (ArduinoJson + LittleFS)
- [x] **Project Implementation Documentation** (docs/PROJECT_IMPLEMENTATION.md - complete development log)
- [x] **Custom Program Presets from UI** (Create/Edit/Delete programs directly on device)
- [x] **ISR IRAM-safe RMT/GPTIMER flags** (CONFIG_RMT_ISR_IRAM_SAFE, CONFIG_GPTIMER_ISR_IRAM_SAFE)
- [x] **Microstepping selection** (1/4, 1/8, 1/16, 1/32 configurable from UI)
- [x] **Linear acceleration phase** (setLinearAcceleration for resonance-zone traversal)
- [x] **On-hardware testing validated** — all modes, speed control, E-STOP, presets confirmed working

## In Progress

- [ ] DM542T driver integration (anti-resonance DSP for wider RPM range)
- [ ] Field testing with real weld parameters

## Planned

- [ ] Wi-Fi / Web panel remote control (ESP32-C6)
- [ ] OTA firmware updates

---

## Build Info

| Metric | Value |
|--------|-------|
| **Platform** | pioarduino (ESP-IDF 5.5.x) |
| **Board** | esp32-p4 (Waveshare/Guition JC4880P433) |
| **RAM Usage** | ~9.3% (30 KB / 328 KB) |
| **Flash Usage** | ~15.5% (1014 KB / 6.5 MB) |
| **FastAccelStepper** | 0.33.14 |
| **LVGL** | 9.5.0 (RGB565) |
| **ArduinoJson** | 7.4.3 |

---

## Key Achievements (v0.4.0)

### Motor Control
- Live speed adjustment via pot and buttons during continuous rotation
- Thread-safe cross-core communication (volatile + request flag pattern)
- applySpeedAcceleration() for immediate speed changes on running motor
- Linear acceleration phase to traverse NEMA 23 resonance zone (100-300 motor RPM)
- ISR IRAM-safe flags preventing PSRAM cache miss in RMT/GPTIMER interrupts
- Microstepping support: 1/4, 1/8, 1/16, 1/32

### Display & Touch
- ST7701S MIPI-DSI running at 34MHz, 2-lane, 500Mbps
- RGB565 color format (16-bit) for optimal performance
- 90-degree manual rotation for landscape (800x480)
- GT911 touch with proper coordinate mapping
- Vsync-based rendering for smooth updates
- Custom ST7701 initialization sequence from JC4880P433C BSP

### UI/UX
- All screens scaled properly for 4.3" display
- Consistent navigation with back buttons
- Touch-responsive controls
- Real-time status updates
- RPM gauge with dynamic range (MAX_RPM * 100)
- 2-decimal RPM display for precision at low speeds

### Safety & Reliability
- Hardware E-STOP with interrupt priority
- Task watchdog protection
- Stack overflow protection (64KB for LVGL task)
- Cache-safe memory allocation for LVGL buffers in PSRAM
