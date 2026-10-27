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

Windows simulator self-test and the program-editor preview flow pass. The 800 x 480 layout audit reports zero failures. Release, debug and USB-mirror firmware compile successfully. Calibration, main, Diagnostics, motor configuration and program-editor simulator screenshots were visually checked. A functional pre-release build at commit 673e078 was later uploaded to the owner's ESP32-P4 with flash verification. On 2026-10-03 the owner confirmed that all currently assembled controller functions work. Pedal/ADS1115 wiring and commissioning were completed on 2026-10-05 (ADS1115 detected at 0x48; GPIO33 switch + analog speed owner-confirmed working with the master pedal fix). This is owner-reported functional confirmation; no quantitative stop-time, calibration-accuracy, rendering or HF test measurements were supplied.

## Performance and scope

The original software-renderer improvements follow from LVGL 9.6 and do not establish an FPS increase on the GUITION board. The later partial-SRAM changes below also require hardware timing measurements before claiming a performance result.

Image-downscale helpers are reserved for a future illustrated setup flow; no new wiring illustration is added here. No display-sync or motor-control architecture change is introduced.

## Partial SRAM rendering (October 2026)

The flush path now uses one 800 x 48 RGB565 draw buffer and an equally sized rotation buffer in internal DMA-capable SRAM, both aligned to 64 bytes. Each buffer is 76,800 bytes (150 KiB combined). These replace two 768,000-byte draw buffers and a 768,000-byte rotation buffer in PSRAM. The MIPI-DSI driver's two PSRAM scanout framebuffers remain unchanged.

PPA SRM rotates each dirty rectangle by 90 degrees counterclockwise, preserving the existing landscape-to-portrait mapping `(x, y) -> (y, 799 - x)`. Earlier comments incorrectly called this clockwise. PPA runs in blocking mode; the panel's existing `use_dma2d = false` path then copies synchronously before `lv_display_flush_ready()` allows buffer reuse. A second LVGL draw buffer would not overlap work in this pipeline. If PPA registration or a transaction fails, rotation uses the CPU implementation instead. Out-of-screen flush rectangles are rejected rather than clamped with an incorrect source stride. USB mirroring continues to receive landscape dirty rectangles before rotation.

Main, Jog, Step and Timer RPM fields use fixed dimensions and tabular digit advances. The fonts reuse the existing Montserrat bitmaps, including the main screen's 104 px subset. Step angle fields also have fixed dimensions; the arc shows progress and `DEG` on two lines. Their existing-style text bindings suppress unchanged updates, and the shared RPM setter skips identical strings. Motion commands, update cadence, E-STOP and fault overlays are unchanged.

Validation covers production CPU rotation for one-pixel, odd-sized, full-width and maximum-capacity strips, font metrics/kerning and RPM digit-count changes in actual LVGL, plus the simulator's screen/layout checks. Hardware PPA orientation, free internal heap under load, rendering latency, tearing and behavior during HF TIG welding still require measurements on the board. No sub-3-ms flush or 30–50 Hz performance result is claimed. No 3D orientation indicator or sprite asset was added; the setup wizard currently has no such feature.

API reference: [Espressif PPA guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/ppa.html).

Reference: [official LVGL 9.6 changelog](https://lvgl.io/docs/open/changelog/CHANGELOG).
