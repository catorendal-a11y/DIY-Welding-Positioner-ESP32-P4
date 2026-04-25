// TIG Rotator Controller - Pure Math for Speed Control Testing
// Extracted from speed.cpp for native unit testing (no Arduino dependencies)

#pragma once
#include <cmath>
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// RPM CONVERSION — workpiece RPM to motor step frequency (Hz)
// Parameters match the runtime values from config.h and microstep.h:
//   gear_ratio     = GEAR_RATIO (108)
//   d_emne         = D_EMNE (0.300)
//   d_rulle        = D_RULLE (0.080)
//   steps_per_rev  = microstep_get_steps_per_rev() (e.g. 3200 for 1/16 default)
// ───────────────────────────────────────────────────────────────────────────────
inline float rpmToStepHz_testable(float rpm_workpiece, float gear_ratio,
                                  float d_emne, float d_rulle,
                                  uint32_t steps_per_rev) {
  return rpm_workpiece * gear_ratio * (d_emne / d_rulle)
         * (float)steps_per_rev / 60.0f;
}

// ───────────────────────────────────────────────────────────────────────────────
// REVERSE RPM CONVERSION — motor step frequency (Hz) to workpiece RPM
// Mirrors speed_get_actual_rpm() from speed.cpp
// Production also applies calibration_apply_angle (divide by cal_factor)
// ───────────────────────────────────────────────────────────────────────────────
inline float stepHzToRpm_testable(uint32_t hz, float gear_ratio,
                                  float d_emne, float d_rulle,
                                  uint32_t steps_per_rev,
                                  float cal_factor = 1.0f) {
  if (hz == 0) return 0.0f;
  float rpm_motor = (float)hz * 60.0f / (float)steps_per_rev;
  float rpm_workpiece = rpm_motor / gear_ratio * (d_rulle / d_emne);
  return rpm_workpiece / cal_factor;
}

// ───────────────────────────────────────────────────────────────────────────────
// ANGLE CONVERSION — workpiece angle (degrees) to motor steps
// Must match rpmToStepHz_testable (same d_emne/d_rulle factor).
// cal_factor = calibration factor (1.0 = no correction)
// ───────────────────────────────────────────────────────────────────────────────
inline long angleToSteps_testable(float degrees, float gear_ratio,
                                  float d_emne, float d_rulle,
                                  uint32_t steps_per_rev, float cal_factor) {
  float motor_deg = degrees * gear_ratio * (d_emne / d_rulle);
  long steps = (long)(motor_deg / 360.0f * (float)steps_per_rev);
  return (long)((float)steps * cal_factor);
}

// ───────────────────────────────────────────────────────────────────────────────
// ADC TO RPM — potentiometer ADC reading to workpiece RPM
// Mirrors the normalization logic from speed_apply() in speed.cpp
// adc_ref = 3315 (calibrated pot reference for ESP32-P4)
// snap_max_rpm_adc: if > 0 and clamped ADC <= this, normalized = 1 (matches speed_apply)
// ───────────────────────────────────────────────────────────────────────────────
inline float adcToRpm_testable(float adc_value, float adc_ref,
                               float min_rpm, float max_rpm,
                               float snap_max_rpm_adc = 0.0f) {
  float clamped = adc_value;
  if (clamped < 0.0f) clamped = 0.0f;
  if (clamped > 4095.0f) clamped = 4095.0f;
  float normalized = (adc_ref - clamped) / adc_ref;
  if (normalized < 0.0f) normalized = 0.0f;
  if (normalized > 1.0f) normalized = 1.0f;
  if (snap_max_rpm_adc > 0.0f && clamped <= snap_max_rpm_adc) normalized = 1.0f;
  return min_rpm + normalized * (max_rpm - min_rpm);
}

// ───────────────────────────────────────────────────────────────────────────────
// POT OVERRIDE — determines whether pot movement overrides slider control
// Mirrors the logic in speed_apply(): if |adc - lastPotAdc| > threshold,
// slider control is released and pot takes over.
// ───────────────────────────────────────────────────────────────────────────────
inline bool pot_should_override_testable(float current_adc, float last_adc,
                                         float threshold) {
  float delta = current_adc - last_adc;
  if (delta < 0.0f) delta = -delta;
  return delta > threshold;
}

// ───────────────────────────────────────────────────────────────────────────────
// CALIBRATION — apply/validate calibration factor
// Mirrors calibration.cpp logic
// ───────────────────────────────────────────────────────────────────────────────
inline long calibration_apply_steps_testable(long steps, float factor) {
  return (long)((float)steps * factor);
}

inline float calibration_apply_angle_testable(float angle, float factor) {
  return angle / factor;
}

inline bool calibration_validate_testable(float factor) {
  return (factor >= 0.5f && factor <= 1.5f);
}

// Mirrors production calibration_validate() which resets to 1.0 if invalid
// Returns the resulting factor (1.0 if invalid, original if valid)
inline float calibration_validate_with_reset_testable(float factor) {
  if (factor < 0.5f || factor > 1.5f) return 1.0f;
  return factor;
}

