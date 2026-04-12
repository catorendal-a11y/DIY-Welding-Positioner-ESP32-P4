# TIG Rotator Controller - Project Implementation

**Board**: GUITION JC4880P443C ESP32-P4 4.3" Touch Display (with ESP32-C6 co-processor)
**Display**: ST7701S 480x800 MIPI-DSI (rotated to 800x480 landscape)
**Touch**: GT911 capacitive touch controller
**Firmware**: v2.0.3 (`FW_VERSION` in `src/config.h`)

---

## Table of Contents
1. [MIPI-DSI Display Setup](#1-mipi-dsi-display-setup)
2. [RTOS Task Architecture](#2-rtos-task-architecture)
3. [Motor Control & Live Speed Adjustment](#3-motor-control--live-speed-adjustment)
4. [Safety System](#4-safety-system)
5. [BLE Remote Control](#5-ble-remote-control)
6. [WiFi Connectivity](#6-wifi-connectivity)
7. [Storage & Presets](#7-storage--presets)
8. [UI Screen System](#8-ui-screen-system)
9. [Thread Safety Patterns](#9-thread-safety-patterns)
10. [Known Issues & Workarounds](#10-known-issues--workarounds)

---

## 1. MIPI-DSI Display Setup

### Hardware Configuration
- **Bus**: 2-lane MIPI-DSI — `config.h` sets **1 Gbps per lane** nominal (`MIPI_DSI_LANE_BITRATE`); actual PHY negotiation follows ESP-IDF / board config
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
| storageTask | 1 | 1 | 12 KB | NVS flush (settings/presets), WiFi process, BLE update, health monitoring |

- motorTask, controlTask, safetyTask: subscribed to WDT
- lvglTask, storageTask: NOT subscribed (blocking I/O can exceed WDT timeout)
- storageTask removed from WDT after causing reboot loops during flash writes

---

## 3. Motor Control & Live Speed Adjustment

- **FastAccelStepper 0.33.x** with RMT driver on GPIO 50
- **Live speed**: `applySpeedAcceleration()` required after `setSpeedInMilliHz()` for changes during running
- **Cross-core**: Shared RPM variables use `std::atomic<float>` with `.load()`/`.store()`
- **Stepper mutex**: `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex) protects all stepper calls — uses `xSemaphoreTake`/`xSemaphoreGive`, keeps interrupts enabled during cross-core contention
- **Pending-flag pattern**: UI sets volatile flags, motorTask executes within 5ms cycle

---

## 4. Safety System

- **E-STOP**: GPIO 34, NC contact, INPUT_PULLUP, hardware ISR
- **ISR**: GPIO register write (ENA HIGH) + `g_estopPending` + `g_wakePending` (NO function calls — flash may be disabled)
- **Debounce**: 5ms in safetyTask before STATE_ESTOP transition
- **CAS transitions**: `control_transition_to()` uses `compare_exchange_strong` for race-free state changes
- **UI reset**: `g_uiResetPending` flag from UI, processed in controlTask on Core 0
- **Overlay**: lvglTask auto-shows/hides ESTOP overlay based on current state

---

## 5. BLE Remote Control

- **Stack**: NimBLE via Arduino BLE (baked into arduino-esp32 3.3.x)
- **Transport**: ESP-Hosted SDIO to ESP32-C6 co-processor (shared with WiFi)
- **Service**: Custom UUID `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **NUS**: TX (6E400002, NOTIFY), RX (6E400003, WRITE)
- **Commands**: 0x00=Arm, F=Start, S/X=Stop, R=CW, L=CCW, B=Reverse+Start
- **Security**: MITM bonding, passkey 123456
- **Rate limit**: Notify at 500ms max to avoid SDIO saturation
- **Pending flags**: BLE write callbacks set flags, processed in `ble_update()` on storageTask

---

## 6. WiFi Connectivity

- **Stack**: ESP-Hosted via C6 co-processor (same SDIO as BLE)
- **Init order**: `WiFi.begin()` MUST be called before `BLEDevice::init()`
- **Thread safety**: ALL WiFi calls via `wifi_process_pending()` in storageTask — never from UI thread
- **Scan**: Async `WiFi.scanNetworks(true)`, results cached
- **Polling**: WiFi.status() every 2s (reduced from 100ms to avoid SDIO blocking)
- **Settings**: When WiFi is enabled in a build, credential persistence follows that build’s storage layer; mainline settings/presets use **NVS** (`wrot` / `cfg`), not `/settings.json`

---

## 7. Storage & Presets

- **Backend**: **NVS** using Arduino `Preferences`, namespace `wrot`
- **Keys**: `cfg` — JSON object for `SystemSettings` (acceleration, microstep, calibration, brightness, themes, countdown, etc.); `prs` — JSON array for up to **16** presets
- **Serialization**: ArduinoJson → buffer → `putBytes` / `getBytes` (plaintext JSON on flash)
- **Legacy**: One-time import from LittleFS `/settings.json` and `/presets.json` if NVS keys are empty and those files exist
- **Thread safety**: `g_nvs_mutex` around Preferences I/O; `g_presets_mutex` / `g_settings_mutex` for in-RAM structures
- **Copy-based API**: `storage_get_preset()` returns copy, never pointer into vector
- **Debounce**: ~500ms presets, ~1000ms settings in `storage_flush()` on **storageTask**
- **UI coordination**: `g_flashWriting` during NVS writes; LVGL mutex held around the flag window where required
- **storageTask**: 12KB stack, handles NVS I/O off the UI thread (not TWDT-subscribed)

---

## 8. UI Screen System

- **19 `ScreenId` root screens** with lazy creation (only boot, main, confirm created at init) plus separate **E-STOP overlay** (`screen_estop_overlay.cpp`)
- **Screen management**: `screens_show()` dispatches create/update, tracks `screenCreated[]` array
- **Reinit**: `screens_reinit()` destroys all screens + ESTOP overlay, restores boot/main/confirm
- **Keyboard**: Deferred cleanup pattern — set flag in callback, cleanup in next update cycle
- **Confirm dialog**: Pending-flag pattern — callback sets flag, execution in update loop
- **Footer navigation**: BACK button at bottom of all settings screens

---

## 9. Thread Safety Patterns

### Pending-Flag Pattern (UI -> Core 0)
```cpp
// UI callback (Core 1) — sets flag only
volatile bool g_uiResetPending = false;
void estop_reset_cb(lv_event_t* e) { g_uiResetPending = true; }

// Core 0 task — executes within cycle
void controlTask(void* pvParameters) {
  if (g_uiResetPending) {
    g_uiResetPending = false;
    safety_check_ui_reset();
    control_transition_to(STATE_IDLE);
  }
}
```

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

### Deferred Keyboard Cleanup
```cpp
// Event callback — sets flag only
static void wifi_kb_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_READY) {
    wifiConnectOnClose = true;
    kbClosePending = true;
  }
}

// Update function — performs actual cleanup
void screen_wifi_update() {
  if (kbClosePending) {
    if (wifiConnectOnClose) connect_wifi();
    cleanup_kb();  // uses lv_obj_delete_async()
  }
}
```

### Widget Invalidation on Screen Reinit
```cpp
void screen_wifi_invalidate_widgets() {
  scrollPanel = nullptr; kb = nullptr; passTa = nullptr;
  // ... null all static widget pointers ...
  kbClosePending = false;
  wifiConnectOnClose = false;
}
// Called from screens_reinit() to prevent dangling pointers
```

---

## 10. Known Issues & Workarounds

| Issue | Workaround |
|-------|-----------|
| `lv_display_set_rotation()` crashes ESP32-P4 | Manual rotation in flush callback |
| `ledc_set_duty_and_update()` crashes backlight | Use separate `ledc_set_duty()` + `ledc_update_duty()` |
| `LittleFS.rename()` crashes ESP32-P4 | Use direct FILE_WRITE (not atomic) |
| GPIO 28/32 claimed by C6 co-processor | Do not use for GPIO — reserved for WiFi/BLE SDIO transport |
| BLE notify floods SDIO | Rate-limit to 500ms |
| WiFi API not thread-safe | All calls via `wifi_process_pending()` on storageTask |
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
