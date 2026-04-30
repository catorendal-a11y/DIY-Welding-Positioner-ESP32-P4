# Troubleshooting

## Motor Issues

### Motor does not move
- Check STEP/DIR/ENA wiring continuity
- Verify ENA pin logic: HIGH = disabled, LOW = enabled
- Confirm motor supply is in range (24V or 36V on driver `VM`) at >=5A; **36V** is optimal if wiring and driver allow it
- Check that microstepping DIP switches match the UI setting

### Motor stalls under load
- Basic PUL/DIR drivers have no anti-resonance DSP — stalling at certain RPMs is expected
- Use 1/8 or finer microstepping to reduce resonance
- If using DM542T, set **Settings > Motor Config > Driver** to DM542T and match DIP microstep to the UI

### Motor stalls at low RPM with 1/4 microstepping
- NEMA 23 resonance zone is 100-300 motor RPM
- At 0.5 RPM workpiece: motor speed ≈ 0.5 x 108 x 3.75 = 202.5 RPM (above coarse-microstep resonance zone for some drivers)
- Solution: use 1/8 microstepping or upgrade to DM542T

### Wrong rotation direction
- Toggle direction switch (GPIO 29) or press CW/CCW button on main screen
- Or physically reverse one motor coil pair (A+ and A-)

### Speed not changing during rotation
- Ensure firmware is v2.0.2 or newer (includes `applySpeedAcceleration()` fix)
- Check that potentiometer ADC reads change in debug log
- **v2.0.4+:** On the **main** screen, RPM is adjusted with the **potentiometer only** — there are no **+/-** touch buttons there. Use **Jog** (or load a program) for touch-based RPM tweaks where the UI provides them.

### Main screen: missing +/- buttons (v2.0.4+)
- **By design:** touchscreen **+/-** for workpiece RPM were removed from `SCREEN_MAIN` to simplify pot-only operation. Jog RPM still has **+/-** on `SCREEN_JOG`.

### Main START sometimes does not start
- Current firmware uses a single overwrite command queue, so the latest START/STOP request wins and stale STOP requests cannot swallow the next START.
- Open **Settings > Diagnostics** and verify `MOTION BLOCK = NO`, `ESTOP GPIO34 = HIGH OK`, and `DM542T ALM GPIO32 = HIGH OK`.
- Check the Diagnostics event log for the latest START/STOP, pedal, program and fault entries; it shows whether motion was requested or blocked.
- If using a foot pedal, open **Settings > Pedal Settings** and verify GPIO33 changes between `HIGH OPEN` and `LOW PRESSED`.
- Quick JOG tap/release is also protected: release cancels a pending JOG start before `controlTask` can run it.

### Direction switch not working
- Enable it in Settings > Motor Config > Direction Switch
- Verify physical wire is on GPIO 29 (not GPIO 28, which is C6_U0TXD)

## Display Issues

### Controller resets, touch freezes, or inputs glitch when TIG arc starts
- HF-start TIG can couple into open wiring strongly enough to reset ESP32-P4, lock I2C/touch, or create false GPIO/ADC readings
- Install the ESP32-P4 screen, stepper driver, and motor PSU inside the same grounded aluminum/steel enclosure
- Bond the enclosure to PE/chassis ground with a short low-impedance connection
- Keep motor cables physically separated from E-STOP, pedal, pot, STEP/DIR/ENA, and I2C wiring
- Use shielded external cables and terminate shields to the enclosure at the controller side
- Add ferrites on motor, E-STOP, pedal/pot, USB/power, and external signal cables if needed

### Black screen on boot
- Verify ESP32-P4 drivers installed correctly
- Check PSRAM init settings in `platformio.ini`
- Check `ARDUINO_LOOP_STACK_SIZE=32768` in build flags

### Screen in portrait mode
- Rotation is handled manually in the flush callback — this is correct
- Do NOT add `lv_display_set_rotation()` — it crashes ESP32-P4

### Touch not responding
- Verify I2C SDA=GPIO 7, SCL=GPIO 8
- Check that external pull-ups are present on the board

### Montserrat font crash
- Maximum safe font size is montserrat_40
- montserrat_48 and larger will crash ESP32-P4

### Blue flash during settings save
- NVS writes can block briefly while flash cache is toggled; firmware sets `g_flashWriting` to limit visible glitches
- Allow **1-2 seconds** after edits — `storageTask` debounces settings (~1s) and presets (~500ms)

## Storage

