# Process Flow Analysis — TIG Rotator Controller

## Task Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CORE 0 (Real-time)                              │
│                                                                             │
│  safetyTask (pri 5, 4KB)     motorTask (pri 4, 5KB)    controlTask (pri 3, 4KB)│
│  ┌─────────────────────┐    ┌──────────────────────┐    ┌────────────────────┐  │
│  │ 1ms loop            │    │ 5ms loop              │    │ 10ms loop          │  │
│  │ - WDT feed          │    │ - WDT feed            │    │ - WDT feed          │  │
│  │ - ESTOP debounce    │    │ - speed_apply()       │    │ - Process pending  │  │
│  │ - forceStop()       │    │ - ADC read (20ms)     │    │   mode requests    │  │
│  │ - state→ESTOP       │    │ - accel apply         │    │ - State transitions│  │
│  │ - reset handling    │    │ - motor config apply  │    │                    │  │
│  └─────────────────────┘    │ - pedal input         │    └────────────────────┘  │
│                              └──────────────────────┘                            │
│                                                                             │
│  ISR: estopISR (FALLING edge on GPIO34)                                     │
│  ┌─────────────────────┐                                                      │
│  │ GPIO.out1_w1ts      │  ← ONLY GPIO write + set flag                     │
│  │ g_estopPending=true │                                                      │
│  └─────────────────────┘                                                      │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                           CORE 1 (UI/IO)                                  │
│                                                                             │
│  lvglTask (pri 2, 64KB)               storageTask (pri 1, 12KB)             │
│  ┌──────────────────────────────┐    ┌──────────────────────────┐           │
│  │ 10ms loop                    │    │ 100ms loop               │           │
│  │                              │    │                          │           │
│  │ 1. screens_process_pending() │    │ 1. storage_flush()       │           │
│  │    → theme_reinit (rare)     │    │    → LittleFS write      │           │
│  │    → screens_show()          │    │    → .tmp + rename       │           │
│  │                              │    │ 2. wifi_process_pending()│           │
│  │ 2. lvgl_lock()               │    │    → WiFi.status()       │           │
│  │    lv_timer_handler()         │    │    → scan/connect        │           │
│  │    lvgl_unlock()             │    │ 3. ble_update()          │           │
│  │                              │    │    → BLE notifications   │           │
│  │ 3. dim_update()              │    │    → pending commands    │           │
│  │                              │    │ 4. Health monitoring     │           │
│  │ 4. screens_update_current()  │    │    → stack/heap check    │           │
│  │    [every 200ms, mutex]       │    │                          │           │
│  │                              │    │ NO WDT — blocking I/O   │           │
│  │ 5. ESTOP overlay show/hide   │    └──────────────────────────┘           │
│  │    [mutex]                   │                                         │
│  │                              │    Shared SDIO bus: WiFi + BLE            │
│  │ 6. ESTOP overlay update      │    (C6 co-processor)                     │
│  │    [mutex]                   │                                         │
│  └──────────────────────────────┘                                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Data Flow: SAVE Operation (Settings)

```
User presses SAVE on any settings screen
         │
         ▼
┌─────────────────────────────────────────┐
│ Event callback (inside lv_timer_handler) │
│                                         │
│ 1. g_settings.xxx = newValue           │
│ 2. storage_save_settings()              │
│    → savePending = true (1 line)        │
│ 3. Return (no flash write!)             │
└─────────────────┬───────────────────────┘
                  │
                  │  (1 second debounce)
                  ▼
┌─────────────────────────────────────────┐
│ storageTask: storage_flush()            │
│                                         │
│ 1. Check savePending && 1s elapsed     │
│ 2. savePending = false                 │
│ 3. storage_save_settings_internal()    │
│    a. LittleFS.open("settings.tmp","w")│
│    b. serializeJson(doc, file)          │
│       → FLASH WRITE (~50-200ms)        │
│       → Flash cache DISABLED on both    │
│         cores during write              │
│    c. file.close()                     │
│    d. LittleFS.remove("settings.json") │
│    e. LittleFS.rename(".tmp",".json")  │
│ 4. LOG_I("Successfully saved...")      │
└─────────────────────────────────────────┘
```

