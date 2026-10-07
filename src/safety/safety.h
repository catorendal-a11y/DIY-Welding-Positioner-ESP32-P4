// TIG Rotator Controller - Safety System
// ESTOP interrupt handler, watchdog, fail-safe boot verification

#pragma once
#include <Arduino.h>
#include <hal/wdt_hal.h>
#include "../app_state.h"  // g_estopPending, g_estopTriggerMs, g_uiResetPending

#include "fault_reason.h"

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void safety_task_ready(uint32_t bit);
void safety_register_watchdog();
void safety_report_input_fault();
void safety_report_motor_fault(FaultReason reason);
void safety_init();           // Initialize safety system
void safety_cache_stepper();  // Compatibility no-op; safety never calls the stepper library
void safety_attach_estop();   // Attach ESTOP interrupt (call after motor_init)

// ESTOP status
bool safety_is_estop_active();          // Returns true if ESTOP is pressed
bool safety_is_driver_alarm_latched();  // DM542T ALM held low (debounced) — blocks motion / reset
bool safety_inhibit_motion();           // ESTOP pin OR driver alarm (use before enabling motor)
bool safety_can_reset_from_overlay();   // ESTOP released and driver alarm clear (for RESET UI)
bool safety_is_estop_locked();          // Returns true if in ESTOP state
FaultReason safety_get_fault_reason();  // Last latched fault source for diagnostics/UI
const char* safety_fault_reason_name(FaultReason reason);
const char* safety_fault_reason_message(FaultReason reason);
void safety_reset_estop();     // Reset ESTOP (only when physical button released)
bool safety_check_ui_reset();  // Check if UI requested reset, process if safe

// Watchdog
void safety_init_watchdog();  // Initialize hardware watchdog (5s timeout)
void safety_feed_watchdog();  // Feed watchdog (call from tasks periodically)

// FreeRTOS task
void safetyTask(void* pvParameters);  // ESTOP monitoring task (Core 0, priority 5)
void safety_run_cycle(); // One scheduled supervisor cycle; also used by host tests.
