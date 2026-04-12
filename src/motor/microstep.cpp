// Microstep - Microstepping mode selection and validation
#include "microstep.h"
#include "../config.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void microstep_init() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  uint8_t val = g_settings.microstep;
  if (val != MICROSTEP_4 && val != MICROSTEP_8 && val != MICROSTEP_16 && val != MICROSTEP_32) {
    g_settings.microstep = 16;
  }
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Microstep: %s", microstep_get_string());
}

MicrostepSetting microstep_get() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  MicrostepSetting m = (MicrostepSetting)g_settings.microstep;
  xSemaphoreGive(g_settings_mutex);
  return m;
}

void microstep_set(MicrostepSetting setting) {
  if (setting == MICROSTEP_4 || setting == MICROSTEP_8 || setting == MICROSTEP_16 || setting == MICROSTEP_32) {
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    g_settings.microstep = (int)setting;
    xSemaphoreGive(g_settings_mutex);
    LOG_I("Microstep set to: %s", microstep_get_string());
  }
}

const char* microstep_get_string() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  uint8_t ms = g_settings.microstep;
  xSemaphoreGive(g_settings_mutex);
  switch (ms) {
    case MICROSTEP_4:  return "1/4";
    case MICROSTEP_8:  return "1/8";
    case MICROSTEP_16: return "1/16";
    case MICROSTEP_32: return "1/32";
    default: return "1/16";
  }
}

uint32_t microstep_get_steps_per_rev() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  uint32_t ms = (uint32_t)g_settings.microstep;
  xSemaphoreGive(g_settings_mutex);
  return 200 * ms;
}

void microstep_save() {
  storage_save_settings();
  LOG_I("Microstep saved: %s", microstep_get_string());
}
