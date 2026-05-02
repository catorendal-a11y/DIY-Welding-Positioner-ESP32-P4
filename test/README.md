# Unit and Integration Testing Structure

This directory is intended for PlatformIO native and unity-based tests. 
The system uses a mockable hardware layer to allow for logic verification without a physical ESP32-P4. Run commands from a PlatformIO environment so `pio` uses a compatible Python runtime.

## Current Test Suites
- `test_logic/`: Verifies state transitions, command queue overwrite semantics, mode request helpers, RPM-to-Hz conversions, clamp/floor behavior, direction override behavior, soft-start/auto-stop helpers, and storage helpers.
- `test_screens/`: Verifies screen registry logic that can run without LVGL hardware.
- `test_utils/`: Verifies pure utility helpers.

To run tests:
```bash
pio test -e native
```

## Simulator UI Checks

The Windows SDL simulator can smoke-test the screen registry and export visual
screenshots:

```powershell
.\simulator\run.ps1 -SelfTest
.\simulator\run.ps1 -Screenshots artifacts\sim_screens
```

These checks verify UI construction/navigation logic only. Real E-STOP, driver
alarm, touch hardware, motor movement, and calibration measurement still require
the ESP32-P4 device.
