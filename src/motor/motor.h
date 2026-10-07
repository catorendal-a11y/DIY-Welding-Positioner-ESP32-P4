// TIG Rotator Controller - Motor Control Interface
// ESP32-P4: stepper driver via FastAccelStepper (timing from g_settings.stepper_driver)

#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ───────────────────────────────────────────────────────────────────────────────
// STEPPER MUTEX — all stepper calls must hold this
// FreeRTOS mutex (not spinlock) so tick interrupts stay enabled on both cores
// ───────────────────────────────────────────────────────────────────────────────
extern SemaphoreHandle_t g_stepperMutex;

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void motor_gpio_init();  // Configure GPIO pins — ENA must be HIGH before calling
void motor_init();       // Initialize FastAccelStepper

// Motor control functions (must be called from Core 0 only)
bool motor_run_cw();   // Run clockwise; false if safety or hardware state blocks start
bool motor_run_ccw();  // Run counter-clockwise; false if safety or hardware state blocks start
void motor_stop();     // Smooth deceleration to stop
bool motor_halt();     // ENA inhibit immediately; bounded library cleanup, retry if false
void motor_bind_owner(); // Called once by controlTask; initialization precedes binding.
bool motor_lock();     // Bounded stepper access; failure latches a motor timeout
bool motor_cleanup_pending();
void motor_command_failed(); // Caller may hold the stepper mutex
uint32_t motor_stop_timeout_ms();
bool motor_move_timeout_ms(uint32_t pulses, float rpm, uint32_t& budget);
void motor_disable();  // Disable motor (ENA HIGH) after stopped

// Driver-family ENA settle (DM542T/Leadshine-class t1): before the first PUL
// after an enable, ENA must be asserted for the settle window. controlTask
// drives this via STATE_ENABLING; these run on the control core only.
bool motor_prepare_start();      // Final inhibit re-check, then assert ENA LOW and stamp the clock
bool motor_ena_settle_pending(); // True until the asserted enable has waited out its settle window

// Status queries
bool motor_is_running();
bool motor_direction_is_cw();
void motor_record_direction(bool cw);  // Called only when issuing a motion command.
uint32_t motor_get_current_hz();       // Rounded Hz from UI cache (see motor_refresh_hz_cache)
// Step frequency magnitude (Hz), sub-Hz; lock-free read of cache (~5ms fresh, controlTask updates).
float motor_get_step_frequency_hz();
// controlTask: sample stepper under mutex and publish for UI (lvglTask) without cross-core mutex wait.
void motor_refresh_hz_cache(void);
// Calibrated workpiece RPM -> milliHz. Zero rejects out-of-range rates; never silently raises RPM.
uint32_t motor_milli_hz_for_rpm_calibrated(float rpm_workpiece_command);
// Caller must hold g_stepperMutex.
bool motor_apply_speed_for_rpm_locked(float rpm_workpiece_command);
// Apply a new target step rate (milliHz) with acceleration. Handles stepper mutex internally.
// Safe to call from controlTask. No-op if stepper not yet initialized.
void motor_set_target_milli_hz(uint32_t mhz);
bool motor_move_steps(long steps, float rpm, int32_t* start_position);
bool motor_read_position(int32_t* position);
bool motor_apply_settings();            // Apply driver timing/acceleration; false on hardware failure
void motor_apply_soft_start_acceleration();
bool motor_restore_configured_acceleration(); // Retry cleanup until restoration succeeds.

// Input sampling lives in main.cpp; only controlTask calls this motor adapter.

struct MotorDriverInfo {
  const char* name = "UNAVAILABLE";
  uint32_t direction_before_us = 0, direction_after_us = 0;
};
bool motor_read_driver_info(MotorDriverInfo& out); // Cached at configuration, no library call from UI
