// Calibration - Workpiece calibration factor management
#include "calibration.h"
#include "../config.h"
#include "../storage/storage.h"

void calibration_init() {
  if (g_settings.calibration_factor < 0.5f || g_settings.calibration_factor > 1.5f) {
    g_settings.calibration_factor = 1.0f;
  }
  LOG_I("Calibration: factor=%.3f", g_settings.calibration_factor);
}

void calibration_set_factor(float factor) {
  g_settings.calibration_factor = constrain(factor, 0.5f, 1.5f);
  LOG_I("Calibration factor set to %.3f", g_settings.calibration_factor);
}

float calibration_get_factor() {
  return g_settings.calibration_factor;
}

long calibration_apply_steps(long steps) {
  return (long)(steps * g_settings.calibration_factor);
}

float calibration_apply_angle(float angle) {
  return angle / g_settings.calibration_factor;
}

void calibration_save() {
  storage_save_settings();
  LOG_I("Calibration saved: factor=%.3f", g_settings.calibration_factor);
}

bool calibration_validate() {
  if (g_settings.calibration_factor < 0.5f || g_settings.calibration_factor > 1.5f) {
    g_settings.calibration_factor = 1.0f;
    return false;
  }
  LOG_I("Calibration loaded: factor=%.3f", g_settings.calibration_factor);
  return true;
}
