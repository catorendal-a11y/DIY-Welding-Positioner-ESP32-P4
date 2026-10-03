# Architecture

## Dual-Core FreeRTOS Design

The controller runs on ESP32-P4's dual-core RISC-V processor.

```
CORE 0 (Real-time)              CORE 1 (UI)
========================         ========================
safetyTask  (pri 5, 4KB)        lvglTask    (pri 2, 64KB)
inputTask   (pri 4, 5KB)        storageTask (pri 1, 12KB)
controlTask (pri 3, 4KB)
```

Runtime FastAccelStepper calls have one owner: controlTask. inputTask publishes sampled inputs; the UI reads fresh coherent control snapshots. FastAccelStepper has its own priority-24 Core 0 task. A bounded boot task allocates the RMT channel on Core 0 because engine-task affinity alone does not place the IRQ. See the [1.4.0 re-audit](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/blob/master/docs/FASTACCELSTEPPER_1_4_REAUDIT.md).

**Critical rule:** All `lv_*` calls must come from `lvglTask` on Core 1 only. Motor control functions on Core 0 must never call LVGL directly.

**WDT:** inputTask, controlTask, safetyTask are subscribed to TWDT. lvglTask and storageTask are NOT subscribed (they do blocking I/O that can exceed WDT timeout).

**Flash safety:** `CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` + `CONFIG_SPIRAM_RODATA=y` keeps code reachable when flash cache is disabled during **NVS** (and any filesystem) writes. Settings/presets use NVS `putBytes`, not LittleFS `.tmp` + rename. IWDT timeout is 2000ms (`CONFIG_ESP_INT_WDT_TIMEOUT_MS=2000`).

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
| `STATE_ESTOP` | Latched fault, ENA inhibited; physical stop time unmeasured |

## Thread Safety Patterns

### Pending-Flag Pattern (UI -> Core 0)
UI callbacks on Core 1 set `std::atomic` flags with `memory_order_release`. Core 0 tasks check them with `memory_order_acquire` and execute within their cycle. `volatile` alone is insufficient on RISC-V SMP, so all cross-core flags are `std::atomic` with explicit memory ordering. The canonical list of cross-core atomics is declared in `src/app_state.h` and defined in `src/app_state.cpp` (`g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending`, `g_flashWriting`, `g_screenRedraw`, `g_dir_switch_cache`, `motorConfigApplyPending`).

```
UI callback (Core 1)                   Core 0 task (every 5ms)
─────────────────────                ────────────────────────
speed_slider_set(rpm)     ──>
  stores atomic target RPM            inputTask polls pot/pedal/input
                                      publishes sampled input snapshot
                                      controlTask computes/applies milli-Hz
                                      and publishes coherent ControlSnapshot
```

### CAS State Transition
`control_transition_to()` uses `std::atomic<SystemState>` with **`memory_order_acquire`** on the initial load, **`memory_order_acq_rel`** on successful `compare_exchange_strong`, **`memory_order_acquire`** on the failure path, and **`memory_order_release`** when publishing `previousState` — so `control_get_state()` (acquire load) sees consistent ordering on both cores.

Pseudocode shape (ordering omitted):
```
bool control_transition_to(SystemState newState) {
  SystemState expected = currentState.load(/* acquire */);
  if (!control_is_valid_transition(expected, newState)) return false;
  if (!currentState.compare_exchange_strong(expected, newState /* acq_rel / acquire */)) return false;
  previousState.store(expected /* release */);
  return true;
}
```

### Mutex-Protected Stepper
Runtime FastAccelStepper calls require controlTask ownership and bounded `g_stepperMutex` access (`SemaphoreHandle_t`, FreeRTOS mutex). Initialization precedes owner binding. A failed 2 ms lock acquisition latches a motor fault. Example:
```
if (motor_lock()) {
  // Typed move results and speed/acceleration errors are checked.
  // Runtime calls execute only in controlTask.
  xSemaphoreGive(g_stepperMutex);
}
```

