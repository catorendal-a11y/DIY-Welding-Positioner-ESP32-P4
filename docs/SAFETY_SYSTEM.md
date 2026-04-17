# Safety System & Fault Tolerance

The TIG Rotator Controller implements a multi-layer safety architecture to protect both the operator and the mechanical hardware in industrial welding environments.

## 1. Safety State Machine
The system transitions through a rigorous state machine (defined in `control.h`) using compare-and-swap (CAS) for race-free transitions:
- **STATE_IDLE:** Default state. Motor ENA is HIGH (Disabled).
- **STATE_RUNNING:** Continuous rotation. Acceleration ramps active.
- **STATE_PULSE:** Pulse mode (ON/OFF cycles).
- **STATE_STEP:** Step mode (exact angle rotation).
- **STATE_JOG:** Touch-and-hold jog control.
- **STATE_TIMER:** Timed rotation with auto-stop.
- **STATE_STOPPING:** Deceleration ramp before IDLE.
- **STATE_ESTOP:** CRITICAL FAULT. All motion halted instantly.

## 2. Emergency Stop (E-STOP)
- **Hardware Interrupt:** GPIO 34 configured with interrupt (`FALLING` edge — transition **HIGH → LOW**).
- **Active LOW on fault:** Wiring must match firmware: **safe / released = HIGH**, **pressed / fault = LOW** (same sense as `digitalRead(PIN_ESTOP)==LOW` meaning “active” in code).
- **ISR Response (<0.5ms):** Direct GPIO register write sets ENA HIGH (Disabled) + `g_estopPending.store(true, std::memory_order_release)` + `g_wakePending.store(true, ...)` so a dimmed panel is woken on the next `dim_update()` / overlay path. NO function calls in ISR (flash may be disabled during NVS or other flash writes).
- **State Transition (<5ms):** `safetyTask` (priority 5) checks the pending flag with debounce, stores `g_estopTriggerMs` on the debounced edge, and transitions to STATE_ESTOP via CAS.
- **Boot-time sampling:** `safety_init()` samples `PIN_ESTOP` 3× with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle; requires ≥2/3 LOW to treat ESTOP as pressed. GPIO34 has no internal pull-up on ESP32-P4, so this mitigates false ESTOPs from a floating line at power-on.
- **UI Overlay:** `lvglTask` detects STATE_ESTOP and shows full-screen red overlay on any active screen.
- **ESTOP Reset:** UI sets `g_uiResetPending.store(true, std::memory_order_release)`. `controlTask` on Core 0 calls `safety_check_ui_reset()` and transitions to STATE_IDLE. Overlay auto-hides.
- **Single source of truth:** `g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending` are declared in `src/app_state.h` / defined in `src/app_state.cpp` (all `std::atomic` with explicit memory ordering).

## 3. Acceleration & Deceleration (Anti-Stall)
- **Linear Acceleration:** Hardware-timer-based `FastAccelStepper` engine.
- **Default:** 7500 steps/s² in factory `SystemSettings` (`storage.cpp`); configurable via **Settings → Motor Config** (NVS).
- **Soft Stop:** Standard stop commands trigger deceleration ramp.
- **Live Speed Changes:** `applySpeedAcceleration()` after `setSpeedInMilliHz()` for immediate effect during rotation.

## 4. Thread Safety
- **Stepper mutex:** `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex) protects all FastAccelStepper calls. Uses `xSemaphoreTake`/`xSemaphoreGive` — keeps tick interrupts enabled during cross-core contention (prevents IWDT crashes). Non-motor modules call `motor_set_target_milli_hz()` (encapsulated lock) instead of taking the mutex directly.
- **Atomic variables:** Cross-core shared state uses `std::atomic` with explicit memory ordering. All such flags are declared in `src/app_state.h` and defined in `src/app_state.cpp` — no scattered declarations.
- **Pending-flag pattern:** UI callbacks `.store()` atomic flags with `memory_order_release`; Core 0 tasks `.load()` with `memory_order_acquire` and execute within their cycle. No direct motor calls from UI thread.
- **Storage mutex:** `g_presets_mutex` semaphore protects preset vector access.
- **LVGL safety:** `lv_obj_delete_async()` for all keyboard/numpad deletion from event callbacks. `screen_*_invalidate_widgets()` nullifies static pointers on `screens_reinit()`.

## 4a. Fatal-error handling
- Unrecoverable init failures call `fatal_halt("<context>")` (declared in `src/app_state.h`) instead of calling `ESP.restart()` directly. `fatal_halt` logs the reason via `LOG_E` (always compiled in, even in release builds) and drains the serial buffer before rebooting — so power-on loops are diagnosable in the field. Example contexts currently in use: `"storage: NVS mutex alloc"`, `"storage: NVS namespace open"`, `"motor: stepper mutex alloc"`, `"motor: FastAccelStepper init"`.

## 5. Hardware Watchdog (TWDT)
- **Motor Task:** Subscribed to WDT. Ensures pulses are generated.
- **Control Task:** Subscribed to WDT. Ensures state machine is responsive.
- **Safety Task:** Subscribed to WDT. Ensures E-STOP logic is alive.
- **Storage Task:** NOT subscribed (does blocking I/O — flash writes and related work — that can exceed WDT timeout).
- **LVGL Task:** NOT subscribed (graphics rendering must not be interrupted).

If any WDT-subscribed task freezes, the system triggers a hardware reset, disabling the motor via ENA pull-up.

## 6. EMI Protection
For detailed EMI mitigation (Shielding, Ferrites, TVS Diodes), see the [EMI Mitigation Guide](EMI_MITIGATION.md).
