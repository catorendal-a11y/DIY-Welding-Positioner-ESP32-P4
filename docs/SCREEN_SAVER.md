# Idle screen saver (v2.2.0)

![Actual LVGL screen-saver capture](images/ui_runtime_v5/SCREEN_SAVER.png)

The screen saver uses the V5 graphite palette with the selected accent color, in both color schemes, English labels and flat graphics without shadows. A marker moves around the rotator ring every four seconds; the central composition also shifts slightly. It states "Motor stopped" and shows the set speed and direction. Both are settings, not measurements: the screen saver only appears while the motor is stopped. It does not display an invented clock.

## Settings and waking

1. **Open** <kbd>Settings</kbd> › <kbd>Display settings</kbd> › <kbd>Screen saver after</kbd>.
2. **Choose** <kbd>Off</kbd>, <kbd>30s</kbd>, <kbd>1m</kbd>, <kbd>2m</kbd> or <kbd>5m</kbd>.
3. **Press** <kbd>Save</kbd>.
4. **Try** <kbd>Preview</kbd> to open the screen saver immediately at normal brightness, including when the automatic timeout is Off.

This reuses the existing saved dim timeout; no settings migration is required. Automatic activation reduces the backlight to 38/255 and restores the configured brightness on wake.

> [!IMPORTANT]
> The first touch only wakes the display. The entire press is consumed until release, so holding a finger over <kbd>Start</kbd> cannot start rotation. A subsequent deliberate touch operates the UI normally.

The same filter serves the touchscreen, USB mirror and simulator inputs. Physical input activity also wakes the display; physical controls keep their existing behavior.

The previous page stays loaded underneath. Waking does not navigate, save changes, reset a fault or issue a motion command.

## When it stays awake

Automatic activation is limited to idle navigation/status pages: Main, Menu, Run Modes, Settings, Display, About, System Info and Diagnostics.

| Condition | Screen saver |
|---|---|
| Motion (including pulse pauses and stopping) | Stays off |
| Setup, calibration, editing | Stays off |
| Confirmation dialogs, input overlays | Stays off |
| Stale control status | Stays off |
| Pending/failed storage writes | Stays off |
| Fault / E-STOP overlays | Take priority and restore normal brightness |

Preview is disabled when these conditions prevent activation.

All widget changes run in the UI task under the LVGL lock. Touch-controller read failures do not release the wake-consumption latch; a successful release sample is required.

## Verification

The simulator self-test covers timeout activation, Off, preview, held-touch consumption through actual LVGL pointer hit testing over Start, a second deliberate Start touch, external motion start, physical wake requests, E-STOP priority, stale status, editing/commissioning exclusion, failed storage and repeated theme/overlay recreation. The capture checks text bounds and absence of shadows. The simulator records backlight commands but does not emulate physical LCD brightness.

Build the simulator, then run:

```powershell
.\simulator\build\rotator_simulator.exe --screensaver-preview .pio\screensaver-preview
```

This also exports BMP captures of the screen saver, its movement, the restored main page, E-STOP priority and Display settings.

> [!NOTE]
> Real GT911 touch, physical backlight behavior and USB mirror wake should be checked on the controller before treating hardware behavior as verified.

Local validation on 2026-10-03: 459 native/control/speed/storage tests passed; the complete simulator self-test and dedicated screen-saver preview passed; the all-screen layout audit reported zero failures; release, debug and USB-mirror firmware compiled successfully. The pre-release functional build at commit 673e078 was subsequently uploaded to the owner's ESP32-P4, its flash data verified, and no serial error output was observed during a 10-second check. On 2026-10-03 the owner confirmed that all currently assembled controller functions work. Pedal/ADS1115 wiring and commissioning were completed on 2026-10-05 (ADS1115 detected at 0x48; GPIO33 switch + analog speed owner-confirmed working with the master pedal fix). This is owner-reported functional confirmation; no quantitative stop-time, calibration-accuracy, rendering or HF test measurements were supplied.
