# Architecture

## Dual-Core FreeRTOS Design

The controller runs on ESP32-P4's dual-core RISC-V processor, with an ESP32-C6 co-processor connected via SDIO for WiFi and BLE.

```
CORE 0 (Real-time)              CORE 1 (UI)
========================         ========================
safetyTask  (pri 5, 4KB)        lvglTask    (pri 2, 64KB)
motorTask   (pri 4, 5KB)        storageTask (pri 1, 12KB)
controlTask (pri 3, 4KB)
```

**Critical rule:** All `lv_*` calls must come from `lvglTask` on Core 1 only. Motor control functions on Core 0 must never call LVGL directly.

**WDT:** motorTask, controlTask, safetyTask are subscribed to TWDT. lvglTask and storageTask are NOT subscribed (they do blocking I/O that can exceed WDT timeout).

**Flash safety:** `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` + `CONFIG_SPIRAM_RODATA=y` ensures code remains accessible when flash cache is disabled during LittleFS writes. All WiFi calls go through `wifi_process_pending()` (thread-safe). Storage uses .tmp + rename for atomic writes. IWDT timeout is 2000ms (`CONFIG_ESP_INT_WDT_TIMEOUT_MS=2000`).

## State Machine

State transitions use compare-and-swap (CAS) for race-free multi-core access:

```
IDLE ──> RUNNING ──> STOPPING ──> IDLE
IDLE ──> PULSE   ──> STOPPING ──> IDLE
IDLE ──> STEP    ──> STOPPING ──> IDLE
IDLE ──> JOG     ──> STOPPING ──> IDLE
IDLE ──> TIMER   ──> STOPPING ──> IDLE
Any  ──> ESTOP   ──> IDLE (manual reset via Core 0)
```

| State | Description |
|-------|-------------|
| `STATE_IDLE` | Motor disabled (ENA HIGH), ready for command |
| `STATE_RUNNING` | Continuous rotation, speed adjustable live |
| `STATE_PULSE` | Rotate/pause cycles with configurable on/off times |
| `STATE_STEP` | Rotate exact angle, then stop |
| `STATE_JOG` | Run while button held, stop on release |
| `STATE_TIMER` | Run for configurable duration |
| `STATE_STOPPING` | Decelerating to stop (smooth ramp) |
| `STATE_ESTOP` | Emergency stop, motor halted instantly |

## Thread Safety Patterns

### Pending-Flag Pattern (UI -> Core 0)
UI callbacks on Core 1 set volatile flags. Core 0 tasks check and execute within their cycle:
```
UI callback (Core 1)              Core 0 task (every 5ms)
─────────────────              ────────────────────────
speed_slider_set(rpm)  ──>
  sets std::atomic sliderRPM
  speed_request_update()  ──>  speed_apply()
  sets speedUpdatePending         checks flag, applies speed
```

### CAS State Transition
```
bool control_transition_to(SystemState newState) {
  SystemState expected = currentState.load();
  if (!control_is_valid_transition(expected, newState)) return false;
  return currentState.compare_exchange_strong(expected, newState);
}
```

