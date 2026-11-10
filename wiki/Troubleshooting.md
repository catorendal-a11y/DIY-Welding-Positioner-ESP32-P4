# Troubleshooting

> [!CAUTION]
> Turn off motor power before touching motor wiring. Never connect or disconnect motor coils while the driver is powered.

## Motor Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="motor-does-not-move"></a>Motor does not move | STEP/DIR/ENA wiring, ENA logic, supply or microstep mismatch | Check STEP/DIR/ENA wiring continuity. Verify ENA pin logic: HIGH = disabled, LOW = enabled. Confirm motor supply is in range (24V or 36V on driver `VM`) at >=5A; **36V** is optimal if wiring and driver allow it. Check that microstepping DIP switches match the UI setting |
| <a id="motor-stalls-under-load"></a>Motor stalls under load | Basic PUL/DIR drivers have no anti-resonance DSP, so stalling at certain RPMs is expected | Use 1/8 or finer microstepping to reduce resonance. If using DM542T, set the driver to DM542T under <kbd>Settings</kbd> › <kbd>Motor configuration</kbd> and match DIP microstep to the UI |
| <a id="motor-stalls-at-low-rpm-with-14-microstepping"></a>Motor stalls at low RPM with 1/4 microstepping | NEMA 23 resonance zone is 100-300 motor RPM. At 0.5 RPM workpiece: motor speed ≈ 0.5 x 108 x 3.75 = 202.5 RPM (above coarse-microstep resonance zone for some drivers) | Use 1/8 microstepping or upgrade to DM542T |
| <a id="wrong-rotation-direction"></a>Wrong rotation direction | Direction setting or coil order | Toggle direction switch (GPIO 29) or press CW/CCW button on main screen. Or physically reverse one motor coil pair (A+ and A-) |
| <a id="speed-not-changing-during-rotation"></a>Speed not changing during rotation | Old firmware, or RPM controls blocked during rotation | Ensure firmware is v2.0.2 or newer (includes `applySpeedAcceleration()` fix). Check that potentiometer ADC reads change in debug log. V5 adds idle-screen RPM **+ / −** controls. During rotation, these controls are blocked; the potentiometer remains available according to the selected speed source |
| <a id="main-screen-rpm--controls"></a>Main screen: RPM +/− controls disabled | Motion or fault state. Older firmware had a pot-only main screen | V5 restores main-screen RPM +/− controls. Check motion and fault state |
| <a id="main-start-sometimes-does-not-start"></a>Main <kbd>Start</kbd> sometimes does not start | STOP uses a separate latch and invalidates pending starts. Task readiness, fault or pedal state may block it | A new explicit <kbd>Start</kbd> is required after STOP is processed. See the checks below |
| <a id="direction-switch-not-working"></a>Direction switch not working | Switch disabled, or wired to the wrong pin | Enable it in <kbd>Settings</kbd> › <kbd>Motor configuration</kbd> › <kbd>Physical direction switch</kbd>. Verify physical wire is on GPIO 29 (not GPIO 28, which is C6_U0TXD) |

### Main Start sometimes does not start: checks

- [ ] Open <kbd>Settings</kbd> › <kbd>Diagnostics</kbd> and verify that `E-STOP` and `Driver alarm` show `High / OK`, `State` shows `IDLE` and `Enable output` shows `High / disabled`.
- [ ] Check the Diagnostics event log for the latest START/STOP, pedal, program and fault entries. It shows whether motion was requested or blocked.
- [ ] If using a foot pedal, open <kbd>Settings</kbd> › <kbd>Pedal settings</kbd> and verify `Start switch / GPIO33` changes between `Open` and `Pressed`.

> [!NOTE]
> Quick JOG tap/release is also protected: release cancels a pending JOG start before `controlTask` can run it.

## Display Issues

> [!WARNING]
> HF-start TIG can couple into open wiring strongly enough to reset ESP32-P4, lock I2C/touch, or create false GPIO/ADC readings.

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="controller-resets-touch-freezes-or-inputs-glitch-when-tig-arc-starts"></a>Controller resets, touch freezes, or inputs glitch when TIG arc starts | HF noise coupling into open wiring | Install the ESP32-P4 screen, stepper driver, and motor PSU inside the same grounded aluminum/steel enclosure. Bond the enclosure to PE/chassis ground with a short low-impedance connection. Keep motor cables physically separated from E-STOP, pedal, pot, STEP/DIR/ENA, and I2C wiring. Use shielded external cables and terminate shields to the enclosure at the controller side. Add ferrites on motor, E-STOP, pedal/pot, USB/power, and external signal cables if needed |
| <a id="black-screen-on-boot"></a>Black screen on boot | Driver, PSRAM or stack settings | Verify ESP32-P4 drivers installed correctly. Check PSRAM init settings in `platformio.ini`. Check `ARDUINO_LOOP_STACK_SIZE=32768` in build flags |
| <a id="screen-in-portrait-mode"></a>Screen in portrait mode | Not a fault: rotation is handled manually in the flush callback | Leave it. Do NOT add `lv_display_set_rotation()`; it crashes ESP32-P4 |
| <a id="usb-c-mirror-is-slow-or-stuck-on-waiting"></a>USB-C mirror is slow or stuck on WAITING | Wrong firmware, port in use, baud rate or disarmed remote control | See the mirror checklist below |
| <a id="touch-not-responding"></a>Touch not responding | I2C wiring or pull-ups | Verify I2C SDA=GPIO 7, SCL=GPIO 8. Check that external pull-ups are present on the board |
| <a id="montserrat-font-crash"></a>Montserrat font crash | Font too large | Maximum safe font size is montserrat_40. montserrat_48 and larger will crash ESP32-P4 |
| <a id="blue-flash-during-settings-save"></a>Blue flash during settings save | NVS writes can block briefly while flash cache is toggled; firmware sets `g_flashWriting` to limit visible glitches | Allow **1-2 seconds** after edits. `storageTask` debounces settings (~1s) and presets (~500ms) |

