// Acceleration - Motor acceleration/deceleration control
#include "acceleration.h"
#include "../config.h"
#include "../storage/storage.h"
#include <atomic>

#define ACCEL_MIN  1000
#define ACCEL_MAX  30000

void acceleration_init() {
  if (g_settings.acceleration < ACCEL_MIN || g_settings.acceleration > ACCEL_MAX) {
    g_settings.acceleration = 7500;
  }
  LOG_I("Acceleration: %u", g_settings.acceleration);
}

static std::atomic<bool> accelApplyPending{false};

void acceleration_set(uint32_t accel) {
  g_settings.acceleration = constrain(accel, ACCEL_MIN, ACCEL_MAX);
  accelApplyPending.store(true, std::memory_order_release);
  LOG_I("Acceleration set to: %u", g_settings.acceleration);
}

bool acceleration_has_pending_apply() {
  return accelApplyPending.load(std::memory_order_acquire);
}

void acceleration_clear_pending() {
  accelApplyPending.store(false, std::memory_order_release);
}

uint32_t acceleration_get() {
  return g_settings.acceleration;
}

void acceleration_save() {
  storage_save_settings();
  LOG_I("Acceleration saved: %u", g_settings.acceleration);
}
