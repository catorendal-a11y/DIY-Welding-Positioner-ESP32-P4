# Unit and Integration Testing Structure

This directory is intended for PlatformIO native and unity-based tests. 
The system uses a mockable hardware layer to allow for logic verification without a physical ESP32-P4. Run commands from a PlatformIO environment so `pio` uses a compatible Python runtime.

Uses upstream **Unity 2.7.0**, pinned by commit in `test_custom_runner.py`. The custom runner keeps PlatformIO's Unity integration while avoiding its automatic registry override to 2.6.1.

## Current Test Suites
- `test_logic/`: Legacy modeled state/motion/conversion/storage checks. Some queue helpers model the earlier overwrite approach; these tests do not exercise the production FreeRTOS dispatcher.
- `test_screens/`: Verifies screen registry logic that can run without LVGL hardware.
- `test_utils/`: Utility helpers and direct production-policy checks for STOP generations, pedal release interlocks, ADC freshness, save requests and persisted settings. The maintenance run passed 407 tests in the native environment.

To run tests:
```bash
pio test -e native -e native-control
```

## Production control and tooling tests

`test_control_production/` compiles the actual firmware dispatcher, motor wrapper, event log and all four motion modes with a fake clock, driver and FreeRTOS adapters. Its 31 cases cover command rejection, held locks, STOP cancellation/deadlines, pulse phases, counter wrap, mode timing, log contention, coherent concurrent snapshot reads, stale admission, direction inversion and setup sequence/migration rules, calibration draft isolation, interrupted moves, context changes, correction bounds and strict measurement parsing. Together with native tests, 438 cases pass. These are host integration tests, not device timing measurements.

The added migration cases check explicit RMT selection, asynchronous force-stop drain/reset inhibition and rejection of direction-timing changes while pulses remain queued. `python scripts/test_fas_rmt.py` compiles the installed upstream 1.4.0 RMT encoder and runs its `test_30` host regression for both buffer geometries; CI runs this on Linux and Windows. It does not measure physical GPIO pulses.

Run packaging regressions with `python -m unittest discover -s test/tooling -v` (six cases).

## Simulator UI Checks

The Windows SDL simulator can smoke-test the screen registry and export visual
screenshots:

```powershell
.\simulator\run.ps1 -SelfTest
.\simulator\run.ps1 -Screenshots artifacts\sim_screens
```

These checks run actual LVGL screen code against stubbed hardware, including navigation, program editing, main RPM +/−, blocked/available fault reset and the full commissioning workflow. The simulator uses the same production dispatcher, motor wrapper and four modes as native-control. Real E-STOP, driver
alarm, touch hardware, motor movement, and calibration measurement still require
the ESP32-P4 device.

## Program editor regression and preview

The actual LVGL self-test checks idempotent run-mode selection, availability, invalid RPM, decimal commas, UTF-8 byte limits, a 31-character wide name, fine RPM adjustment in Continuous/Pulse/Step settings, draft preservation, save/cancel and keyboard cleanup during navigation.

```powershell
simulator/build/rotator_simulator.exe --program-preview .pio/program-preview
```

This runs the same interaction regression and exports seven actual LVGL states with label-layout checks. [Program editor guide](../docs/PROGRAM_EDITOR.md). It does not connect to hardware.
