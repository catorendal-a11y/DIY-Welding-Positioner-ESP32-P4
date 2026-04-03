// Acceleration - Motor acceleration/deceleration control
#include "acceleration.h"
#include "../config.h"
#include "../storage/storage.h"

#define ACCEL_MIN  1000
#define ACCEL_MAX  20000

void acceleration_init() {
  if (g_settings.acceleration < ACCEL_MIN || g_settings.acceleration > ACCEL_MAX) {
    g_settings.acceleration = 5000;
  }
  LOG_I("Acceleration: %u", g_settings.acceleration);
}

static volatile bool accelApplyPending = false;

void acceleration_set(uint32_t accel) {
  g_settings.acceleration = constrain(accel, ACCEL_MIN, ACCEL_MAX);
  accelApplyPending = true;
  LOG_I("Acceleration set to: %u", g_settings.acceleration);
}

bool acceleration_has_pending_apply() {
  return accelApplyPending;
}

void acceleration_clear_pending() {
  accelApplyPending = false;
}

uint32_t acceleration_get() {
  return g_settings.acceleration;
}

void acceleration_save() {
  storage_save_settings();
  LOG_I("Acceleration saved: %u", g_settings.acceleration);
}
