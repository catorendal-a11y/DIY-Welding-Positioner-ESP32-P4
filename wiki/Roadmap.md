# Roadmap

## v0.4.0 — Current Release (Hardware Validated)

- [x] Live RPM adjustment during rotation (pot + buttons)
- [x] Thread-safe cross-core speed updates
- [x] ISR IRAM-safe RMT/GPTIMER flags
- [x] Microstepping selection (1/4, 1/8, 1/16, 1/32)
- [x] Linear acceleration for resonance traversal
- [x] All 5 welding modes tested on hardware
- [x] Program preset save/load
- [x] E-STOP validated

## v0.5.0 — DM542T Driver Integration

- [ ] DM542T driver support (anti-resonance DSP)
- [ ] Increase MAX_RPM to 5.0 (current 1.0 limited by TB6600 resonance)
- [ ] Auto-detect connected driver type
- [ ] Driver-specific acceleration profiles
- [ ] Stall detection (current feedback monitoring)

## v0.6.0 — Enhanced Welding Features

- [ ] Weld pass counter (total welds completed)
- [ ] Per-preset weld time accumulator
- [ ] Auto-reverse mode (alternating CW/CCW per pass)
- [ ] Multi-pass program chaining (execute preset sequence)
- [ ] Pause/resume during welding

## v0.7.0 — Connectivity

- [ ] ESP32-C6 co-processor for Wi-Fi
- [ ] Web panel for remote monitoring and control
- [ ] OTA firmware updates
- [ ] Bluetooth LE for mobile app communication
- [ ] WebSocket real-time telemetry

## v0.8.0 — Advanced Control

- [ ] PID-based speed regulation with encoder feedback
- [ ] Adaptive acceleration based on load
- [ ] Torque limiting mode
- [ ] Position tracking (absolute position with encoder)
- [ ] Weld seam angle presets (0-360 degrees)

## v0.9.0 — Multi-Axis

- [ ] Second axis support (tilt/height control)
- [ ] Coordinated multi-axis motion
- [ ] Synchronized rotation + wire feed control
- [ ] TIG cold wire feeder integration

## v1.0.0 — Production Release

- [ ] Enclosure design files (3D printable)
- [ ] Manufacturing BOM with sourcing links
- [ ] Assembly guide with photos
- [ ] CE/FCC compliance notes
- [ ] Full test suite and validation report
- [ ] User manual

## Long-Term Ideas

- [ ] CAN bus communication between multiple controllers
- [ ] MQTT integration for factory automation
- [ ] Touchscreen calibration wizard
- ] Multi-language UI support
- ] Custom gauge themes
