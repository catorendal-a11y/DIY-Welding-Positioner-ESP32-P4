# Unit and Integration Testing Structure

This directory is intended for PlatformIO native and unity-based tests. 
The system uses a mockable hardware layer to allow for logic verification without a physical ESP32-P4. Run commands from a PlatformIO environment so `pio` uses a compatible Python runtime.

## Current Test Suites
- `test_logic/`: Verifies state transitions, mode request helpers, RPM-to-Hz conversions, clamp/floor behavior, and storage helpers.
- `test_screens/`: Verifies screen registry logic that can run without LVGL hardware.
- `test_utils/`: Verifies pure utility helpers.

To run tests:
```bash
pio test -e native
```
