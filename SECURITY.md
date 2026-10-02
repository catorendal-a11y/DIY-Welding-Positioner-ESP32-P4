# Security Policy

## Supported Versions

| Version | Supported |
| ------- | --------- |
| 2.1.x   | Current; use the latest release |
| 2.0.x   | Upgrade recommended for STOP, pedal, JOG and persistence fixes |
| 1.x.x   | No |
| < 1.0   | No |

## Hardware Safety

This project controls a motorized welding positioner. Safety is critical.

### E-STOP System
- GPIO34 expects HIGH healthy / LOW fault and a FALLING interrupt. A bare NC contact to GND has opposite polarity; verify input conditioning and cable-break behavior.
- ISR immediately cuts ENA pin (HIGH = motor disabled) and sets `g_wakePending` so a dimmed backlight recovers on the next UI dim pass
- Software state machine enforces ESTOP as highest-priority state
- All state transitions validated via compare-and-swap (CAS) pattern
- UI overlay blocks all interaction during ESTOP; overlay show also calls `dim_reset_activity()` for full brightness
- Reset is guarded by physical input/alarm health, returns to idle and never starts motion. Physical driver-disable behavior and stop latency require bench measurement.

### Motor Safety
- ENA pin defaults HIGH (motor disabled) on boot
- ENA never driven LOW without ESTOP check
- Task Watchdog Timer (TWDT) on motor, control and safety tasks, with checked setup and critical-task readiness gating
- Stepper access protected by FreeRTOS mutex
- STOP has a separate latch and invalidates queued starts. JOG requires renewal; pedal requires stable release before arming and fresh active ADC data.

## Reporting a Vulnerability

If you discover a safety-critical bug or security vulnerability:

1. **Do not open a public GitHub issue** for safety-critical problems
2. Email the maintainer directly or use [GitHub Security Advisories](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/security/advisories/new)
3. Include: firmware version, reproduction steps, hardware configuration, serial log output (debug build)

### What to Report
- E-STOP bypass or failure to stop motor
- Unauthorized or unintended motor start or motion
- State machine corruption allowing unsafe transitions
- Any flaw that could expose motor control to untrusted input

### Response Timeline
- Safety-critical: response within 24 hours, fix within 1 week
- Non-critical: response within 1 week

## Embedded Security Considerations

- **No OTA for P4 application firmware** in the baseline release flow documented here
- **NVS has no application-layer encryption** — settings (`cfg`) and presets (`prs`) are **plaintext JSON blobs** in namespace `wrot` (same practical risk as unencrypted files on flash)
- **USB mirror** is optional, disabled after boot and requires physical-screen arming. Remote input is released on keepalive timeout or failed transport; it has no direct motor API. Treat an armed USB connection as operator access.
- Native/simulator tests use models or stubbed hardware in some paths. A passing build does not certify the physical safety circuit.