### USB-C mirror checklist

- [ ] Build and flash the mirror firmware: `pio run -e esp32p4-mirror --target upload`
- [ ] Close PlatformIO Monitor. The mirror viewer needs exclusive access to the COM port.
- [ ] Start with `.\simulator\run.ps1 -UsbMirror COM5 -Baud 4000000`. Fall back to `2000000` or `921600` only if the Windows driver is unstable.
- [ ] After flashing, arm <kbd>Settings</kbd> › <kbd>Display settings</kbd> › <kbd>USB remote control</kbd> again on the physical screen. It is intentionally not saved across reboot.

> [!NOTE]
> Higher baud alone is not the main fix. Current mirror firmware uses LVGL partial flushes so only dirty rectangles are streamed.

## Storage

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="storage-fault-at-boot"></a>STORAGE fault at boot | Saved data is invalid. Motion stays disabled and the data is left untouched | Reflash a matching build, or erase flash and set up again |
| <a id="settings-or-presets-not-persisting"></a>Settings or presets not persisting | Current firmware stores data in **NVS** (namespace `wrot`, keys `cfg` and `prs`), not LittleFS files. Saves run only at rest | Stop the motor, then wait for the debounced flush (1-2 seconds). After a full flash erase, re-enter settings and presets; ensure the partition table includes an **NVS** data partition |

## Build Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="support_select_driver_type-compile-error"></a>`SUPPORT_SELECT_DRIVER_TYPE` compile error | `DRIVER_RMT` is not available on ESP32-P4 (only RMT supported) | ESP32-P4 automatically selects RMT, so no explicit selection is needed. Use `stepperConnectToPin(PIN_STEP)` without driver type argument |
| <a id="-mfix-esp32-psram-cache-issue-compile-error"></a>`-mfix-esp32-psram-cache-issue` compile error | This is an Xtensa compiler flag; ESP32-P4 uses RISC-V | Remove this flag |
| <a id="system-python-314-incompatible-with-platformio"></a>System Python 3.14 incompatible with PlatformIO | PlatformIO needs its own compatible Python runtime | Run builds from the PlatformIO/VS Code terminal so `pio` uses PlatformIO's own compatible Python runtime |
| <a id="esptool-unicode-crash-during-flash"></a>esptool Unicode crash during flash | Windows cp1252 encoding can't handle esptool progress bar characters | `set PYTHONUTF8=1 && pio run --target upload` |
| <a id="legacy-littlefs-settings-not-found"></a>Legacy LittleFS settings not found | Current firmware stores settings and presets in **NVS** (`wrot/cfg`, `wrot/prs`), not active LittleFS files | On first boot after upgrading, `storage_init()` migrates legacy `/settings.json` and `/presets.json` into NVS if the NVS keys are empty |

## Speed Control Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="calibration-will-not-save"></a>Calibration will not save | This is intentional unless verification has passed | Open <kbd>Settings</kbd> › <kbd>Calibration</kbd>, set the workpiece OD on that page, then <kbd>Move 360°</kbd>, measure real rotation, enter measured degrees, <kbd>Apply measurement</kbd>, run <kbd>Verify 360°</kbd>, and <kbd>Save calibration</kbd> only after PASS. Test calibration unloaded first. The firmware can verify UI/math flow, but the final factor depends on the real drivetrain measurement |
| <a id="program-does-not-start-or-rpm-drops-with-a-large-diameter"></a>Program does not start, or RPM drops with a large diameter | Workpiece RPM is capped per diameter by `MAX_STEP_HZ` (200 kHz) and `MOTOR_RPM_LIMIT` (1500 RPM). A saved program above the cap is blocked at start and shows **PROGRAM BLOCKED** with the reason | Lower the program RPM or check its diameter, roller diameter and gear ratio in <kbd>Motor configuration</kbd> |
| <a id="potentiometer-dead-zone-at-bottom"></a>Potentiometer dead zone at bottom | ADC reference is 3315 (measured range of 10k pot with 11dB attenuation) | If pot doesn't reach 0 RPM, calibrate ADC reference in `speed.cpp` |
| <a id="buttons-dont-change-rpm-during-rotation"></a>Buttons don't change RPM during rotation | Main-screen RPM +/− is available only while idle and safe. Jog and program/edit flows retain their own controls | None needed. UI callbacks never call `speed_apply()` directly. Core 0 applies live speed via `motor_set_target_milli_hz()`, which wraps stepper speed and acceleration under the motor mutex |
| <a id="rpm-gauge-doesnt-show-below-01"></a>RPM gauge doesn't show below 0.1 | Gauge scaling follows `MIN_RPM` / max RPM from settings; firmware `MIN_RPM` is **0.001** (`config.h`) | If the gauge looks "stuck", confirm Motor configuration **max RPM** and preset RPM are within range |

