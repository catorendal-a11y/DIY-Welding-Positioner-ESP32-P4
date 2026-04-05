// TIG Rotator Controller - Motor Control Interface
// ESP32-P4: TB6600 stepper driver via FastAccelStepper

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
void motor_run_cw();      // Run clockwise
void motor_run_ccw();     // Run counter-clockwise
void motor_stop();        // Smooth deceleration to stop
void motor_halt();        // Immediate stop (emergency)
void motor_disable();     // Disable motor (ENA HIGH) after stopped

// Status queries
bool motor_is_running();
uint32_t motor_get_current_hz();  // Current step frequency
FastAccelStepper* motor_get_stepper();  // Get stepper instance (caller must hold g_stepperMutex)
void motor_apply_settings();  // Apply acceleration from g_settings

// FreeRTOS task
void motorTask(void* pvParameters);    // Motor speed update task (Core 0, priority 4)
