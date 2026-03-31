# Architecture

## Dual-Core FreeRTOS Design

The controller runs on ESP32-P4's dual-core RISC-V processor, separating real-time motor control from the heavy UI rendering.

```
CORE 0 (Real-time)              CORE 1 (UI)
========================         ========================
safetyTask  (pri 5, 2KB)        lvglTask    (pri 1, 64KB)
motorTask   (pri 4, 5KB)        storageTask (pri 1, 4KB)
controlTask (pri 3, 4KB)
```

**Critical rule:** All `lv_*` calls must come from `lvglTask` on Core 1 only. Motor control functions on Core 0 must never call LVGL directly.

## State Machine

```
IDLE ──> RUNNING ──> STOPPING ──> IDLE
IDLE ──> PULSE   ──> STOPPING ──> IDLE
IDLE ──> STEP    ──> STOPPING ──> IDLE
IDLE ──> JOG     ──> STOPPING ──> IDLE
IDLE ──> TIMER   ──> STOPPING ──> IDLE
Any  ──> ESTOP   ──> IDLE (manual reset)
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

## Module Breakdown

### `src/motor/` — Motor Control

| File | Responsibility |
|------|---------------|
| `motor.cpp` | FastAccelStepper init, enable/disable, direction, ESTOP halt |
| `motor.h` | Public API: `motor_init()`, `motor_get_stepper()`, `motor_is_running()` |
| `speed.cpp` | RPM-to-Hz conversion, ADC pot filtering, live speed apply |
| `speed.h` | `speed_apply()`, `speed_request_update()`, `speed_slider_set()` |
| `microstep.cpp` | EEPROM persistence of microstepping setting |

**Speed control flow:**
```
UI callback (Core 1)              motorTask (Core 0, every 5ms)
─────────────────────             ─────────────────────────────
speed_slider_set(rpm)  ──>
  sets volatile sliderRPM
  sets volatile buttonsActive
  speed_request_update()  ──>    speed_apply()
  sets speedUpdatePending           reads volatile sliderRPM
                                     setSpeedInMilliHz()
                                     applySpeedAcceleration()
```

**Thread safety:** `sliderRPM`, `buttonsActive`, `lastPotAdc` are `volatile` for cross-core visibility. UI never calls `speed_apply()` directly — uses `speed_request_update()` flag instead.

### `src/control/` — State Machine

| File | Responsibility |
|------|---------------|
| `control.cpp` | State transitions, mode dispatch, controlTask (10ms cycle) |
| `control.h` | `SystemState` enum, mode control functions |
| `modes/continuous.cpp` | Continuous rotation start/stop |
| `modes/pulse.cpp` | Pulse mode timing logic |
| `modes/step.cpp` | Step mode angle accumulation |
| `modes/jog.cpp` | Jog mode (press-and-hold) |
| `modes/timer.cpp` | Timer mode countdown |

### `src/safety/` — Emergency Stop

Two-layer ESTOP architecture:
1. **ISR layer (<0.5ms):** GPIO 33 falling-edge interrupt calls `stepper->forceStop()` and sets ENA HIGH
2. **Task layer:** `safetyTask` monitors ESTOP pin state, transitions to `STATE_ESTOP`, updates UI

### `src/storage/` — Persistence

- **LittleFS** filesystem for non-volatile storage
- **ArduinoJson** for structured data serialization
- 16 program presets max
- System settings (microstepping, button enable, etc.)

### `src/ui/` — User Interface

| File | Responsibility |
|------|---------------|
| `display.cpp` | MIPI-DSI ST7701S init, GT911 touch init |
| `lvgl_hal.cpp` | LVGL display driver, flush callback with manual rotation |
| `screens.cpp` | Screen registry, show/hide management |
| `theme.h` | Color palette, font definitions, constants |
| `screens/screen_main.cpp` | Main screen: gauge, buttons, pulse controls |
| `screens/screen_*.cpp` | 12+ additional screens |

## Display Pipeline

Physical panel is 480x800 portrait. LVGL renders at 800x480 landscape with manual rotation in the flush callback:

```
LVGL (800x480) --> flush_cb rotates pixels --> DPI panel (480x800)

Rotation formula:
  portrait_x = landscape_y
  portrait_y = 799 - landscape_x

Touch inverse:
  landscape_x = 799 - portrait_y
  landscape_y = portrait_x
```

**Do NOT use `lv_display_set_rotation()`** — it causes Load access fault on ESP32-P4.

## Task Communication

```
UI (Core 1)                         Motor (Core 0)
─────────────                        ─────────────
speed_slider_set(rpm)  ──────────>  speed_get_target_rpm() reads volatile
speed_request_update() ──────────>  speed_apply() checks flag
control_start_continuous() ──────>  controlTask transitions state
                                      motorTask applies speed
```

No mutexes needed — worst case is one 5ms tick delay for speed updates.
