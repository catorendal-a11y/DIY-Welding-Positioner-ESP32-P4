// TIG Rotator Controller - Acceleration Control Implementation
// EEPROM storage for motor acceleration

#include "acceleration.h"
#include "../config.h"
#include <EEPROM.h>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t accelerationValue = 5000;  // Default steps/s²

// EEPROM addresses (offset from others)
#define EEPROM_ACCEL_ADDR   20  // Separate from calibration (0-2), microstep (10), brightness (15)

// Valid acceleration range
#define ACCEL_MIN  1000   // Smooth start
#define ACCEL_MAX  20000  // Fast start

// ───────────────────────────────────────────────────────────────────────────────
// INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void acceleration_init() {
  uint32_t value;
  EEPROM.get(EEPROM_ACCEL_ADDR, value);

  // Validate loaded value
  if (value >= ACCEL_MIN && value <= ACCEL_MAX) {
    accelerationValue = value;
    LOG_I("Acceleration loaded: %u", accelerationValue);
  } else {
    accelerationValue = 5000;  // Default
    LOG_I("Acceleration: default (%u)", accelerationValue);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// GET/SET
// ───────────────────────────────────────────────────────────────────────────────
void acceleration_set(uint32_t accel) {
  accelerationValue = constrain(accel, ACCEL_MIN, ACCEL_MAX);
  LOG_I("Acceleration set to: %u", accelerationValue);
}

uint32_t acceleration_get() {
  return accelerationValue;
}

// ───────────────────────────────────────────────────────────────────────────────
// EEPROM STORAGE
// ───────────────────────────────────────────────────────────────────────────────
void acceleration_save() {
  EEPROM.put(EEPROM_ACCEL_ADDR, accelerationValue);
  EEPROM.commit();
  LOG_I("Acceleration saved: %u", accelerationValue);
}
