# V5 UI integration

Implemented in the existing LVGL screens:

- Dark graphite and orange theme, 12/16 px corner radii, minimum shared font 14 px.
- Main screen with large orange speed panel, native 104 px bold numeric font, explicit CW/CCW selection, 0.01 RPM adjustment, source indication and fixed wide START/STOP position.
- Motor configuration and physical direction-switch interlocks remain effective. Direction selection accounts for the configured inversion. Speed adjustment and direction changes are blocked during motion.
- Strong red stop buttons. Full-width red fault banner, explicit physical input/alarm state and readable disabled reset button. Reset callback still checks the existing safety gate.
- Orange primary-value cards in pulse/continuous editing; shared styling is applied across all 22 existing screens.

The mockups are a design reference. Existing functional screen layouts are retained where their controls differ: calibration remains the current scrollable workflow, and some step/countdown widgets remain instrument-style. Proposed calibration verification gates and new workflow features from the mockups are not newly implemented by this UI-only deployment.

Validation: 403 native tests passed. The actual LVGL simulator self-test passed, including new RPM +/- and reset-blocking cases. Simulator pumping now updates the fault overlay as the firmware UI task does. Firmware release/debug/mirror builds and upload results are in validation/2026-10-01/ui-v5. Physical legibility and touch behavior require inspection on the device.

[Actual runtime screens](images/ui_runtime_v5/overview.png), [main](images/ui_runtime_v5/01_MAIN.png), [active fault](images/ui_runtime_v5/ESTOP_ACTIVE.png).

Fonts: generated Montserrat Bold numeric subset, lv_font_conv 1.5.3; SIL OFL license included with the source. Firmware and simulator compile the same font file.

## Device upload

Release firmware uploaded to COM3 on 1 October 2026. Esptool verified the written data hash and issued a hard reset through RTS. All three final firmware variants built successfully. The upload verifies flash contents, not physical motion or hardware safety. See [upload log](validation/2026-10-01/ui-v5/upload.log).
