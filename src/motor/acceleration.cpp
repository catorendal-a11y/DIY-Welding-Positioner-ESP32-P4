// Acceleration - Motor acceleration/deceleration control
#include "acceleration.h"
#include "../config.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include "../control/motion_policy.h"

void acceleration_init() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.acceleration = motion_acceleration_or_default(g_settings.acceleration);
  [[maybe_unused]] uint32_t a = g_settings.acceleration;
  xSemaphoreGive(g_settings_mutex);
  LOG_I("Acceleration: %u", (unsigned)a);
}

static std::atomic<bool> accelApplyPending{false};

void acceleration_set(uint32_t accel) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.acceleration = motion_acceleration_clamp(accel);
  [[maybe_unused]] uint32_t a = g_settings.acceleration;
  xSemaphoreGive(g_settings_mutex);
  accelApplyPending.store(true, std::memory_order_release);
  LOG_I("Acceleration set to: %u", (unsigned)a);
}

bool acceleration_has_pending_apply() {
  return accelApplyPending.load(std::memory_order_acquire);
}

void acceleration_clear_pending() {
  accelApplyPending.store(false, std::memory_order_release);
}

uint32_t acceleration_get() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] uint32_t a = g_settings.acceleration;
  xSemaphoreGive(g_settings_mutex);
  return a;
}

void acceleration_save() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  [[maybe_unused]] uint32_t a = g_settings.acceleration;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
  LOG_I("Acceleration saved: %u", (unsigned)a);
}
