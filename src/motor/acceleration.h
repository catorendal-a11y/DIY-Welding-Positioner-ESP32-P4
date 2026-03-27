// TIG Rotator Controller - Acceleration Control
// Motor acceleration/deceleration rate

#pragma once
#include <Arduino.h>

void acceleration_init();                  // Initialize (load from EEPROM)
void acceleration_set(uint32_t accel);     // Set acceleration (steps/s²)
uint32_t acceleration_get();               // Get current acceleration
void acceleration_save();                  // Save to EEPROM
