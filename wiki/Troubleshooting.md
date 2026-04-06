# Troubleshooting

## Motor Issues

### Motor does not move
- Check STEP/DIR/ENA wiring continuity
- Verify ENA pin logic: HIGH = disabled, LOW = enabled
- Confirm TB6600 power supply outputs 24V at >=5A
- Check that microstepping DIP switches match the UI setting

### Motor stalls under load
- TB6600 has no anti-resonance DSP — stalling at certain RPMs is expected
- Use 1/8 or finer microstepping to reduce resonance
- DM542T upgrade planned with built-in anti-resonance

### Motor stalls at low RPM with 1/4 microstepping
- NEMA 23 resonance zone is 100-300 motor RPM
- At 0.5 RPM workpiece: motor speed = 0.5 x 199.5 x 3.75 = 374 RPM (above resonance zone)
- Solution: use 1/8 microstepping or upgrade to DM542T

### Wrong rotation direction
- Toggle direction switch (GPIO 29) or press CW/CCW button on main screen
- Or physically reverse one motor coil pair (A+ and A-)

### Speed not changing during rotation
- Ensure firmware is v2.0.2 or newer (includes `applySpeedAcceleration()` fix)
- Check that potentiometer ADC reads change in debug log

### Direction switch not working
- Enable it in Settings > Motor Config > Direction Switch
- Verify physical wire is on GPIO 29 (not GPIO 28, which is C6_U0TXD)

## Display Issues

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
- Storage writes can block for >5ms, pre-empting display refresh
- This is normal — the flash lasts one frame cycle

## Build Issues

### `SUPPORT_SELECT_DRIVER_TYPE` compile error
- `DRIVER_RMT` is not available on ESP32-P4 (only RMT supported)
- ESP32-P4 automatically selects RMT — no explicit selection needed
- Use `stepperConnectToPin(PIN_STEP)` without driver type argument

### `-mfix-esp32-psram-cache-issue` compile error
- This is an Xtensa compiler flag — ESP32-P4 uses RISC-V
- Remove this flag; use `CONFIG_RMT_ISR_IRAM_SAFE=1` instead

### System Python 3.14 incompatible with PlatformIO
- Always use bundled Python: `C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe`

### esptool Unicode crash during flash
- Windows cp1252 encoding can't handle esptool progress bar characters
- Solution: `set PYTHONUTF8=1 && pio run --target upload`

### LittleFS.rename() crashes ESP32-P4
- LittleFS rename is broken on ESP32-P4 — do not use temp file + rename pattern
- Use direct FILE_WRITE for storage saves

## Speed Control Issues

### Potentiometer dead zone at bottom
- ADC reference is 3315 (measured range of 10k pot with 11dB attenuation)
- If pot doesn't reach 0 RPM, calibrate ADC reference in `speed.cpp`

### Buttons don't change RPM during rotation
- Firmware v2.0.2 uses `speed_request_update()` (thread-safe flag pattern)
- UI callbacks never call `speed_apply()` directly

### RPM gauge doesn't show below 0.1
- v2.0.2 uses 2-decimal precision and `MAX_RPM * 100` gauge range
- MIN_RPM is 0.02

## Safety

### E-STOP not working
- Verify NC button wiring: GPIO 34 -> button -> GND
- Button should hold GPIO LOW; pressing breaks circuit -> pulls HIGH -> ISR fires
- No external pull-up needed (internal INPUT_PULLUP enabled)

### Motor moves on boot
- Firmware sets ENA HIGH (disabled) before any motor init
- If motor moves on boot, check for hardware wiring issue with ENA pin

### E-STOP reset doesn't work
- Tap the red overlay to dismiss — sets `g_uiResetPending` flag
- `controlTask` on Core 0 processes the reset via `safety_check_ui_reset()`
- State transitions use CAS pattern to prevent race conditions

## Keyboard Crash

### Typing in Program Edit causes crash
- Keyboard close/delete from inside LVGL event callback causes use-after-free
- Fixed in v2.0.2: deferred cleanup pattern with `*ClosePending` flags + `lv_obj_delete_async()`
- `screen_*_invalidate_widgets()` functions prevent dangling pointers after theme changes

### Crash after navigating many screens (IWDT timeout)
- Fixed in v2.0.2: `g_stepperMutex` changed from spinlock (`portMUX_TYPE`) to FreeRTOS mutex (`SemaphoreHandle_t`)
- The spinlock disabled interrupts on both cores during cross-core contention, causing Interrupt WDT timeout
- FreeRTOS mutex blocks via scheduler without disabling interrupts
