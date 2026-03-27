// TIG Rotator Controller - Motor Calibration
// Allow user to calibrate actual rotation vs theoretical

#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────────────────────────────────────
// CALIBRATION API
// ───────────────────────────────────────────────────────────────────────────────
void calibration_init();                    // Initialize calibration system
void calibration_set_factor(float factor); // Set calibration factor (1.0 = no correction)
float calibration_get_factor();             // Get current calibration factor

// Apply calibration to step count or angle
long calibration_apply_steps(long steps);       // Adjust steps for actual rotation
float calibration_apply_angle(float angle);    // Adjust angle for input steps

// EEPROM save/load
void calibration_save();                      // Save to EEPROM
bool calibration_load();                      // Load from EEPROM
