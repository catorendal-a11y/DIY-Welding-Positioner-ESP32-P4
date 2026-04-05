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
- **Hardware Interrupt:** GPIO 34 configured with interrupt.
- **Normally Closed (NC) Logic:** The button holds the pin LOW. When pressed (or wire cut), pin goes HIGH, triggering the ISR.
- **ISR Response (<0.5ms):** Direct GPIO register write sets ENA HIGH (Disabled) + sets `g_estopPending` flag. NO function calls in ISR (flash may be disabled during LittleFS writes).
- **State Transition (<5ms):** `safetyTask` (priority 5) checks pending flag with debounce, transitions to STATE_ESTOP via CAS.
- **UI Overlay:** `lvglTask` detects STATE_ESTOP and shows full-screen red overlay on any active screen.
- **ESTOP Reset:** UI sets `g_uiResetPending` flag. `controlTask` on Core 0 calls `safety_check_ui_reset()` and transitions to STATE_IDLE. Overlay auto-hides.

## 3. Acceleration & Deceleration (Anti-Stall)
- **Linear Acceleration:** Hardware-timer-based `FastAccelStepper` engine.
- **Default:** 10000 steps/s^2 (configurable via Settings > Motor Config or `config.h`).
- **Soft Stop:** Standard stop commands trigger deceleration ramp.
- **Live Speed Changes:** `applySpeedAcceleration()` after `setSpeedInMilliHz()` for immediate effect during rotation.

## 4. Thread Safety
- **Stepper mutex:** `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex) protects all FastAccelStepper calls. Uses `xSemaphoreTake`/`xSemaphoreGive` — keeps tick interrupts enabled during cross-core contention (prevents IWDT crashes).
- **Atomic variables:** Cross-core shared state uses `std::atomic` with explicit memory ordering.
- **Pending-flag pattern:** UI callbacks set volatile flags, Core 0 tasks execute within their cycle. No direct motor calls from UI thread.
- **Storage mutex:** `g_presets_mutex` semaphore protects preset vector access.
- **LVGL safety:** `lv_obj_delete_async()` for all keyboard/numpad deletion from event callbacks. `screen_*_invalidate_widgets()` nullifies static pointers on `screens_reinit()`.

## 5. Hardware Watchdog (TWDT)
- **Motor Task:** Subscribed to WDT. Ensures pulses are generated.
- **Control Task:** Subscribed to WDT. Ensures state machine is responsive.
- **Safety Task:** Subscribed to WDT. Ensures E-STOP logic is alive.
- **Storage Task:** NOT subscribed (does blocking I/O — flash writes, WiFi/BLE SDIO — that can exceed WDT timeout).
- **LVGL Task:** NOT subscribed (graphics rendering must not be interrupted).

If any WDT-subscribed task freezes, the system triggers a hardware reset, disabling the motor via ENA pull-up.

## 6. EMI Protection
For detailed EMI mitigation (Shielding, Ferrites, TVS Diodes), see the [EMI Mitigation Guide](EMI_MITIGATION.md).