inline float calibration_clamp_testable(float factor) {
  if (factor < 0.5f) return 0.5f;
  if (factor > 1.5f) return 1.5f;
  return factor;
}

// ───────────────────────────────────────────────────────────────────────────────
// MICROSTEP — steps per revolution calculation
// Mirrors microstep_get_steps_per_rev() from microstep.cpp
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t microstep_steps_per_rev_testable(uint32_t microstep) {
  return 200 * microstep;
}

inline bool microstep_is_valid_testable(uint32_t microstep) {
  return (microstep == 4 || microstep == 8 || microstep == 16 || microstep == 32);
}

// ───────────────────────────────────────────────────────────────────────────────
// ACCELERATION — clamp to valid range
// Mirrors acceleration.cpp logic (ACCEL_MIN=1000, ACCEL_MAX=30000)
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t acceleration_clamp_testable(uint32_t accel) {
  if (accel < 1000) return 1000;
  if (accel > 30000) return 30000;
  return accel;
}

// ───────────────────────────────────────────────────────────────────────────────
// ACCELERATION INIT DEFAULT — mirrors acceleration_init() in acceleration.cpp
// Falls back to 7500 if stored value is outside [1000, 30000]
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t acceleration_init_default_testable(uint32_t stored) {
  if (stored < 1000 || stored > 30000) return 7500;
  return stored;
}

// ───────────────────────────────────────────────────────────────────────────────
// DIRECTION INVERSION — mirrors speed_get_direction() logic
// ───────────────────────────────────────────────────────────────────────────────
inline int direction_with_invert_testable(int dir, bool invert) {
  if (invert) return (dir == 0) ? 1 : 0;  // 0=CW, 1=CCW
  return dir;
}

inline int direction_source_testable(bool program_override_active,
                                     int program_override_dir,
                                     bool dir_switch_enabled,
                                     bool dir_switch_high,
                                     int stored_dir,
                                     bool invert) {
  int dir = stored_dir;
  if (program_override_active) {
    dir = program_override_dir;
  } else if (dir_switch_enabled) {
    dir = dir_switch_high ? 0 : 1;
  }
  return direction_with_invert_testable(dir, invert);
}

// ───────────────────────────────────────────────────────────────────────────────
// PULSE TIME CLAMPS — mirrors pulse.cpp constrain logic
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t pulse_clamp_on_ms_testable(uint32_t ms) {
  if (ms < 100) return 100;
  if (ms > 10000) return 10000;
  return ms;
}

inline uint32_t pulse_clamp_off_ms_testable(uint32_t ms) {
  if (ms < 100) return 100;
  if (ms > 10000) return 10000;
  return ms;
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP ANGLE BOUNDS — mirrors step_execute() validation in step_mode.cpp
// ───────────────────────────────────────────────────────────────────────────────
inline bool step_angle_bounds_valid_testable(float deg) {
  return deg > 0.0f && deg <= 3600.0f;
}

// ───────────────────────────────────────────────────────────────────────────────
// MICROSTEP STRING — mirrors microstep_get_string() in microstep.cpp
// ───────────────────────────────────────────────────────────────────────────────
inline const char* microstep_get_string_testable(uint32_t ms) {
  switch (ms) {
    case 4:  return "1/4";
    case 8:  return "1/8";
    case 16: return "1/16";
    case 32: return "1/32";
    default: return "1/16";
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// MICROSTEP INIT DEFAULT — mirrors microstep_init() in microstep.cpp
// Falls back to 16 if stored value is not valid (4/8/16/32)
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t microstep_init_default_testable(uint32_t stored) {
  if (stored != 4 && stored != 8 && stored != 16 && stored != 32) return 16;
  return stored;
}

// ───────────────────────────────────────────────────────────────────────────────
// MILLI-HZ WITH START_SPEED FLOOR
// Mirrors motor_milli_hz_for_rpm_calibrated() in motor.cpp — takes a calibrated
// Hz and returns milliHz clamped to at least start_speed*1000 so the stepper
// pulse train stays reliable at very low RPM.
//
// Production signature takes RPM and internally calls rpmToStepHzCalibrated;
// the testable version takes Hz directly so it can be exercised from unit
// tests without the calibration global.
// ───────────────────────────────────────────────────────────────────────────────
inline uint32_t milli_hz_floor_testable(float hz, uint32_t start_speed_hz) {
  if (std::isnan(hz) || hz < 0.0f) hz = 0.0f;
  double mhzD = (double)hz * 1000.0;
  const double kMaxU32 = 4294967295.0;
  if (mhzD > kMaxU32) mhzD = kMaxU32;
  uint32_t mhz = (uint32_t)mhzD;
  uint32_t floor_mhz = start_speed_hz * 1000u;
  if (mhz < floor_mhz) mhz = floor_mhz;
  return mhz;
}
