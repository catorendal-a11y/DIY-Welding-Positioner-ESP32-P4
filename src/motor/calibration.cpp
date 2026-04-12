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
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration: factor=%.3f", f);
}

void calibration_set_factor(float factor) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.calibration_factor = constrain(factor, 0.5f, 1.5f);
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration factor set to %.3f", f);
}

float calibration_get_factor() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  return f;
}

long calibration_apply_steps(long steps) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  return (long)(steps * f);
}

float calibration_apply_angle(float angle) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  if (f < 1e-6f) return angle;
  return angle / f;
}

void calibration_save() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
  LOG_I("Calibration saved: factor=%.3f", f);
}

bool calibration_validate() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  if (g_settings.calibration_factor < 0.5f || g_settings.calibration_factor > 1.5f) {
    g_settings.calibration_factor = 1.0f;
    xSemaphoreGive(g_settings_mutex);
    return false;
  }
  float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration loaded: factor=%.3f", f);
  return true;
}