### Settings or presets not persisting
- Current firmware stores data in **NVS** (namespace `wrot`, keys `cfg` and `prs`), not LittleFS files
- Wait for debounced flush (1-2 seconds) after saving
- After a full flash erase, re-enter settings and presets; ensure the partition table includes an **NVS** data partition

## Build Issues

### `SUPPORT_SELECT_DRIVER_TYPE` compile error
- `DRIVER_RMT` is not available on ESP32-P4 (only RMT supported)
- ESP32-P4 automatically selects RMT — no explicit selection needed
- Use `stepperConnectToPin(PIN_STEP)` without driver type argument

### `-mfix-esp32-psram-cache-issue` compile error
- This is an Xtensa compiler flag — ESP32-P4 uses RISC-V
- Remove this flag; use `CONFIG_RMT_ISR_IRAM_SAFE=1` instead

### System Python 3.14 incompatible with PlatformIO
- Run builds from the PlatformIO/VS Code terminal so `pio` uses PlatformIO's own compatible Python runtime.

### esptool Unicode crash during flash
- Windows cp1252 encoding can't handle esptool progress bar characters
- Solution: `set PYTHONUTF8=1 && pio run --target upload`

### Legacy LittleFS settings not found
- Current firmware stores settings and presets in **NVS** (`wrot/cfg`, `wrot/prs`), not active LittleFS files.
- On first boot after upgrading, `storage_init()` migrates legacy `/settings.json` and `/presets.json` into NVS if the NVS keys are empty.

## Speed Control Issues

### Potentiometer dead zone at bottom
- ADC reference is 3315 (measured range of 10k pot with 11dB attenuation)
- If pot doesn't reach 0 RPM, calibrate ADC reference in `speed.cpp`

### Buttons don't change RPM during rotation
- Main-screen RPM is pot-only by design; Jog and program/edit flows have their own touch controls where applicable.
- UI callbacks never call `speed_apply()` directly. Core 0 applies live speed via `motor_set_target_milli_hz()`, which wraps stepper speed and acceleration under the motor mutex.

### RPM gauge doesn't show below 0.1
- Gauge scaling follows `MIN_RPM` / max RPM from settings; firmware `MIN_RPM` is **0.001** (`config.h`)
- If the gauge looks “stuck”, confirm Motor Config **max RPM** and preset RPM are within range

## Safety

### E-STOP not working
- Verify wiring matches firmware: **safe = GPIO 34 HIGH**, **fault = GPIO 34 LOW**; ISR is **FALLING** (see `safety.cpp`, `docs/HARDWARE_SETUP.md`)
- Firmware calls `pinMode(PIN_ESTOP, INPUT_PULLUP)` — add **external** pull-up / RC if leads are long or noisy (see `EMI_MITIGATION.md`, `config.h` notes on GPIO34)

### Motor moves on boot
- Firmware sets ENA HIGH (disabled) before any motor init
- If motor moves on boot, check for hardware wiring issue with ENA pin

### E-STOP reset doesn't work
- Tap the red overlay to dismiss — sets `g_uiResetPending.store(true, std::memory_order_release)` (declared in `src/app_state.h`)
- `controlTask` on Core 0 processes the reset via `safety_check_ui_reset()`
- State transitions use CAS pattern to prevent race conditions

### ESTOP triggered at power-on even though the button isn't pressed
- GPIO34 has **no internal pull-up** on ESP32-P4 and can float during boot if the E-STOP loop is disconnected or long-wired
- v2.0.5+: `safety_init()` samples `PIN_ESTOP` three times with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle, and requires ≥2/3 LOW before treating ESTOP as pressed. If you still see "ESTOP=PRESSED (low samples X/3)" with the button released, add an **external pull-up** to 3V3 (e.g. 10 kΩ) on the E-STOP input and/or shorten/shield the wiring (see `EMI_MITIGATION.md`).

## Keyboard Crash

### Typing in Program Edit causes crash
- Keyboard close/delete from inside LVGL event callback causes use-after-free
- Fixed in v2.0.2: deferred cleanup pattern with `*ClosePending` flags + `lv_obj_delete_async()`
- `screen_*_invalidate_widgets()` functions prevent dangling pointers after theme changes

### Crash after navigating many screens (IWDT timeout)
- Fixed in v2.0.2: `g_stepperMutex` changed from spinlock (`portMUX_TYPE`) to FreeRTOS mutex (`SemaphoreHandle_t`)
- The spinlock disabled interrupts on both cores during cross-core contention, causing Interrupt WDT timeout
- FreeRTOS mutex blocks via scheduler without disabling interrupts
