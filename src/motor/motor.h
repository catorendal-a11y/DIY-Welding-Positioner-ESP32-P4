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
void motor_gpio_init();   // Configure GPIO pins — ENA must be HIGH before calling
void motor_init();        // Initialize FastAccelStepper

// Motor control functions (must be called from Core 0 only)
bool motor_run_cw();      // Run clockwise; false if safety or hardware state blocks start
bool motor_run_ccw();     // Run counter-clockwise; false if safety or hardware state blocks start
void motor_stop();        // Smooth deceleration to stop
void motor_halt();        // Immediate stop (emergency)
void motor_disable();     // Disable motor (ENA HIGH) after stopped

// Status queries
bool motor_is_running();
uint32_t motor_get_current_hz();       // Rounded Hz from UI cache (see motor_refresh_hz_cache)
// Step frequency magnitude (Hz), sub-Hz; lock-free read of cache (~5ms fresh, motorTask updates).
float motor_get_step_frequency_hz();
// Core 0 / motorTask: sample stepper under mutex and publish for UI (lvglTask) without cross-core mutex wait.
void motor_refresh_hz_cache(void);
// Calibrated workpiece RPM -> step rate (MilliHz), floored to START_SPEED so all paths match.
uint32_t motor_milli_hz_for_rpm_calibrated(float rpm_workpiece_command);
// Caller must hold g_stepperMutex.
void motor_apply_speed_for_rpm_locked(float rpm_workpiece_command);
// Apply a new target step rate (milliHz) with acceleration. Handles stepper mutex internally.
// Safe to call from Core 0 tasks (motorTask). No-op if stepper not yet initialized.
void motor_set_target_milli_hz(uint32_t mhz);
FastAccelStepper* motor_get_stepper();  // Get stepper instance (caller must hold g_stepperMutex)
void motor_apply_settings();  // Apply acceleration from g_settings
void motor_apply_soft_start_acceleration();
void motor_restore_configured_acceleration();

// FreeRTOS task
void motorTask(void* pvParameters);    // Motor speed update task (Core 0, priority 4)
