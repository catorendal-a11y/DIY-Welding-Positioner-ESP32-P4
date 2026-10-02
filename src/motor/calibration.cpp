// Calibration - Workpiece calibration factor management
#include "calibration.h"
#include "../config.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void calibration_init() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  if (g_settings.calibration_factor < 0.5f || g_settings.calibration_factor > 1.5f) {
    g_settings.calibration_factor = 1.0f;
  }
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration: factor=%.3f", f);
}

void calibration_set_factor(float factor) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.calibration_factor = constrain(factor, 0.5f, 1.5f);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration factor set to %.3f", f);
}

float calibration_get_factor() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  return f;
}

long calibration_apply_steps(long steps) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  return (long)(steps * f);
}

float calibration_apply_angle(float angle) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  if (f < 1e-6f) return angle;
  return angle / f;
}

uint32_t calibration_save() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  const uint32_t ticket = storage_request_settings_save();
  LOG_I("Calibration save queued: factor=%.3f", f);
  return ticket;
}

bool calibration_validate() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  if (g_settings.calibration_factor < 0.5f || g_settings.calibration_factor > 1.5f) {
    g_settings.calibration_factor = 1.0f;
    xSemaphoreGive(g_settings_mutex);
    return false;
  }
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration loaded: factor=%.3f", f);
  return true;
}
