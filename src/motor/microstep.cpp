// TIG Rotator Controller - Microstep Settings Implementation
// EEPROM storage for microstep configuration

#include "microstep.h"
#include "../config.h"
#include <EEPROM.h>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static MicrostepSetting currentMicrostep = MICROSTEP_8;

// EEPROM addresses (offset from calibration which uses 0-2)
#define EEPROM_MICROSTEP_ADDR   10  // Separate from calibration (0-2)

// ───────────────────────────────────────────────────────────────────────────────
// INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void microstep_init() {
  uint8_t value;
  EEPROM.get(EEPROM_MICROSTEP_ADDR, value);

  // Validate loaded value
  if (value == 8 || value == 16 || value == 32) {
    currentMicrostep = (MicrostepSetting)value;
    LOG_I("Microstep loaded: %s", microstep_get_string());
  } else {
    currentMicrostep = MICROSTEP_8;  // Default
    LOG_I("Microstep: default (1/8)");
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// GET/SET
// ───────────────────────────────────────────────────────────────────────────────
MicrostepSetting microstep_get() {
  return currentMicrostep;
}

void microstep_set(MicrostepSetting setting) {
  if (setting == MICROSTEP_8 || setting == MICROSTEP_16 || setting == MICROSTEP_32) {
    currentMicrostep = setting;
    LOG_I("Microstep set to: %s", microstep_get_string());
  }
}

const char* microstep_get_string() {
  switch (currentMicrostep) {
    case MICROSTEP_8:  return "1/8";
    case MICROSTEP_16: return "1/16";
    case MICROSTEP_32: return "1/32";
    default: return "1/8";
  }
}

uint32_t microstep_get_steps_per_rev() {
  // 200 steps per revolution * microstep setting
  return 200 * (uint32_t)currentMicrostep;
}

// ───────────────────────────────────────────────────────────────────────────────
// EEPROM STORAGE
// ───────────────────────────────────────────────────────────────────────────────
void microstep_save() {
  EEPROM.put(EEPROM_MICROSTEP_ADDR, (uint8_t)currentMicrostep);
  EEPROM.commit();
  LOG_I("Microstep saved: %s", microstep_get_string());
}
