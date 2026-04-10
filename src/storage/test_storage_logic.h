// TIG Rotator Controller - Pure Storage/Config Logic for Testing
// Extracted from storage.cpp and config.h for native unit testing

#pragma once
#include <cstdint>
#include <cstring>

// ───────────────────────────────────────────────────────────────────────────────
// SANITIZE ASCII — replace non-printable chars with '?'
// Mirrors sanitize_ascii() in config.h
// ───────────────────────────────────────────────────────────────────────────────

inline void sanitize_ascii_testable(char* buf, unsigned int len) {
  for (unsigned int i = 0; i < len && buf[i]; i++) {
    if (buf[i] < 0x20 || buf[i] > 0x7E) buf[i] = '?';
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SETTINGS CONSTRAIN — mirrors storage_load_settings() clamp logic
// ───────────────────────────────────────────────────────────────────────────────

inline int settings_constrain_acceleration(int val) {
  if (val < 1000) return 1000;
  if (val > 30000) return 30000;
  return val;
}

inline uint8_t settings_constrain_brightness(uint8_t val) {
  if (val < 10) return 10;
  if (val > 255) return 255;
  return val;
}

inline uint8_t settings_constrain_countdown(uint8_t val) {
  if (val < 1) return 1;
  if (val > 10) return 10;
  return val;
}

inline uint8_t settings_constrain_accent(uint8_t val) {
  if (val > 7) return 7;
  return val;
}

inline float settings_constrain_calibration(float val) {
  if (val < 0.5f) return 0.5f;
  if (val > 1.5f) return 1.5f;
  return val;
}

// ───────────────────────────────────────────────────────────────────────────────
// PRESET CONSTRAIN — mirrors storage_load_presets() clamp logic
// ───────────────────────────────────────────────────────────────────────────────

inline float preset_constrain_rpm(float val, float min_rpm, float max_rpm) {
  if (val < min_rpm) return min_rpm;
  if (val > max_rpm) return max_rpm;
  return val;
}

inline uint32_t preset_constrain_pulse_on(uint32_t val) {
  if (val < 10) return 10;
  if (val > 60000) return 60000;
  return val;
}

inline uint32_t preset_constrain_pulse_off(uint32_t val) {
  if (val < 10) return 10;
  if (val > 60000) return 60000;
  return val;
}

inline float preset_constrain_step_angle(float val) {
  if (val < 0.1f) return 0.1f;
  if (val > 360.0f) return 360.0f;
  return val;
}

inline uint32_t preset_constrain_timer_ms(uint32_t val) {
  if (val < 1) return 1;
  if (val > 3600000) return 3600000;
  return val;
}

// ───────────────────────────────────────────────────────────────────────────────
// WIFI BACKOFF — exponential reconnect interval
// Mirrors wifi_process_pending() in storage.cpp
// ───────────────────────────────────────────────────────────────────────────────

inline uint32_t wifi_backoff_interval(uint32_t current) {
  if (current < 300000) return current * 2;
  return 300000;
}

inline uint32_t wifi_backoff_reset() {
  return 30000;
}
