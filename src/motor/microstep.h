// TIG Rotator Controller - Microstep Settings
// Allow user to select 1/8, 1/16, or 1/32 microstepping
// TB6600 DIP switches must match the selection!

#pragma once
#include <Arduino.h>

// Microstep options
typedef enum {
  MICROSTEP_8  = 8,
  MICROSTEP_16 = 16,
  MICROSTEP_32 = 32
} MicrostepSetting;

// Microstep API
void microstep_init();                          // Load from EEPROM
MicrostepSetting microstep_get();               // Get current setting
void microstep_set(MicrostepSetting setting);   // Set and save
const char* microstep_get_string();             // Get "1/8", "1/16", or "1/32"
uint32_t microstep_get_steps_per_rev();         // Get calculated steps per rev (200 * microstep)
void microstep_save();                          // Save to EEPROM
