# Layout and readability review — 1 October 2026

Scope: all 30 V5 SVG mockups covering all 22 registered screen types. Native viewport 800 × 480. This review changes design artifacts only, not the installed firmware.

## Changes

- Emergency-stop views now have a full-width red header, large white EMERGENCY STOP title, warning icon and explicit MOTOR OUTPUT DISABLED message.
- Active fault and cleared-input states remain distinct; RESET BLOCKED and RESET TO IDLE are clearly labelled. Reset never implies automatic restart.
- Operational STOP buttons have solid red backgrounds and white labels. They remain separate from the physical emergency-stop function.
- Minimum font size raised to 14 px. Body text is generally 18–22 px, common button labels 17 px, and critical stop actions larger.
- Header line spacing, EDIT action padding and large-number spacing corrected.
- Secondary orange surfaces darkened and disabled-control labels brightened for contrast.

## Measured results

| Check | Result |
| --- | --- |
| Views examined | 30 |
| Text elements measured | 581 |
| Text outside 800 × 480 | 0 |
| Text-to-text overlaps exceeding 1 px in both axes | 0 |
| Text lacking 6 px horizontal / 3 px vertical container inset | 0 |
| Text below 4.5:1 contrast against its flat containing rectangle | 0 |
| Minimum text size | 14 px |

Bounds use browser-rendered SVG getBBox, rather than estimated character counts. Contrast uses the sRGB relative-luminance formula and the preceding containing background rectangle. A conservative 4.5:1 target was applied to all text, including large and disabled labels. Detailed per-screen results: readability_audit.json. Regenerate checks with python scripts/audit_ui_mockup_v5.py (requires Playwright/Chromium).

## Review limits

These measurements cover the example English strings shown in the mockups. They do not prove readability on the physical panel, under workshop glare, through protective equipment or for every user's eyesight. The 14 px floor is a design choice, not a physical readability certification. Firmware implementation must preserve margins and test long program names, translated labels, extreme numeric values and font substitutions. Text should wrap or truncate intentionally instead of shrinking below the floor. Program names in lists should truncate; editing should expose the full name. The gallery and SVGs are not interactive hardware tests.
