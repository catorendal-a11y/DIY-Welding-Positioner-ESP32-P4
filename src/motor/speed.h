// TIG Rotator Controller - Speed Control
// Converts workpiece RPM to motor step frequency with ADC filtering

#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
// RPM on workpiece → motor step frequency
float rpmToStepHz(float rpm_workpiece);

// Angle in degrees (on workpiece) → motor steps
long angleToSteps(float degrees);

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONTROL
// ───────────────────────────────────────────────────────────────────────────────
void speed_init();                      // Initialize speed control
void speed_update_adc();                // Read and filter potentiometer (call every 20ms)
void speed_slider_set(float rpm);       // Set speed from GUI slider
float speed_get_target_rpm();           // Get current target RPM (pot or slider)
float speed_get_actual_rpm();           // Get actual measured RPM from motor
bool speed_using_slider();              // Returns true if slider was used recently
void speed_apply();                     // Apply speed to motor (call every 5ms)

// Direction control
typedef enum { DIR_CW = 0, DIR_CCW = 1 } Direction;
Direction speed_get_direction();
void speed_set_direction(Direction dir);
