# Safety System & Fault Tolerance

The TIG Rotator Controller implements a multi-layer safety architecture to protect both the operator and the mechanical hardware in industrial welding environments.

## 1. Safety State Machine
The system transitions through a rigorous state machine (defined in `control.h`) using compare-and-swap (CAS) for race-free transitions:
| State | Meaning |
|-------|---------|
| **STATE_IDLE** | Default state. Motor ENA is HIGH (Disabled). |
| **STATE_ENABLING** | An admitted start asserts ENA and waits 200 ms in DM542T mode before the first pulse. STOP cancels the start. Jog must keep receiving hold events throughout the wait. Step mode keeps ENA asserted during the dwell, so repeats do not wait again. |
| **STATE_RUNNING** | Continuous rotation. Acceleration ramps active. |
| **STATE_PULSE** | Pulse mode (ON/OFF cycles). |
| **STATE_STEP** | Step mode (exact angle rotation). |
| **STATE_JOG** | Touch-and-hold jog control. |
| **Countdown UI** | Countdown precedes an explicit continuous-mode request; it is not a separate control state. Program auto-stop is a separate setting. |
| **STATE_STOPPING** | Deceleration ramp before IDLE. |
| **STATE_ESTOP** | Latched fault. ENA is driven HIGH; mechanical stopping behavior depends on the driver and load. |

## 2. Emergency Stop (E-STOP)
| E-STOP state | `GPIO34` level |
|--------------|----------------|
| Safe / released (healthy) | `HIGH` |
| Pressed / fault | `LOW` |

> [!CAUTION]
> Wiring must match firmware: **safe / released = HIGH**, **pressed / fault = LOW** (same sense as `digitalRead(PIN_ESTOP)==LOW` meaning “active” in code). A bare NC contact to GND has the opposite logic.

- **Hardware Interrupt:** GPIO 34 configured with interrupt (`FALLING` edge — transition **HIGH → LOW**).
- **Active LOW on fault:** see the table above.
- **ISR response (latency unmeasured):** Direct GPIO register write sets ENA HIGH (Disabled) + `g_estopPending.store(true, std::memory_order_release)` + `g_wakePending.store(true, ...)` so a dimmed panel is woken on the next `dim_update()` / overlay path. NO function calls in ISR (flash may be disabled during NVS or other flash writes).
- **Scheduled state transition:** `safetyTask` (priority 5) checks the pending flag with debounce, stores `g_estopTriggerMs` on the debounced edge, and transitions to STATE_ESTOP via CAS.
- **Final motion-start guard:** continuous/jog/step start paths check E-STOP/driver ALM before enabling ENA and again immediately after ENA is pulled LOW. If the input becomes unsafe in that final window, ENA is driven HIGH again before any run/move command.
- **Boot-time sampling:** `safety_init()` samples `PIN_ESTOP` 3× with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle; requires ≥2/3 LOW to treat ESTOP as pressed. startup sampling does not verify wiring continuity, so this mitigates false ESTOPs from a floating line at power-on.
- **UI Overlay:** `lvglTask` detects STATE_ESTOP and shows the full-screen fault overlay on any active screen. It states the next action first and the cause below it. The banner is red while something blocks the reset and amber with **READY TO RESET** when only the reset is missing.
- **ESTOP Reset:** UI sets `g_uiResetPending.store(true, std::memory_order_release)`. `controlTask` on Core 0 calls `safety_check_ui_reset()` and transitions to STATE_IDLE. Overlay auto-hides.
- **Single source of truth:** `g_estopPending`, `g_estopTriggerMs`, `g_uiResetPending`, `g_wakePending` are declared in `src/app_state.h` / defined in `src/app_state.cpp` (all `std::atomic` with explicit memory ordering).

## 3. Acceleration & Deceleration (Anti-Stall)
- **Linear Acceleration:** Hardware-timer-based `FastAccelStepper` engine.
- **Default:** 7500 steps/s² in factory `SystemSettings` (`storage.cpp`); configurable via **Settings → Motor Config** (NVS).
- **Soft Stop:** Standard stop commands trigger deceleration ramp.
- **Live Speed Changes:** `applySpeedAcceleration()` after `setSpeedInMilliHz()` for immediate effect during rotation.

## 4. Thread Safety
- **Stepper mutex:** `g_stepperMutex` (`SemaphoreHandle_t`, FreeRTOS mutex) protects all FastAccelStepper calls. Uses `xSemaphoreTake`/`xSemaphoreGive` — keeps tick interrupts enabled during cross-core contention (prevents IWDT crashes). Only controlTask calls runtime motor adapter functions after ownership binding. UI/input tasks submit requests; the raw stepper pointer is not exposed.
- **Safety stepper stop:** ENA is disabled before cleanup. Physical E-STOP/driver-alarm handling does not call the stepper library. controlTask alone performs bounded `forceStop()` cleanup and retries until `isRunning()` confirms that queued pulses drained. Reset remains blocked during cleanup; ENA stays inhibited. The E-STOP transition publishes its state without waiting for blocking motor cleanup.
- **Atomic variables:** Cross-core shared state uses `std::atomic` with explicit memory ordering. All such flags are declared in `src/app_state.h` and defined in `src/app_state.cpp` — no scattered declarations.
- **Pending-flag pattern:** UI callbacks `.store()` atomic flags with `memory_order_release`; Core 0 tasks `.load()` with `memory_order_acquire` and execute within their cycle. No direct motor calls from UI thread.
- **Storage mutex:** `g_presets_mutex` semaphore protects preset vector access.
- **Flash writes only at rest:** the prebuilt framework has no IRAM-safe RMT/GPIO ISRs, so settings and programs are saved only in IDLE or ESTOP. A start waits for a save in progress.
- **LVGL safety:** `lv_obj_delete_async()` for all keyboard/numpad deletion from event callbacks. `screen_*_invalidate_widgets()` nullifies static pointers on `screens_reinit()`.

