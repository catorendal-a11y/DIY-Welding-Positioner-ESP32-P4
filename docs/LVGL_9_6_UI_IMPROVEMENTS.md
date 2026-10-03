# LVGL 9.6 UI refinements

These v2.2.0 changes retain the V5 graphite/orange design and its 800 x 480 layout. They refine typography and how the operating panel and Diagnostics receive status.

## Text placement

Shared screen labels and buttons, including Calibration, Motor Configuration and New Program/Edit Program, use `LV_TEXT_LEADING_TRIM_CAPITAL` to remove unused space above the first text line. Space below the baseline remains available for names and instructions containing descenders such as `g`, `j`, `p` and `y`. The same policy applies to input-overlay labels. The recursive trim helper skips textareas, keyboards, dropdowns and spinboxes so their internal text/cursor metrics stay intact.

The layout audit measures text with the public top/bottom trim APIs. It still rejects insufficient label height and out-of-parent bounds. A deliberately short trimmed label verifies that clipping is detected.

Leading trim is only enabled when the font supplies cap-height metadata. Older generated fonts, including the 104 px numeric subset on the main panel, remain untrimmed. A regression checks this exception; the layout audit also rejects trim without the required font metrics.

## UI data bindings

`src/ui/value_binding.h` uses LVGL 9.6's public subject creation and typed string, integer and boolean bindings. Each binding has one widget and a screen-owned lifetime. Fixed string buffers copy temporary formatted text; unchanged strings are compared before notifying observers.

Main-screen RPM, captions, limits, surface speed, diameter, speed-source text, motion/storage status and START text use string bindings. The speed bar uses an integer binding. Button visibility and availability use boolean bindings. Existing direction styling and control command callbacks remain in place.

Diagnostics uses `ui_control_view()` for target/estimated RPM and `ui_control_state()` for motion state, matching the operating panel's snapshot policy. When the snapshot is stale, it shows `STATUS STALE` and `---` for RPM values. GPIO rows remain direct pin-level diagnostics; they are not an independent motion measurement.

All binding creation, updates and cleanup run in the UI context. Control and safety tasks never call LVGL observers. Bindings do not replace command admission or physical E-STOP handling.

## Consistent input and motor settings

`src/ui/input_panel.h` provides the same full-screen editor for program name/RPM, calibration angle/diameter and motor limit/acceleration. It has a visible CANCEL button, an unchanged LVGL keyboard, and a separate error area. Closing, navigation, external deletion and theme reconstruction release the panel's widget references and error binding. A queued deletion cannot clear a replacement panel.

Motor Configuration uses two value cards with larger adjustment buttons and retained sliders. Short taps and hold-to-repeat retain the existing adjustment behavior, without an extra step on release. Tap the value to enter exact maximum RPM (0.001–3.000, at most three decimal places) or whole-number motor acceleration (1000–30000 steps/s2). Dot and comma decimals are accepted. Invalid or fractional acceleration input keeps the editor open and leaves the draft unchanged; values are not silently rounded. Confirming an input changes only the screen draft. SAVE & APPLY submits that draft to the control owner and waits for the storage receipt.

Microstep and direction toggles expose their checked state. Pending saves and unavailable/stale control lock edits and apply; cancelled requests allow retry. Returning to Motor Configuration reloads the current settings. Shared buttons and sliders visibly dim when disabled. Calibration text and motor settings/status use typed bindings to skip unchanged text updates.

Shared button availability changes immediately, without the default theme's delayed fades. A regression checks that verified calibration Save is both enabled and visibly accented before accepting it.

The UI remains flat: no blur or shadows were added. Layout audits check both traditional and drop-shadow properties as well as label bounds.

Fault/E-STOP overlays move to the front of the top layer when shown, covering input panels created later. Calibration confirmation reports an interrupted session explicitly instead of misreporting it as an invalid numeric range. The physical E-STOP input and motor stopping policy are unchanged.

## Lifecycle and failure behavior

Screens reset bindings before replacing widgets and during theme reconstruction. LVGL detaches object-bound observers on widget deletion; an additional delete hook clears the direct-update fallback's widget pointer. If a subject/observer cannot be allocated, the binding falls back to direct widget updates. No allocation-failure injector was added for LVGL itself.

Simulator regressions cover unchanged integer notifications, text copied from a temporary buffer, callback cleanup, rebinding to another widget, update after widget deletion, repeated main/Diagnostics reconstruction, stale status and recovery. Input-panel tests also cover external deletion, replacement before queued deletion, and untouched textarea metrics. Motor tests cover exact input, invalid ranges, fractional acceleration, Cancel, draft isolation, save failure/retry, edit locks, navigation/theme cleanup and stale control. Existing navigation, program-save, calibration and commissioning scenarios remain part of the self-test.

## Local validation

Windows simulator self-test and the program-editor preview flow pass. The 800 x 480 layout audit reports zero failures. Release, debug and USB-mirror firmware compile successfully. Calibration, main, Diagnostics, motor configuration and program-editor simulator screenshots were visually checked. A functional pre-release build at commit 673e078 was later uploaded to the owner's ESP32-P4 with flash verification. On 2026-10-03 the owner confirmed that all currently assembled controller functions work. Pedal/ADS1115 wiring and commissioning are unfinished and intentionally deferred. This is owner-reported functional confirmation; no quantitative stop-time, calibration-accuracy, rendering or HF test measurements were supplied.

## Performance and scope

The software-renderer improvements already follow from LVGL 9.6. This work does not claim an FPS increase on the GUITION JC4880P443C board. Firmware still uses the existing manually rotated RGB565 flush path; hardware timing and rendering performance require measurement on the board.

Image-downscale helpers are reserved for a future illustrated setup flow; no new wiring illustration is added here. No display-sync or motor-control architecture change is introduced.

Reference: [official LVGL 9.6 changelog](https://lvgl.io/docs/open/changelog/CHANGELOG).