## Data Flow: Screen Switch

```
User presses BACK button
         │
         ▼
┌─────────────────────────────────────────┐
│ Event callback (inside lv_timer_handler) │
│                                         │
│ screens_show(SCREEN_SETTINGS)           │
│  1. create_screen() if not created      │
│  2. currentScreen = SCREEN_SETTINGS     │
│  3. lv_screen_load(screenRoots[id])     │
│                                         │
│ SAFE: LVGL 9 guarantees sequential      │
│ execution within lv_timer_handler()      │
└─────────────────────────────────────────┘
```

## Data Flow: Theme Change (Deferred)

```
User presses ACCENT COLOR cycle button
         │
         ▼
┌─────────────────────────────────────────┐
│ Event callback (inside lv_timer_handler) │
│                                         │
│ 1. g_settings.accent_color = next       │
│ 2. theme_set_color(idx)                 │
│    → g_accent = new color (fast)        │
│ 3. Update button label text             │
│ 4. themeRefreshPending = true           │
│ 5. Return                              │
└─────────────────┬───────────────────────┘
                  │
                  │  (next 10ms loop iteration)
                  ▼
┌─────────────────────────────────────────┐
│ lvglTask: screens_process_pending()     │
│ (OUTSIDE mutex — no LVGL rendering)     │
│                                         │
│ 1. themeRefreshPending = false         │
│ 2. theme_refresh()                     │
│    → screens_reinit()                   │
│      a. estop_overlay_destroy()         │
│      b. lv_obj_delete() ALL screens    │
│      c. screens_init()                  │
│      d. screens_show(prev)              │
│                                         │
│ SAFE: No LVGL rendering active         │
│ Next lv_timer_handler() will render     │
│ fresh screens with new colors           │
└─────────────────────────────────────────┘
```

## Data Flow: ESTOP Activation

```
ESTOP button pressed (GPIO34 LOW)
         │
         ▼
┌─────────────────────────────────────────┐
│ estopISR (interrupt, IRAM_ATTR)         │
│                                         │
│ 1. GPIO.out1_w1ts = ENA HIGH           │
│    → Motor disabled (direct register)  │
│ 2. g_estopPending = true               │
│                                         │
│ TOTAL: ~2-3 CPU cycles                  │
│ NO function calls, NO flash access      │
└─────────────────┬───────────────────────┘
                  │
                  │  (next 1ms safetyTask loop)
                  ▼
┌─────────────────────────────────────────┐
│ safetyTask (Core 0, priority 5)         │
│                                         │
│ 1. WDT feed                            │
│ 2. if (g_estopPending)                 │
│    a. g_estopTriggerMs = millis()      │
│    b. estopStepper->forceStop()         │
│    c. Wait 5ms (debounce)              │
│    d. Verify PIN still LOW              │
│    e. control_transition_to(STATE_ESTOP)│
│       → motor_halt()                   │
│       → estopLocked = true             │
│    f. g_estopPending = false            │
│    g. g_estopTriggerMs = 0             │
└─────────────────────────────────────────┘
```

---

## KNOWN PROBLEMS & RISK AREAS

### PROBLEM 1: LittleFS Flash Write Disables CPU Cache (CRITICAL)

**Severity**: HIGH — causes blue screen flash, potential IWDT reboot
**Status**: PARTIALLY MITIGATED

When `storage_save_settings_internal()` or `storage_save_presets_internal()` writes to flash:
- SPI flash cache is **disabled on BOTH cores** for the duration of the write
- Any code in flash (not PSRAM) will crash if executed during this window
- LittleFS write takes 50-200ms depending on data size
- `serializeJson()` builds the entire JSON in RAM first, then writes — the write itself is a single `file.write()` call

**Mitigations applied**:
- `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` + `CONFIG_SPIRAM_RODATA=y` — moves code/rodata to PSRAM
- ESTOP ISR contains NO function calls (only GPIO + flag)
- IWDT timeout increased to 2000ms
- storageTask NOT subscribed to WDT

