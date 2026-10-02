# Unit and Integration Testing Structure

This directory is intended for PlatformIO native and unity-based tests. 
The system uses a mockable hardware layer to allow for logic verification without a physical ESP32-P4. Run commands from a PlatformIO environment so `pio` uses a compatible Python runtime.

## Current Test Suites
- `test_logic/`: Legacy modeled state/motion/conversion/storage checks. Some queue helpers model the earlier overwrite approach; these tests do not exercise the production FreeRTOS dispatcher.
- `test_screens/`: Verifies screen registry logic that can run without LVGL hardware.
- `test_utils/`: Utility helpers and direct production-policy checks for STOP generations, pedal release interlocks, ADC freshness, save requests and persisted settings. The maintenance run passed 407 tests in the native environment.

To run tests:
```bash
pio test -e native -e native-control
```

## Production control and tooling tests

`test_control_production/` compiles the actual firmware dispatcher, motor wrapper, event log and all four motion modes with a fake clock, driver and FreeRTOS adapters. Its 12 cases cover command rejection, held locks, STOP cancellation/deadlines, pulse phases, counter wrap, mode timing and log contention. Together with native tests, 419 cases pass. These are host integration tests, not device timing measurements.

Run packaging regressions with `python -m unittest discover -s test/tooling -v` (four cases).

## Simulator UI Checks

The Windows SDL simulator can smoke-test the screen registry and export visual
screenshots:

```powershell
.\simulator\run.ps1 -SelfTest
.\simulator\run.ps1 -Screenshots artifacts\sim_screens
```

These checks run actual LVGL screen code against stubbed hardware, including navigation, program editing, main RPM +/− and blocked/available fault reset. Real E-STOP, driver
alarm, touch hardware, motor movement, and calibration measurement still require
the ESP32-P4 device.
