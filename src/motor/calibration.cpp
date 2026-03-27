// TIG Rotator Controller - Motor Calibration Implementation
// EEPROM storage of calibration factor for accurate rotation

#include "calibration.h"
#include "../config.h"
#include <EEPROM.h>

// ───────────────────────────────────────────────────────────────────────────────
// CALIBRATION FACTOR
// Default 1.0 = no correction (theoretical = actual)
// > 1.0 = motor runs MORE than expected (gear ratio different, etc.)
// < 1.0 = motor runs LESS than expected
// ───────────────────────────────────────────────────────────────────────────────
static float calibrationFactor = 1.0f;

// EEPROM addresses
#define EEPROM_CALIBRATION_ADDR  0
#define EEPROM_MAGIC_NUMBER      0xA5  // To detect if EEPROM has valid data

// ───────────────────────────────────────────────────────────────────────────────
// INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void calibration_init() {
  // Try to load from EEPROM
  if (!calibration_load()) {
    // No valid data found, use default
    calibrationFactor = 1.0f;
    LOG_I("Calibration: default (factor=1.0)");
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SET/GET CALIBRATION FACTOR
// ───────────────────────────────────────────────────────────────────────────────
void calibration_set_factor(float factor) {
  // Constrain factor to reasonable range (50% to 150%)
  calibrationFactor = constrain(factor, 0.5f, 1.5f);
  LOG_I("Calibration factor set to %.3f", calibrationFactor);
}

float calibration_get_factor() {
  return calibrationFactor;
}

// ───────────────────────────────────────────────────────────────────────────────
// APPLY CALIBRATION
// ───────────────────────────────────────────────────────────────────────────────
long calibration_apply_steps(long steps) {
  // Adjust steps by calibration factor
  // If factor is 1.05, we need 5% more steps for the same rotation
  return (long)(steps * calibrationFactor);
}

float calibration_apply_angle(float angle) {
  // Adjust angle input - what angle do we need to request
  // to get the actual desired angle?
  return angle / calibrationFactor;
}

// ───────────────────────────────────────────────────────────────────────────────
// EEPROM STORAGE
// ───────────────────────────────────────────────────────────────────────────────
void calibration_save() {
  EEPROM.put(EEPROM_CALIBRATION_ADDR, (uint8_t)EEPROM_MAGIC_NUMBER);
  EEPROM.put(EEPROM_CALIBRATION_ADDR + 1, calibrationFactor);
  EEPROM.commit();
  LOG_I("Calibration saved: factor=%.3f", calibrationFactor);
}

bool calibration_load() {
  uint8_t magic;
  EEPROM.get(EEPROM_CALIBRATION_ADDR, magic);

  if (magic != EEPROM_MAGIC_NUMBER) {
    return false;  // No valid data
  }

  EEPROM.get(EEPROM_CALIBRATION_ADDR + 1, calibrationFactor);

  // Validate loaded factor
  if (calibrationFactor < 0.5f || calibrationFactor > 1.5f) {
    LOG_W("Calibration factor out of range, using default");
    calibrationFactor = 1.0f;
    return false;
  }

  LOG_I("Calibration loaded: factor=%.3f", calibrationFactor);
  return true;
}