**REMAINING RISK**:
- `lv_timer_handler()` runs on Core 1 and may call LVGL code in flash during a write
- LVGL tick timer callback (`lvgl_tick_cb`) is `IRAM_ATTR` — safe
- But LVGL rendering code (widget draw functions) is in flash — if `lv_timer_handler()` is called while storageTask is writing, LVGL code in flash may crash
- `dim_update()` runs OUTSIDE mutex — calls `display_set_brightness()` which may be in flash
- `g_presets_mutex` is held during `storage_save_presets_internal()` — if UI code tries to take this mutex while storageTask holds it, Core 1 blocks. But Core 0 tasks don't take this mutex.

**FIX NEEDED**: `lv_timer_handler()` and `dim_update()` should NOT run during flash writes. Options:
1. Add a `volatile bool flashWriteInProgress` flag that `lvglTask` checks before calling `lv_timer_handler()`
2. Or: accept the blue flash as cosmetic (no crash if all ISR code is in IRAM/PSRAM)

### PROBLEM 2: `dim_update()` Runs Outside Mutex (MEDIUM)

**Severity**: MEDIUM — touches display hardware without mutex protection
**Status**: NOT FIXED

`dim_update()` is called on line 102 of main.cpp, AFTER `lvgl_unlock()` and BEFORE the 200ms update check. It calls:
- `display_set_brightness()` — LEDC PWM, probably safe
- `esp_lcd_touch_read_data()` + `esp_lcd_touch_get_coordinates()` — I2C touch, NOT thread-safe with LVGL's internal touch processing

If `lv_timer_handler()` processes a touch event while `dim_update()` is reading touch data, the GT911 I2C bus gets concurrent access → garbage touch data or I2C bus lockup.

**FIX**: Move `dim_update()` inside the mutex, or add a separate mutex for touch I2C.

### PROBLEM 3: `control_get_state()` Called Outside Mutex (LOW)

**Severity**: LOW — reads atomic variable, probably safe
**Status**: NOT FIXED

Line 114: `SystemState state = control_get_state();` is called outside mutex.
`control_get_state()` reads `std::atomic<SystemState>` which is lock-free — safe.
But the subsequent `estop_overlay_show/hide/update()` calls are properly mutex-wrapped.

### PROBLEM 4: `motor_is_running()` Uses Critical Section (FIXED)

**Severity**: LOW — was short critical sections
**Status**: FIXED (2026-04-05)

`motor_is_running()` and `motor_get_current_hz()` previously used `portENTER_CRITICAL` (spinlock) which disables interrupts on the calling core. When called from Core 1 (UI) while Core 0 held the spinlock, both cores had interrupts disabled — triggering IWDT crashes under cross-core contention.

**Fix**: `g_stepperMutex` replaced from `portMUX_TYPE` (spinlock) to `SemaphoreHandle_t` (FreeRTOS mutex). All `portENTER_CRITICAL`/`portEXIT_CRITICAL` replaced with `xSemaphoreTake`/`xSemaphoreGive`. FreeRTOS mutex blocks via scheduler without disabling interrupts.

### PROBLEM 5: `g_stepperMutex` is `portMUX_TYPE` Not `SemaphoreHandle_t` (FIXED)

**Severity**: WAS LOW, turned out to be CRITICAL — caused IWDT crash on Core 0
**Status**: FIXED (2026-04-05)

`g_stepperMutex` was a spinlock (`portMUX_TYPE`) used with `portENTER_CRITICAL`. When contended across both cores (Core 1 UI calling `motor_is_running()` while Core 0 motorTask holds lock), interrupts were disabled on both cores simultaneously. This blocked the IPC task and tick interrupt on Core 0, triggering "Interrupt wdt timeout on CPU0" after 2000ms.

