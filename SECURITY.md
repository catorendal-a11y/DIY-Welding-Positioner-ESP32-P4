# Security Policy

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 2.0.x   | Yes |
| 1.x.x   | No |
| < 1.0   | No |

## Hardware Safety

This project controls a motorized welding positioner. Safety is critical.

### E-STOP System
- Hardware NC contact on GPIO34 triggers interrupt in <0.5ms
- ISR immediately cuts ENA pin (HIGH = motor disabled)
- Software state machine enforces ESTOP as highest-priority state
- All state transitions validated via compare-and-swap (CAS) pattern
- UI overlay blocks all interaction during ESTOP

### Motor Safety
- ENA pin defaults HIGH (motor disabled) on boot
- ENA never driven LOW without ESTOP check
- Task Watchdog Timer (TWDT) on motor and safety tasks
- Stepper access protected by FreeRTOS mutex

### BLE Security
- BLE uses passkey pairing (default: `123456`)
- ARM command required within 5 seconds before START
- BLE write callbacks use pending flags — no direct motor control from BLE thread
- Notify rate limited to 500ms to prevent SDIO bus saturation

## Reporting a Vulnerability

If you discover a safety-critical bug or security vulnerability:

1. **Do not open a public GitHub issue** for safety-critical problems
2. Email the maintainer directly or use [GitHub Security Advisories](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/security/advisories/new)
3. Include: firmware version, reproduction steps, hardware configuration, serial log output (debug build)

### What to Report
- E-STOP bypass or failure to stop motor
- BLE command injection allowing motor start without ARM
- State machine corruption allowing unsafe transitions
- WiFi/BLE vulnerabilities exposing motor control

### Response Timeline
- Safety-critical: response within 24 hours, fix within 1 week
- Non-critical: response within 1 week

## Embedded Security Considerations

- **No OTA for P4 firmware** — only C6 co-processor receives OTA updates
- **WiFi credentials (if enabled in a given build)** — stored on flash without hardware encryption unless a release notes otherwise; mainline WiFi UI may be disabled
- **BLE passkey is hardcoded** — change in `src/ble/ble.cpp` before deployment in sensitive environments
- **No TLS/HTTPS** — WiFi remote does not use encrypted transport
- **NVS has no application-layer encryption** — settings (`cfg`) and presets (`prs`) are **plaintext JSON blobs** in namespace `wrot` (same practical risk as unencrypted files on flash)
