// Microstep - Microstepping mode selection and validation
#include "microstep.h"
#include "../config.h"
#include "../storage/storage.h"

void microstep_init() {
  uint8_t val = g_settings.microstep;
  if (val != MICROSTEP_4 && val != MICROSTEP_8 && val != MICROSTEP_16 && val != MICROSTEP_32) {
    g_settings.microstep = 16;
  }
  LOG_I("Microstep: %s", microstep_get_string());
}

MicrostepSetting microstep_get() {
  return (MicrostepSetting)g_settings.microstep;
}

void microstep_set(MicrostepSetting setting) {
  if (setting == MICROSTEP_4 || setting == MICROSTEP_8 || setting == MICROSTEP_16 || setting == MICROSTEP_32) {
    g_settings.microstep = (int)setting;
    LOG_I("Microstep set to: %s", microstep_get_string());
  }
}

const char* microstep_get_string() {
  switch (g_settings.microstep) {
    case MICROSTEP_4:  return "1/4";
    case MICROSTEP_8:  return "1/8";
    case MICROSTEP_16: return "1/16";
    case MICROSTEP_32: return "1/32";
    default: return "1/16";
  }
}

uint32_t microstep_get_steps_per_rev() {
  return 200 * (uint32_t)g_settings.microstep;
}

void microstep_save() {
  storage_save_settings();
  LOG_I("Microstep saved: %s", microstep_get_string());
}