**Fix**: Changed to `SemaphoreHandle_t` mutex. All 16 instances of `portENTER_CRITICAL`/`portEXIT_CRITICAL` across `motor.cpp`, `speed.cpp`, `step_mode.cpp`, `jog.cpp`, and `pulse.cpp` replaced with `xSemaphoreTake`/`xSemaphoreGive`. FreeRTOS mutex has priority inheritance and blocks via scheduler, keeping tick interrupts enabled.

### PROBLEM 6: `storage_save_presets_internal()` Holds Mutex During Flash Write (MEDIUM)

**Severity**: MEDIUM — blocks UI if it tries to access presets
**Status**: NOT FIXED

`storage_save_presets_internal()` (storage.cpp:105) takes `g_presets_mutex` at the start and releases it after the entire write + rename sequence. If any UI code (Core 1) tries to take this mutex during the write, it blocks.

Currently, only `screen_programs.cpp` and `screen_program_edit.cpp` take `g_presets_mutex` from the UI. These are event callbacks inside `lv_timer_handler()`. If `storage_flush()` is writing presets while the user clicks a program, the UI callback blocks until the write completes (50-200ms).

**FIX**: Copy presets to local buffer, release mutex, then write from buffer.

### PROBLEM 7: `wifi_process_pending()` Calls WiFi API Every 2s (LOW)

**Severity**: LOW — ESP-Hosted WiFi is on shared SDIO bus
**Status**: ACCEPTABLE

`WiFi.status()` (storage.cpp:331) goes through the SDIO bus to the C6 co-processor. This is a quick poll but still uses the shared bus. If `ble_update()` sends BLE data at the same time, there could be bus contention.

Current mitigation: WiFi poll interval is 2s (not 100ms). This reduces contention.

### PROBLEM 8: `ble_update()` Runs in storageTask (LOW)

**Severity**: LOW — BLE and WiFi share SDIO bus
**Status**: ACCEPTABLE

BLE notifications (`bleServer->notify()`) go through the SDIO bus to the C6 co-processor. If WiFi is being polled at the same time, there could be bus contention. Both run in the same task (storageTask), so they're sequential — no actual concurrency issue.

### PROBLEM 9: `screens_reinit()` Deletes All Screens (MEDIUM)

**Severity**: MEDIUM — causes brief blue flash
**Status**: MITIGATED (deferred via screens_process_pending)

`screens_reinit()` deletes all screen objects and recreates them. During the brief moment between delete and recreate, there is no valid active screen → display shows default color (blue/black).

Mitigation: Runs in `screens_process_pending()` BEFORE `lv_timer_handler()`, so no rendering happens during the transition. The blue flash is caused by the MIPI-DSI panel showing its default color when no new frame is sent.

**FIX**: Could send a black frame before reinit, but this is cosmetic.

### PROBLEM 10: `lvgl_tick_cb` Uses `ESP_TIMER_TASK` (LOW)

**Severity**: LOW — 1ms timer
**Status**: ACCEPTABLE

The LVGL tick timer uses `ESP_TIMER_TASK` dispatch (not `ESP_TIMER_ISR`). This means `lv_tick_inc(1)` runs in the timer task context, not as an actual ISR. This is correct for ESP-IDF 5.x on ESP32-P4 where `ESP_TIMER_ISR` is not available.

The callback is marked `IRAM_ATTR` which is correct — it ensures the function is in IRAM even though it runs in task context.

### PROBLEM 11: `setup()` Calls WiFi Functions Before Tasks Created (LOW)

**Severity**: LOW — only runs once at boot
**Status**: ACCEPTABLE

`setup()` calls `WiFi.begin()` and `ble_init()` before any FreeRTOS tasks are created. This is safe because no concurrent access is possible at this point.

### PROBLEM 12: `extern bool wifiEnabled` in main.cpp (BUG)

**Severity**: LOW — unused variable
**Status**: NOT FIXED

Line 25: `extern bool wifiEnabled;` — this variable was removed from storage.cpp and replaced with `g_settings.wifi_enabled`. This extern reference will cause a linker error or reference a different symbol. It appears to be unused (no code references `wifiEnabled` in main.cpp).

---

## MUTEX OWNERSHIP MAP