Previously a `portMUX_TYPE` spinlock, which disabled interrupts and caused IWDT crashes when contended across cores. The bounded FreeRTOS mutex keeps tick interrupts enabled. ENA inhibition does not wait for the mutex; library cleanup remains in controlTask and reset waits for asynchronous queue drain.

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
| `motor.h` | Public API: `motor_init()`, `motor_read_driver_info()`, `motor_is_running()` |
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
1. **ISR layer (latency unmeasured):** GPIO 34 interrupt sets ENA HIGH via direct register write + sets `g_estopPending.store(true, std::memory_order_release)` + `g_wakePending.store(true, ...)` (NO function calls — flash may be disabled during NVS writes)
2. **Task layer:** `safetyTask` debounces and transitions to STATE_ESTOP; stores `g_estopTriggerMs` on the debounced fault edge
3. **UI layer:** `lvglTask` shows/hides red overlay based on current state
4. **Reset:** UI sets `g_uiResetPending.store(true, ...)`, Core 0 processes via `controlTask`
5. **Boot sampling:** `safety_init()` takes 3 samples of `PIN_ESTOP` with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle, requires ≥2/3 LOW before treating ESTOP as pressed (startup sampling does not verify wiring continuity)

All shared flags (`g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending`) are declared in `src/app_state.h` / defined in `src/app_state.cpp` — single source of truth.

### `src/storage/` — Persistence

- **NVS** via Arduino `Preferences`, namespace `wrot`, keys `cfg` (settings JSON blob) and `prs` (presets JSON array)
- **ArduinoJson** serializes structs to UTF-8 JSON, stored with `putBytes` / read with `getBytes`
- Up to **16** presets; mutexes: `g_nvs_mutex`, `g_presets_mutex`, `g_settings_mutex`
- Debounced writes in `storageTask`: ~**500 ms** presets, ~**1000 ms** settings (`storage_flush()`)
- **Legacy migration:** if NVS keys are empty, one-time copy from LittleFS `/settings.json` and `/presets.json` when that partition exists
- **UI:** `g_flashWriting` pauses LVGL-sensitive paths during NVS commits to reduce visible glitches
- storageTask NOT subscribed to WDT (blocking flash I/O)

### `src/ui/` — User Interface

| File | Responsibility |
|------|---------------|
| `display.cpp` | MIPI-DSI ST7701S init, GT911 touch, LEDC backlight |
| `lvgl_hal.cpp` | LVGL display driver, flush callback, dim control |
| `screens.cpp` | Screen registry, lazy creation, show/hide management |
| `theme.h` / `theme.cpp` | Color palette: runtime **`g_col_*`** from **`NEUT_DARK`/`NEUT_LIGHT`** via `theme_sync_colors()` (`color_scheme` in settings); **`COL_HDR_MUTED`** for header secondary labels; fonts; layout constants (**`MAIN_GAUGE_*`**, **`MAIN_RPM_*`**, **`JOG_RPM_*`**, settings `SET_*`, etc.) |
| `screens/` | `screen_*.cpp` — 23 registered `ScreenId` roots + ESTOP overlay module |

**V5 layout:** Large orange main speed panel with a native 104 px numeric font, idle RPM +/− controls, CW/CCW selection and wide START/STOP. Shared graphite/orange styles apply across all registered screens. Calibration uses fixed Align → Measure → Verify → Save stages; New/Edit Program separates run mode, availability and full-screen name/RPM input. Setup Wizard adds four-stage commissioning.

**USB mirror note:** mirror builds use LVGL partial rendering so `lvgl_hal.cpp`
streams dirty RGB565 rectangles to the Windows viewer instead of repeatedly
sending full 800x480 frames. PC input is registered as a second LVGL pointer
device and is accepted only while **Settings > Display > USB MIRROR** is armed.

**Calibration note:** measurements require completed moves. The correction is a runtime draft until verification passes 360 ± 0.5 degrees and its Save receipt succeeds; Restart restores the saved factor.

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
inputTask pot/pedal     ──────────>  controlTask applies speed from sampled inputs
control_start_continuous() ──────>  controlTask receives generation-checked MotionCommand
                                        motor_set_target_milli_hz() wraps stepper mutex
```

## Flash & Storage Safety

ESP32-P4 executes code from external flash. **NVS commits** (and LittleFS, if used) can temporarily disable the flash cache:

```
NVS commit / flash write  ──>  flash cache DISABLED  ──>  code in flash CRASHES
                               (code in PSRAM OK)
```

Mitigations:
1. **`CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y` + `CONFIG_SPIRAM_RODATA=y`** — moves instructions/rodata to PSRAM
2. **Deferred UI saves** — UI sets pending flags; `storageTask` performs `putBytes` after debounce
3. **storageTask not in WDT** — blocking I/O (flash) can exceed 5s timeout
4. **`g_flashWriting`** — signals active NVS write so the display path can skip work during cache-off windows

## Timer Screen (Countdown)

SCREEN_TIMER provides a visual countdown before rotation starts:
- Configurable 1-10s delay with progress ring, pulsing number, color transition
- Starts continuous rotation at countdown zero
- Settings saved on screen exit only
