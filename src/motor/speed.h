// TIG Rotator Controller - Speed Control
// Converts workpiece RPM to motor step frequency with ADC filtering

#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
// RPM on workpiece -> motor step Hz (geometry only, no calibration)
float rpmToStepHz(float rpm_workpiece);
// UI / pot RPM command -> step Hz (applies calibration so actual RPM display matches command)
float rpmToStepHzCalibrated(float rpm_command);

// Angle in degrees (on workpiece) → motor steps
long angleToSteps(float degrees);

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

// Pedal control (Wiring v2: ADS1115 AIN0 = pedal pot, GPIO33 = pedal SW; requires ADS1115 when enabled)
void speed_set_pedal_enabled(bool enabled);
bool speed_get_pedal_enabled();
// True when pedal mode is armed: ADS1115 detected and user enabled PEDAL (GPIO33 start/stop active).
bool speed_pedal_connected();
// True when ADS1115 responded on touch I2C (build must have ENABLE_ADS1115_PEDAL).
bool speed_ads1115_pedal_present(void);