## 4a. Fatal-error handling
- Unrecoverable failures call `fatal_halt("<context>")` (declared in `src/app_state.h`). It sets `g_restartRequired`, disables ENA, logs the reason, unsubscribes its caller from the task watchdog and remains halted. The inhibit prevents other live tasks from re-enabling motion. Repair and restart are required; this function does not reboot.

Storage formatting uses a separate temporary motion/reset inhibit. It releases only that inhibit on failure; fatal and restart latches remain set. A successful format asserts the permanent restart latch before releasing its temporary inhibit.

## 5. Hardware Watchdog (TWDT)
- **Input Task:** Subscribed to WDT. Refreshes ADC and pedal inputs; step generation belongs to the driver library.
- **Control Task:** Subscribed to WDT. Ensures state machine is responsive.
- **Safety Task:** Subscribed to WDT. Ensures E-STOP logic is alive.
- **Storage Task:** NOT subscribed (does blocking I/O — flash writes and related work — that can exceed WDT timeout).
- **LVGL Task:** NOT subscribed (graphics rendering must not be interrupted).

The LVGL and touch paths are supervised by `safetyTask` instead; see section 7.

If a subscribed task stops feeding the watchdog, the configured watchdog action resets the controller. Driver-disable behavior during reset depends on the physical ENA bias and wiring and requires bench verification. Watchdog initialization, registration and feed failures are checked; motion remains blocked until all critical tasks report ready.

## 6. EMI Protection
For detailed EMI mitigation (Shielding, Ferrites, TVS Diodes), see the [EMI Mitigation Guide](EMI_MITIGATION.md).

## Guided commissioning and stale control

The [Setup Wizard](SETUP_WIZARD.md) requires observations of the physical E-STOP input, release/reset and a separate start/stop sequence plus user confirmation. This records an operator check, not a physical stop-time measurement. Pedal starts are inhibited during setup.

Coherent control snapshots feed 40 ms UI updates. Status older than 100 ms blocks new movement requests and shows a warning. Normal STOP remains unconditional; safetyTask checks its acknowledgement/completion deadlines independently of controlTask. Physical E-STOP ENA inhibition remains independent. See [implementation boundaries and device checks](CONTROL_SETUP_IMPLEMENTATION.md).

The scheduled E-STOP level poll inhibits ENA on the first observed LOW, then uses the 5 ms classification path even if no interrupt arrived. A driver alarm also inhibits ENA and latches a fault on its first LOW sample, including short glitches. A healthy driver input clears the alarm input latch after 50 healthy polling samples, but the system fault still requires explicit reset. Reset requires healthy raw E-STOP/ALM, no pending E-STOP edge, completed motor cleanup, fresh task heartbeats, operational touch when required, and no required restart. Failed configuration rollback or acceleration restoration keeps control cleanup incomplete and is retried.

## 7. Runtime supervision

`safetyTask` writes ENA HIGH directly for stale tasks, because a stop request would wait on the task that hung. The first latched reason is kept until reset. Every latched fault disarms the USB mirror.

Freshness checks compare a timestamp written by one task with the clock read by another. A task can stamp just after the checker read the clock, so the stamp is a millisecond ahead. `timestamp_within()` treats a stamp up to 5 ms ahead as fresh. Without that, unsigned subtraction read it as 49 days old and a live task could be latched as stale. The input and UI heartbeats, the ADS1115 pedal sample and the Jog lease use it.

Boot halts with ENA inhibited when the LVGL tick timer cannot start, because the UI heartbeat would keep running without touch input.

| Condition | Response |
| --- | --- |
| Control cycle or input heartbeat stale for 100 ms | Latches **CONTROL STALE** or **INPUT STALE** |
| LVGL loop stalled for 2 s, or 100 failed touch reads in a row | Stops active motion and latches **HMI STALE**. At rest, new starts are blocked instead |
| Speed pot jumps to the rail within one 20 ms sample | Holds the speed, then latches **SPEED POT** after three samples. Reset stays blocked until the pot reads plausibly. Turning the knob to zero by hand is unaffected |
| Restart after a panic, watchdog or brownout | Latches **UNEXPECTED RESET** at boot. The event log survives these resets, but not power loss |
| Invalid saved data at boot | Latches **STORAGE** with motion disabled and the data untouched. Only this fault offers **Hold 3 s to erase data** on the overlay |

> [!CAUTION]
> A floating pot wiper is not detected and the analog pedal is not jump-checked. An open E-STOP wire reads as healthy. E-STOP acts through the MCU and the driver ENA input; it does not disconnect motor power.

See [All fault names](../wiki/Troubleshooting.md#fault-names).
