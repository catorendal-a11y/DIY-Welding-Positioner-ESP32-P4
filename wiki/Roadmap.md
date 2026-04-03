# Roadmap

## v2.0.0 — Current Release

### Completed
- [x] BLE remote control (NUS service, phone app, passkey 123456)
- [x] WiFi connectivity (STA mode, scan, connect, credentials)
- [x] 23 UI screens (settings, system info, calibration, motor config, etc.)
- [x] Direction switch (GPIO29, toggle via settings)
- [x] Foot pedal support (analog speed + digital start)
- [x] Thread-safe stepper access (mutex + atomic variables)
- [x] CAS state transitions (race-free)
- [x] ESTOP reset via Core 0 pending pattern
- [x] Storage reliability (mutex, debounce, copy-based API)
- [x] WiFi/BLE thread safety (all calls via storageTask)
- [x] Confirm dialog crash fix (pending-flag pattern)
- [x] Keyboard crash fixes (deferred cleanup)
- [x] 8 accent color themes with live switching
- [x] Display settings (brightness slider, dim timeout)
- [x] System info screen (core load, heap, PSRAM, WiFi, uptime)
- [x] Motor configuration UI (microstepping, acceleration, calibration)
- [x] DM542T driver support
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
- [ Assembly guide
- [ ] WiFi SoftAP mode (standalone network)
- [ ] Web-based remote control panel (hosted on C6)
- [ ] Bidirectional state sync (phone mirrors display)
