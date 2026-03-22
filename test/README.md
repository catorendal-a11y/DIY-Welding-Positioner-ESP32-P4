# Unit and Integration Testing Structure

This directory is intended for PlatformIO native and unity-based tests. 
The system uses a mockable hardware layer to allow for logic verification without a physical ESP32-P4.

## Current Test Suites
- `test_logic/`: Verifies state machine transitions (Idle -> Run -> E-Stop).
- `test_math/`: Verifies RPM-to-Hz conversions and gear ratio scaling.
- `test_ui/`: Verifies LVGL screen switching logic.

To run tests:
```bash
pio test -e native
```
