// Microstep - Microstepping mode selection and validation
#include "microstep.h"
#include "../config.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../control/motion_policy.h"

void microstep_init() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  uint8_t val = g_settings.microstep;
  g_settings.microstep = motion_microstep_or_default(val);
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
  return motion_microstep_name(ms);
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