### Mutex-Protected Stepper
All FastAccelStepper calls wrapped in `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex — NOT a spinlock):
```
xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
stepper->setSpeedInMilliHz(mhz);
stepper->applySpeedAcceleration();
xSemaphoreGive(g_stepperMutex);
```

Previously a `portMUX_TYPE` spinlock, which disabled interrupts and caused IWDT crashes when contended across cores. FreeRTOS mutex blocks via scheduler, keeping tick interrupts enabled.

### Storage Mutex
Preset vector access protected by `g_presets_mutex` semaphore. Copy-based API: `storage_get_preset()` returns a copy, never a pointer.

### LVGL Object Deletion Safety
- **Never use `lv_obj_delete()` in event callbacks** for the triggering object — use `lv_obj_delete_async()`
- Keyboard/numpad cleanup uses `*ClosePending` flags, actual deletion deferred to `screen_*_update()` cycle
- `screens_reinit()` calls `screen_*_invalidate_widgets()` for all screens with static widget pointers

## Module Breakdown

### `src/motor/` — Motor Control

| File | Responsibility |
|------|---------------|
| `motor.cpp` | FastAccelStepper init, enable/disable, direction, ESTOP halt |
| `motor.h` | Public API: `motor_init()`, `motor_get_stepper()`, `motor_is_running()` |
| `speed.cpp` | RPM-to-Hz conversion, ADC pot filtering, direction switch, pedal |
| `microstep.cpp` | Microstepping config persistence |
| `calibration.cpp` | Calibration factor validation |

### `src/control/` — State Machine

| File | Responsibility |
|------|---------------|
| `control.cpp` | State transitions (CAS), mode dispatch, controlTask cycle |
| `control.h` | `SystemState` enum, mode control functions |
| `modes/` | continuous, jog, pulse, step_mode, timer |

### `src/safety/` — Emergency Stop

Two-layer ESTOP architecture:
1. **ISR layer (<0.5ms):** GPIO 34 interrupt sets ENA HIGH via direct register write + sets `g_estopPending` flag (NO function calls)
2. **Task layer (<5ms):** `safetyTask` debounces and transitions to STATE_ESTOP
3. **UI layer:** `lvglTask` shows/hides red overlay based on current state
4. **Reset:** UI sets `g_uiResetPending`, Core 0 processes via `controlTask`

### `src/storage/` — Persistence

- **LittleFS** filesystem for non-volatile storage
- **ArduinoJson** for structured data serialization
- 16 program presets max, system settings
- Debounced writes: 500ms presets, 1000ms settings
- Atomic writes: .tmp + rename pattern prevents corruption on power failure
- WiFi thread safety: all WiFi API calls via `wifi_process_pending()`, cached values for UI
- storageTask NOT subscribed to WDT (does blocking I/O)

### `src/ble/` — Bluetooth

- **NimBLE** via Arduino BLE (baked into arduino-esp32 3.3.x)
- **ESP-Hosted** SDIO transport to ESP32-C6 co-processor
- **NUS service:** Command (RX), State+RPM+Direction (TX)
- **Rate limiting:** Notify at 500ms max to avoid SDIO saturation
- **Pending flags:** Write callbacks set flags, processed in `ble_update()`

### `src/ui/` — User Interface

| File | Responsibility |
|------|---------------|
| `display.cpp` | MIPI-DSI ST7701S init, GT911 touch, LEDC backlight |
| `lvgl_hal.cpp` | LVGL display driver, flush callback, dim control |
| `screens.cpp` | Screen registry, lazy creation, show/hide management |
| `theme.h` | Color palette, font definitions, constants |
| `screens/` | 23 screen files (main, settings, modes, countdown, etc.) |

## Display Pipeline

Physical panel is 480x800 portrait. LVGL renders at 800x480 landscape with manual rotation in the flush callback:

```
LVGL (800x480) --> flush_cb rotates pixels --> DPI panel (480x800)

Rotation formula:
  portrait_x = landscape_y
  portrait_y = 799 - landscape_x
```

**Do NOT use `lv_display_set_rotation()`** — it crashes ESP32-P4.

## Task Communication

```
UI (Core 1)                         Motor (Core 0)
─────────────                        ─────────────
speed_slider_set(rpm)  ──────────>  speed_get_target_rpm() reads atomic
speed_request_update() ──────────>  speed_apply() checks flag
control_start_continuous() ──────>  controlTask transitions state (CAS)
                                        motorTask applies speed
```

## Flash & Storage Safety

ESP32-P4 executes code from external flash. LittleFS writes temporarily disable the flash cache:

```
LittleFS.write()  ──>  flash cache DISABLED  ──>  code in flash CRASHES
                        (code in PSRAM OK)
```

Mitigations:
1. **`CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` + `CONFIG_SPIRAM_RODATA=y`** — moves instructions/rodata to PSRAM
2. **Deferred UI saves** — never call `storage_save_*()` from LVGL event callbacks
3. **storageTask not in WDT** — blocking I/O (flash, SDIO) can exceed 5s timeout
4. **WiFi single-threaded** — all WiFi calls through `wifi_process_pending()` in storageTask

## Timer Screen (Countdown)

SCREEN_TIMER provides a visual countdown before rotation starts:
- Configurable 1-10s delay with progress ring, pulsing number, color transition
- Starts continuous rotation at countdown zero
- Settings saved on screen exit only
