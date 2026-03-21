// TIG Rotator Controller - Safety System
// ESTOP interrupt handler, watchdog, fail-safe boot verification

#pragma once
#include <Arduino.h>
#include <hal/wdt_hal.h>

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS (shared with ISR)
// ───────────────────────────────────────────────────────────────────────────────
extern volatile bool g_estopPending;      // Set by ISR, cleared by safetyTask
extern volatile int64_t g_estopTriggerUs; // Timestamp of ESTOP event (microseconds, ISR-safe)

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void safety_init();                    // Initialize safety system
void safety_cache_stepper();           // Cache stepper pointer for ISR (call after motor_init)
void safety_attach_estop();            // Attach ESTOP interrupt (call after motor_init)

// ESTOP status
bool safety_is_estop_active();         // Returns true if ESTOP is pressed
bool safety_is_estop_locked();         // Returns true if in ESTOP state
void safety_reset_estop();             // Reset ESTOP (only when physical button released)

// Watchdog
void safety_init_watchdog();           // Initialize hardware watchdog (2s timeout)
void safety_feed_watchdog();           // Feed watchdog (call from tasks periodically)
