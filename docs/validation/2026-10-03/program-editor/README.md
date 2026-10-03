# Program editor validation — 3 October 2026

Host evidence for the unreleased source: LVGL 9.6.0, FastAccelStepper 1.4.0 pinned upstream, Arduino 3.3.12 / IDF 5.5.5.

- [Native suites](native.txt): 438/438 passing cases.
- [Actual LVGL self-test](simulator.txt): navigation/control, commissioning, calibration, and the new program interaction flow pass.
- [Layout audit](layout.txt): 0 failures at 800×480.
- [Firmware builds](firmware.txt): release/debug/mirror all pass; no application compiler warnings in this incremental build.
- `--program-preview .pio/program-preview` passes and exports seven states; `--screenshots .pio/program-runtime-screens` exports 24 screen/fault captures. [Images and workflow](../../../PROGRAM_EDITOR.md).

UI regression additionally checks all three mode editors at 0.076 RPM, adjusting to 0.077 and cancelling without changing the draft. It checks invalid RPM, comma input, UTF-8 limits, long names, mode availability, saved values and keyboard cleanup.

These checks do not measure GPIO timing, physical motor motion, touch response or storage durability on the board. No device flashing was performed.