| Mutex | Owner | Used by | Notes |
|---|---|---|---|
| `g_lvgl_mutex` (recursive) | lvglTask | lv_timer_handler, screens_update_current, estop overlay | Event callbacks run inside this mutex (called from lv_timer_handler) |
| `g_stepperMutex` (FreeRTOS mutex) | motorTask | All motor functions, speed_apply, jog, pulse, step_mode | xSemaphoreTake/Give — scheduler-based, interrupts stay enabled |
| `g_presets_mutex` | storageTask | storage_save/load_presets_internal | Held during flash write (PROBLEM 6) |
| (none) | storageTask | WiFi API, BLE API | Sequential in same task — no mutex needed |

---

## SHARED STATE BETWEEN CORES

| Variable | Type | Writer | Reader | Protection |
|---|---|---|---|---|
| `currentState` | `atomic<SystemState>` | controlTask | All | Lock-free atomic |
| `g_estopPending` | `volatile bool` | estopISR | safetyTask | Single writer ISR |
| `g_estopTriggerMs` | `volatile uint32_t` | safetyTask | safetyTask | Single task |
| `sliderRPM` | `atomic<float>` | lvglTask (callbacks) | motorTask | Lock-free atomic |
| `cachedTargetRpm` | `atomic<float>` | motorTask | motorTask | Lock-free atomic |
| `jogRPM` | `atomic<float>` | lvglTask (callbacks) | jog.cpp | Lock-free atomic |
| `pendingJogSpeed` | `atomic<float>` | lvglTask (callbacks) | motorTask | Lock-free atomic |
| `motorConfigApplyPending` | `atomic<bool>` | lvglTask (callbacks) | motorTask | Lock-free atomic |
| `g_settings` | plain struct | lvglTask + storageTask | All | NOT protected — single-writer-at-a-time assumed |
| `g_presets` | vector | storageTask + lvglTask | lvglTask | `g_presets_mutex` |
| `g_accent` | `lv_color_t` | lvglTask | lvglTask (via COL_ACCENT) | No mutex needed — same core |

---

## RECOMMENDED FIXES (Priority Order)

### 1. Add flash write guard to lvglTask (fixes blue screen)

```cpp
// In storage.cpp:
volatile bool g_flashWriteInProgress = false;

void storage_save_settings_internal() {
  g_flashWriteInProgress = true;
  // ... existing write code ...
  g_flashWriteInProgress = false;
}

// In main.cpp lvglTask loop:
for (;;) {
  if (!g_flashWriteInProgress) {
    screens_process_pending();
    lvgl_lock();
    lv_timer_handler();
    lvgl_unlock();
  }
  dim_update();
  // ...
}
```

### 2. Move dim_update() inside mutex

```cpp
for (;;) {
  screens_process_pending();
  lvgl_lock();
  lv_timer_handler();
  dim_update();          // ← move here (inside mutex)
  screens_update_current();
  // ESTOP overlay...
  lvgl_unlock();
  vTaskDelayUntil(&t, pdMS_TO_TICKS(10));
}
```

### 3. Remove `extern bool wifiEnabled` from main.cpp

Line 25 is a dead reference to a removed variable.

### 4. Release g_presets_mutex before flash write

```cpp
static bool storage_save_presets_internal() {
  // Copy presets to local buffer
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  std::vector<Preset> localCopy = g_presets;  // deep copy
  xSemaphoreGive(g_presets_mutex);

  // Write from local copy (no mutex held during flash write)
  File file = LittleFS.open(PRESETS_FILE ".tmp", FILE_WRITE);
  // ... serialize localCopy ...
}
```

### 5. Consider: lv_timer_handler() timeout

If `lv_timer_handler()` takes too long (complex screen with many widgets), the 10ms loop can't keep up. Add a timeout or skip frames:

```cpp
uint32_t handlerStart = millis();
lv_timer_handler();
uint32_t handlerMs = millis() - handlerStart;
if (handlerMs > 50) LOG_W("LVGL handler took %lums", handlerMs);
```
