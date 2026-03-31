// TIG Rotator Controller - Motor Control Interface
// ESP32-P4: TB6600 stepper driver via FastAccelStepper

#pragma once
#include <Arduino.h>
#include <FastAccelStepper.h>

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void motor_gpio_init();   // Configure GPIO pins — ENA must be HIGH before calling
void motor_init();        // Initialize FastAccelStepper

// Motor control functions
void motor_run_cw();      // Run clockwise
void motor_run_ccw();     // Run counter-clockwise
void motor_stop();        // Smooth deceleration to stop
void motor_halt();        // Immediate stop (emergency)

// Status queries
bool motor_is_running();
uint32_t motor_get_current_hz();  // Current step frequency
FastAccelStepper* motor_get_stepper();  // Get stepper instance (for mode files)
