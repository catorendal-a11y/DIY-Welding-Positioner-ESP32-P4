// Calibration - Workpiece calibration factor management
#include "calibration.h"
#include "../config.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <cmath>
#include "../control/motion_policy.h"

static std::atomic<float> draftFactor{0};
static std::atomic<bool> discardPending{false};
void calibration_discard_draft() { discardPending.store(true); }
void calibration_process_pending() {
  if (discardPending.exchange(false)) draftFactor.store(0);
}

void calibration_init() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  if (!motion_calibration_factor_valid(g_settings.calibration_factor)) {
    g_settings.calibration_factor = 1.0f;
  }
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Calibration: factor=%.3f", f);
}

void calibration_set_factor(float factor) {
  if (!motion_calibration_factor_valid(factor)) return;
  discardPending.store(false);
  draftFactor.store(factor);
}

float calibration_get_saved_factor() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  return f;
}

float calibration_get_factor() {
  const float draft = draftFactor.load();
  return draft > 0 ? draft : calibration_get_saved_factor();
}

long calibration_apply_steps(long steps) {
  int32_t checked = 0;
  motion_checked_steps(double(steps) * calibration_get_factor(), checked);
  return checked;
}

uint32_t calibration_save() {
  const float verifiedFactor = calibration_get_factor();
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.calibration_factor = verifiedFactor;
  [[maybe_unused]] float f = g_settings.calibration_factor;
  xSemaphoreGive(g_settings_mutex);
  const uint32_t ticket = storage_request_settings_save();
  LOG_I("Calibration save queued: factor=%.3f", f);
  return ticket;
}

