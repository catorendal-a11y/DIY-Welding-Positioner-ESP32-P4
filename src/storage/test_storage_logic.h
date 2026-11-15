// TIG Rotator Controller - Pure Storage/Config Logic
// The clamps storage.cpp applies to NVS data. storage.cpp calls these functions and
// test/test_utils tests them, so a change here is a change to both.

#pragma once
#include <cstdint>
#include <cstring>
#include "../control/motion_policy.h"  // PULSE_MS_MIN, PULSE_MS_MAX

// ───────────────────────────────────────────────────────────────────────────────
// SETTINGS CONSTRAIN — used by storage_decode_settings_doc()
// ───────────────────────────────────────────────────────────────────────────────

inline int settings_constrain_acceleration(int val) {
  if (val < 1000) return 1000;
  if (val > 30000) return 30000;
  return val;
}

inline int settings_constrain_microstep(int val) {
  if (val == 4 || val == 8 || val == 16 || val == 32) return val;
  return 16;
}

inline int settings_constrain_brightness(int val) {
  if (val < 10) return 10;
  if (val > 255) return 255;
  return val;
}

inline int settings_constrain_countdown(int val) {
  if (val < 1) return 1;
  if (val > 10) return 10;
  return val;
}

inline int settings_constrain_accent(int val) {
  if (val < 0) return 0;
  if (val > 7) return 7;
  return val;
}

inline float settings_constrain_calibration(float val) {
  if (val < 0.5f) return 0.5f;
  if (val > 1.5f) return 1.5f;
  return val;
}

// ───────────────────────────────────────────────────────────────────────────────
// PRESET CONSTRAIN — used by storage_decode_presets_buffer()
// ───────────────────────────────────────────────────────────────────────────────

inline float preset_constrain_rpm(float val, float min_rpm, float max_rpm) {
  if (val < min_rpm) return min_rpm;
  if (val > max_rpm) return max_rpm;
  return val;
}

inline uint32_t preset_constrain_pulse_on(uint32_t val) {
  if (val < PULSE_MS_MIN) return PULSE_MS_MIN;
  if (val > PULSE_MS_MAX) return PULSE_MS_MAX;
  return val;
}

inline uint32_t preset_constrain_pulse_off(uint32_t val) {
  if (val < PULSE_MS_MIN) return PULSE_MS_MIN;
  if (val > PULSE_MS_MAX) return PULSE_MS_MAX;
  return val;
}

inline float preset_constrain_step_angle(float val) {
  if (val < 0.1f) return 0.1f;
  if (val > 360.0f) return 360.0f;
  return val;
}

inline float preset_constrain_workpiece_diameter_mm(float val) {
  if (val < 1.0f || val > 20000.0f) return 0.0f;
  return val;
}

inline uint32_t preset_constrain_timer_ms(uint32_t val) {
  if (val < 1) return 1;
  if (val > 3600000) return 3600000;
  return val;
}
