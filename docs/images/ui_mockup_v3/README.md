# V3 UI proposals

Historical SVG proposals for all 22 registered screen types in `src/ui/screens.h`, plus eight additional states: 30 views at 800 × 480. The final integration uses the later V5 design.

- Open `index.html` for the gallery and individual files.
- Open `all_screens.svg` for the scalable vector overview.
- `01_boot.svg` through `30_input_fault.svg` are standalone SVGs.
- `manifest.json` maps each view to its ScreenId and explains its purpose.

## Design choices

Dark graphite, orange for action/activity, red for stop/fault and green for confirmed state. Status also has text. Bahnschrift uses Segoe UI/Arial fallbacks; text remains editable.

Main prioritizes speed, direction and source. Pulse-derived RPM is labelled CALCULATED. Surface speed appears with diameter. Values are illustrative rather than live machine data.

Primary buttons are generally 56–82 px high; JOG has two large hold buttons. STOP is bottom right on operating screens. Keyboard footer keys are 48 px high. Secondary text requires inspection on the physical 4.3-inch display.

## Screen groups

1. **Operate, 01–06:** boot, main ready/running, menu, mode picker, jog.
2. **Process & safety, 07–12:** pulse, step, countdown, active/resettable E-STOP, confirmation.
3. **Programs, 13–18:** list, edit, continuous/pulse/step, empty list.
4. **Setup, 19–24:** settings, motor, pedal, display, calibration/verification.
5. **Service & input, 25–30:** diagnostics, system info, about, numeric/text keyboard, pedal fault.

## Proposed behavior

These files record the original design delivery. Proposed task readiness, release-before-arm pedal input, program review, auto-stop, idle-only settings and separate USB view/control permission are not evidence of implementation. See [implemented improvements](../../IMPROVEMENTS_2026-10-01.md) and [V5 integration](../../UI_V5_DEPLOYMENT.md) for subsequent changes.

RESET returns to IDLE; START is separate. Screen indicators do not replace checking the physical safety circuit.

## Regeneration

Run `python scripts/generate_ui_mockup_v3.py`. The standard-library generator verifies coverage of all 22 registered ScreenIds and retains existing proposals.