## Safety

> [!CAUTION]
> Do not run the machine while an E-STOP problem is unresolved. Verify the E-STOP circuit and ENA HIGH disable before motor power.

### Fault names

The overlay names the first latched fault, states the next action under **What to do** and gives the cause below it. The banner is red while something blocks the reset and amber with **READY TO RESET** when only the reset is missing. Clear the cause, then press <kbd>RESET TO IDLE</kbd>, unless the table says to restart. Reset never starts motion.

| Fault | Meaning | What to do |
|---|---|---|
| **E-STOP** | Physical E-STOP input is active | Release it and check the machine |
| **E-STOP GLITCH** | Input changed during debounce | Check wiring and HF shielding |
| **DRIVER ALM** | Driver alarm input is active | Check the driver and motor. Even a short alarm latches |
| **PEDAL INPUT** | Pedal measurement unavailable | Release the pedal and restore the input |
| **SPEED POT** | Pot jumped to the rail | Check the wiring, then turn the knob up. Reset stays blocked until it reads plausibly |
| **MOTOR COMMAND** / **MOTOR TIMEOUT** | Motor rejected a command or timed out | Inspect the drive |
| **CONTROL STALE** / **INPUT STALE** | A task stopped responding | Restart the controller |
| **HMI STALE** | UI stalled or touch stopped responding | Restart the controller. If this happens during TIG HF, see the enclosure section above |
| **UNEXPECTED RESET** | Restart after a crash, watchdog or brownout | Check the power supply and the Diagnostics event log |
| **STORAGE** | Saved data is invalid; motion disabled, data kept | Reflash or erase |

### E-STOP and boot symptoms

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="e-stop-not-working"></a>E-STOP not working | Wiring does not match firmware: **safe = GPIO 34 HIGH**, **fault = GPIO 34 LOW**; ISR is **FALLING** (see `safety.cpp`, `docs/HARDWARE_SETUP.md`) | Fix the wiring to match. Firmware calls `pinMode(PIN_ESTOP, INPUT_PULLUP)`, so add **external** pull-up / RC if leads are long or noisy (see `EMI_MITIGATION.md`, `config.h` notes on GPIO34) |
| <a id="motor-moves-on-boot"></a>Motor moves on boot | Firmware sets ENA HIGH (disabled) before any motor init, so this points to hardware | Check for hardware wiring issue with ENA pin |
| <a id="e-stop-reset-doesnt-work"></a>E-STOP reset doesn't work | The reset button stays disabled while inputs are unsafe; reset never starts motion | Clear the physical fault, then press <kbd>RESET TO IDLE</kbd>. `controlTask` on Core 0 processes the reset via `safety_check_ui_reset()`. State transitions use CAS pattern to prevent race conditions |
| <a id="estop-triggered-at-power-on-even-though-the-button-isnt-pressed"></a>ESTOP triggered at power-on even though the button isn't pressed | A bare NC contact to GND has opposite polarity; startup sampling does not prove circuit continuity. v2.0.5+: `safety_init()` samples `PIN_ESTOP` three times with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle, and requires ≥2/3 LOW before treating ESTOP as pressed | Verify HIGH healthy / LOW fault at GPIO34. If you still see "ESTOP=PRESSED (low samples X/3)" with the button released, add an **external pull-up** to 3V3 (e.g. 10 kΩ) on the E-STOP input and/or shorten/shield the wiring (see `EMI_MITIGATION.md`) |

## Keyboard Crash

| Symptom | Cause | Fix |
|---------|-------|-----|
| <a id="typing-in-program-edit-causes-crash"></a>Typing in Program Edit causes crash | Keyboard close/delete from inside LVGL event callback causes use-after-free | Fixed in v2.0.2: deferred cleanup pattern with `*ClosePending` flags + `lv_obj_delete_async()`. `screen_*_invalidate_widgets()` functions prevent dangling pointers after theme changes |
| <a id="crash-after-navigating-many-screens-iwdt-timeout"></a>Crash after navigating many screens (IWDT timeout) | The spinlock disabled interrupts on both cores during cross-core contention, causing Interrupt WDT timeout | Fixed in v2.0.2: `g_stepperMutex` changed from spinlock (`portMUX_TYPE`) to FreeRTOS mutex (`SemaphoreHandle_t`). The FreeRTOS mutex blocks via scheduler without disabling interrupts |
