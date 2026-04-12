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
- ISR immediately cuts ENA pin (HIGH = motor disabled) and sets `g_wakePending` so a dimmed backlight recovers on the next UI dim pass
- Software state machine enforces ESTOP as highest-priority state
- All state transitions validated via compare-and-swap (CAS) pattern
- UI overlay blocks all interaction during ESTOP; overlay show also calls `dim_reset_activity()` for full brightness

### Motor Safety
- ENA pin defaults HIGH (motor disabled) on boot
- ENA never driven LOW without ESTOP check
- Task Watchdog Timer (TWDT) on motor and safety tasks
- Stepper access protected by FreeRTOS mutex

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
