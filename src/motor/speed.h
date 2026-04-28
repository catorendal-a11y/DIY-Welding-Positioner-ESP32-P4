// TIG Rotator Controller - Speed Control
// Converts workpiece RPM to motor step frequency with ADC filtering

#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
// Motor steps for one 360° turn of gearbox output (72T shaft), before (D_emne/D_rulle). Uses current microstep.
float speed_steps_per_gear_output_rev(void);

// RPM on workpiece -> motor step Hz (geometry only, no calibration)
float rpmToStepHz(float rpm_workpiece);
// UI / pot RPM command -> step Hz (applies calibration so actual RPM display matches command)
float rpmToStepHzCalibrated(float rpm_command);

// Angle in degrees (on workpiece) → motor steps
long angleToSteps(float degrees);
long angleToStepsForDiameter(float degrees, float mm_od);

// STEP / job: effective workpiece OD in mm for kinematics (0 = use D_EMNE from config.h)
void speed_set_workpiece_diameter_mm(float mm_od);
float speed_get_workpiece_diameter_mm(void);

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONTROL
// ───────────────────────────────────────────────────────────────────────────────
void speed_init();                      // Initialize speed control
void speed_update_adc();                // Read and filter potentiometer (call every 20ms)
// Reload UI max RPM from g_settings (after NVS load or Motor Config save). Core 0 / motorTask safe.
void speed_sync_rpm_limits_from_settings();
float speed_get_rpm_max();              // Current UI/pot ceiling (atomic, MIN_RPM..MAX_RPM)
void speed_slider_set(float rpm);       // Set speed from GUI slider
// When true (Step screen), cached target RPM follows UI slider only (no pot takeover).
void speed_set_slider_priority(bool on);
float speed_get_target_rpm();           // Get current target RPM (pot or slider)
float speed_get_actual_rpm();           // Get actual measured RPM from motor
bool speed_using_slider();              // Returns true if slider was used recently
void speed_apply();                     // Apply speed to motor (call every 5ms)

// Direction control
typedef enum { DIR_CW = 0, DIR_CCW = 1 } Direction;
Direction speed_get_direction();
void speed_set_direction(Direction dir);
void speed_set_program_direction_override(Direction dir);
void speed_clear_program_direction_override();
bool speed_program_direction_override_active();

// Pedal control (GPIO33 switch works without ADS1115; ADS1115 only adds analog pedal speed)
void speed_set_pedal_enabled(bool enabled);
bool speed_get_pedal_enabled();
bool speed_pedal_switch_enabled();
bool speed_pedal_analog_available();
// Compatibility alias: true when GPIO33 pedal start/stop is armed.
bool speed_pedal_connected();
// True when ADS1115 responded on touch I2C (build must have ENABLE_ADS1115_PEDAL).
bool speed_ads1115_pedal_present(void);
