# TIG Rotator Controller - Project Implementation

**Board**: GUITION JC4880P443C ESP32-P4 4.3" Touch Display (with ESP32-C6 co-processor)
**Display**: ST7701S 480x800 MIPI-DSI (rotated to 800x480 landscape)
**Touch**: GT911 capacitive touch controller
**Firmware**: v2.0.9 (`FW_VERSION` in `src/config.h`)

---

## Table of Contents
1. [MIPI-DSI Display Setup](#1-mipi-dsi-display-setup)
2. [RTOS Task Architecture](#2-rtos-task-architecture)
3. [Motor Control & Live Speed Adjustment](#3-motor-control--live-speed-adjustment)
4. [Safety System](#4-safety-system)
5. [Storage & Presets](#5-storage--presets)
6. [UI Screen System](#6-ui-screen-system)
7. [Thread Safety Patterns](#7-thread-safety-patterns)
8. [Known Issues & Workarounds](#8-known-issues--workarounds)

---

## 1. MIPI-DSI Display Setup

### Hardware Configuration
- **Bus**: 2-lane MIPI-DSI — `config.h` documents **500 Mbps per lane** (`MIPI_DSI_LANE_BITRATE_MBPS`) as informational/reference only. The actual lane bitrate is set by the panel init sequence in `src/ui/display.cpp` and the ESP-IDF DSI driver; the constant in `config.h` is for documentation, not a live control.
- **DPI Clock**: 34 MHz for 60Hz refresh
- **Pixel Format**: RGB565 (16-bit)
- **Framebuffers**: 2 (double-buffered)
- **DMA2D**: Disabled (causes crashes with PSRAM on ESP32-P4)
- **Rotation**: Software rotation via `sw_rotate = 1` in LVGL driver (do NOT use `lv_display_set_rotation()` — crashes ESP32-P4)
- **Buffers**: Internal DMA-RAM (not PSRAM) — 80 lines x 800 pixels x 2 bytes = 128KB

### Touch (GT911)
- **I2C**: GPIO 7 (SDA), GPIO 8 (SCL)
- **Swap XY**: 0 (LVGL handles rotation)
- **Mirror**: None

### VSync
- `lv_display_flush_ready()` called from DPI vsync callback, NOT from flush_cb
- Registered on DPI panel handle, not DBI IO handle

### Backlight
- LEDC channel 0, timer 0, 10-bit
- Safe API: `ledc_set_duty()` + `ledc_update_duty()` separately (do NOT use `ledc_set_duty_and_update()` — crashes ESP32-P4)

---

## 2. RTOS Task Architecture

| Task | Core | Priority | Stack | Purpose |
|------|------|----------|-------|---------|
| safetyTask | 0 | 5 | 4 KB | E-STOP ISR processing, state guard |
| motorTask | 0 | 4 | 5 KB | Speed apply, ADC poll, pedal, motor config |
| controlTask | 0 | 3 | 4 KB | State machine, mode logic |
| lvglTask | 1 | 2 | 64 KB | LVGL rendering, screen updates, dim, ESTOP overlay |
| storageTask | 1 | 1 | 12 KB | NVS flush (settings/presets), periodic housekeeping |

- motorTask, controlTask, safetyTask: subscribed to WDT
- lvglTask, storageTask: NOT subscribed (blocking I/O can exceed WDT timeout)
- storageTask removed from WDT after causing reboot loops during flash writes

---

## 3. Motor Control & Live Speed Adjustment

- **FastAccelStepper 0.33.x** with RMT driver on GPIO 50
- **Live speed**: `applySpeedAcceleration()` required after `setSpeedInMilliHz()` for changes during running
- **Cross-core**: Shared RPM variables use `std::atomic<float>` with explicit `.load(memory_order_*)` / `.store(...)`. All cross-core atomic flags are declared in `src/app_state.h` / defined in `src/app_state.cpp` (single source of truth).
- **Stepper mutex**: `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex) protects all stepper calls — uses `xSemaphoreTake`/`xSemaphoreGive`, keeps interrupts enabled during cross-core contention. Non-motor modules should call `motor_set_target_milli_hz()` instead of taking the mutex directly (it wraps `setSpeedInMilliHz` + `applySpeedAcceleration`).
- **Motion-start guard**: start paths check `safety_inhibit_motion()` before ENA is enabled and again immediately after `digitalWrite(PIN_ENA, LOW)`. If E-STOP/ALM appears in that final window, ENA is driven HIGH and no run/move command is issued.
- **Control command queue**: START/STOP/JOG/program requests are sent as a single overwrite command to `controlTask`, so the latest operator command wins and parameters cannot be mixed across requests.
- **Pending-flag pattern**: UI `.store()`s atomic flags with `memory_order_release` for non-motion flags; motorTask/controlTask `.load()`s them with `memory_order_acquire` and executes within the owning task cycle.
- **Non-blocking pedal ADC**: When `ENABLE_ADS1115_PEDAL=1`, `motorTask` uses a state-machine (`ads_poll_and_start()` in `src/motor/speed.cpp`) that starts a single-shot conversion in one tick and reads the result in a later tick. Runtime I2C transactions use a short timeout; the longer timeout is reserved for `speed_init()` probe/init.

---

## 4. Safety System

- **E-STOP**: GPIO 34, NC contact, INPUT_PULLUP, hardware ISR
- **ISR**: GPIO register write (ENA HIGH) + `g_estopPending.store(true, std::memory_order_release)` + `g_wakePending.store(true, ...)` (NO function calls — flash may be disabled)
- **Boot sampling**: `safety_init()` takes 3 samples of `PIN_ESTOP` with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle, requires ≥2/3 LOW (mitigates floating GPIO34 at power-on — this pin has no internal pull-up on ESP32-P4)
- **Debounce**: 5ms in safetyTask before STATE_ESTOP transition; `g_estopTriggerMs` is set by `safetyTask` on the debounced edge (not by the ISR)
- **Stepper API safety**: safetyTask disables ENA first and only calls FastAccelStepper `forceStop()` while holding `g_stepperMutex`; no unsynchronized stepper access is used in ALM/E-STOP paths.
- **CAS transitions**: `control_transition_to()` uses `compare_exchange_strong` for race-free state changes
- **UI reset**: `g_uiResetPending.store(true, std::memory_order_release)` from UI, processed in controlTask on Core 0
- **Overlay**: lvglTask auto-shows/hides ESTOP overlay based on current state
- **Fatal errors**: Storage/motor init failures call `fatal_halt("<context>")` (in `src/app_state.h`) instead of `ESP.restart()` directly — it logs the reason via `LOG_E` (always compiled in) and drains serial before rebooting.

---

## 5. Storage & Presets

- **Backend**: **NVS** using Arduino `Preferences`, namespace `wrot`
- **Keys**: `cfg` — JSON object for `SystemSettings` (acceleration, microstep, calibration, brightness, **`color_scheme`** (0=dark / 1=light UI neutral palette), accent index, countdown, etc.); `prs` — JSON array for up to **16** presets, including per-program `workpiece_diameter_mm` (`0` = default reference diameter)
- **Serialization**: ArduinoJson → buffer → `putBytes` / `getBytes` (plaintext JSON on flash)
- **Legacy**: One-time import from LittleFS `/settings.json` and `/presets.json` if NVS keys are empty and those files exist
- **Load order / validation**: settings are loaded before presets so preset RPM clamp uses the saved `max_rpm`; invalid persisted microstep values are sanitized to 16.
- **Thread safety**: `g_nvs_mutex` around Preferences I/O; `g_presets_mutex` / `g_settings_mutex` for in-RAM structures
- **Copy-based API**: `storage_get_preset()` returns copy, never pointer into vector
- **Debounce**: ~500ms presets, ~1000ms settings in `storage_flush()` on **storageTask**
- **UI coordination**: `g_flashWriting` during NVS writes; LVGL mutex held around the flag window where required
- **storageTask**: 12KB stack, handles NVS I/O off the UI thread (not TWDT-subscribed)

---

## 6. UI Screen System

- **21 active `ScreenId` root screens** with lazy creation (only boot, main, confirm created at init) plus separate **E-STOP overlay** (`screen_estop_overlay.cpp`)
- **Screen management**: `screens_show()` dispatches create/update, tracks `screenCreated[]` array
- **Reinit**: `screens_reinit()` destroys all screens + ESTOP overlay, restores boot/main/confirm
- **Keyboard**: Deferred cleanup pattern — set flag in callback, cleanup in next update cycle
- **Confirm dialog**: Pending-flag pattern — callback sets flag, execution in update loop
- **Settings diagnostics**: `SCREEN_DIAGNOSTICS` shows live ESTOP, DM542T ALM, DIR switch, pedal switch, ENA, RPM, direction and motion-block state.
- **Event log**: `src/event_log.cpp` keeps a small mutex-protected RAM ring buffer of recent operator, program, state and fault events. Diagnostics renders the newest entries so blocked starts can be checked on the panel without serial output.
- **Pedal settings**: `SCREEN_PEDAL_SETTINGS` arms/disarms GPIO33 pedal input and shows ADS1115/analog status.
- **Theme**: neutral backgrounds and text colors are **runtime** values (`g_col_*` in `theme.h`), filled by `theme_sync_colors()` from **`NEUT_DARK`** vs **`NEUT_LIGHT`** packs according to `g_settings.color_scheme`. Accent hue comes from `accent_color` + `theme_palette[]`. **`COL_HDR_MUTED`** is used for secondary labels on the dark header strip (better contrast than `COL_TEXT_DIM` on the fixed dark header bar in both schemes).
- **Footer navigation**: BACK button at bottom of all settings screens

---

## 7. Thread Safety Patterns

### Pending-Flag Pattern (UI -> Core 0)
```cpp
// Declaration in src/app_state.h:
extern std::atomic<bool> g_uiResetPending;

// UI callback (Core 1) — release-store the flag only
void estop_reset_cb(lv_event_t* e) {
  g_uiResetPending.store(true, std::memory_order_release);
}

// Core 0 task — acquire-load and execute within the cycle
void controlTask(void* pvParameters) {
  if (g_uiResetPending.load(std::memory_order_acquire)) {
    g_uiResetPending.store(false, std::memory_order_release);
    safety_check_ui_reset();
    control_transition_to(STATE_IDLE);
  }
}
```

All cross-core flags (`g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending`, `g_flashWriting`, `g_screenRedraw`, `g_dir_switch_cache`, `motorConfigApplyPending`) live in `src/app_state.h` / `src/app_state.cpp`. Do not redeclare them in other headers.

### CAS State Transition
```cpp
bool control_transition_to(SystemState newState) {
  SystemState expected = currentState.load();
  if (!control_is_valid_transition(expected, newState)) return false;
  return currentState.compare_exchange_strong(expected, newState);
}
```

### Mutex-Protected Stepper
```cpp
// g_stepperMutex is SemaphoreHandle_t (FreeRTOS mutex, NOT spinlock)
xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
stepper->setSpeedInMilliHz(mhz);
stepper->applySpeedAcceleration();
xSemaphoreGive(g_stepperMutex);
```

### Deferred keyboard cleanup
```cpp
// Event callback — sets flag only (never delete LVGL objects synchronously here)
static void on_kb_ready_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_READY)
    kbClosePending = true;
}

// screen_*_update() — performs actual cleanup
void screen_example_update() {
  if (kbClosePending) {
    cleanup_kb();  // uses lv_obj_delete_async()
    kbClosePending = false;
  }
}
```

### Widget invalidation on screen reinit
```cpp
void screen_example_invalidate_widgets() {
  scrollPanel = nullptr;
  kb = nullptr;
  kbClosePending = false;
}
// Called from screens_reinit() to prevent dangling pointers
```

---

## 8. Known Issues & Workarounds

| Issue | Workaround |
|-------|-----------|
| `lv_display_set_rotation()` crashes ESP32-P4 | Manual rotation in flush callback |
| `ledc_set_duty_and_update()` crashes backlight | Use separate `ledc_set_duty()` + `ledc_update_duty()` |
| Legacy LittleFS data may exist from old firmware | Current firmware imports it once into NVS if NVS keys are empty |
| GPIO 28 / 14-19 / 54 may be claimed by C6 co-processor | Do not use for application GPIO without GUITION schematic; GPIO 32 is reserved for DM542T ALM in this firmware |
| High traffic on P4↔C6 bus | Keep unrelated work off that path; follow vendor layout |
| Companion MCU APIs not thread-safe with UI | Keep any future bus traffic off the LVGL thread |
| `lv_obj_set_flex_gap` doesn't exist in LVGL 9 | Use `lv_obj_set_style_pad_row/col` |
| montserrat_48+ crashes ESP32-P4 | Max font is montserrat_40 |
| `LV_SYMBOL_MINUS`/`PLUS` not in montserrat_16 | Use ASCII `"-"`/`"+"` |
| `-mfix-esp32-psram-cache-issue` crashes ESP32-P4 | RISC-V only, not Xtensa |
| `portENTER_CRITICAL` spinlock causes IWDT crash | Use FreeRTOS mutex (`SemaphoreHandle_t`) for cross-core mutexes |
| `lv_obj_delete()` in event callback crashes | Use `lv_obj_delete_async()` for self-deletion |
| Static widget pointers dangle after `screens_reinit()` | Implement `screen_*_invalidate_widgets()` called from `screens_reinit()` |

---

## Resources
- [JC4880P433C BSP](https://github.com/csvke/esp32_p4_jc4880p433c_bsp) - Reference implementation
- [ST7701 Driver](https://github.com/espressif/esp_lcd_st7701) - ESP-IDF component
- [GT911 Driver](https://github.com/espressif/esp_lcd_touch_gt911) - Touch controller
- [LVGL Documentation](https://docs.lvgl.io/) - UI framework
- [FastAccelStepper](https://github.com/hinxx/FastAccelStepper) - Motor driver
