// Names used by the native tests, forwarding to the production headers.
// Add no logic here: a copy would keep passing after production changed.
#pragma once
#include <cmath>
#include <cstdint>
#include "../../src/control/system_state.h"
#include "../../src/control/motion_policy.h"

inline float rpmToStepHz_testable(float rpm, float gear, float work_m, float roller_m, uint32_t spr) {
  return rpm * motion_steps_per_workpiece_rev(float(spr) * gear, work_m, roller_m) / 60.0f;
}
inline float stepHzToRpm_testable(uint32_t hz, float gear, float work_m, float roller_m, uint32_t spr, float cal = 1.0f) {
  return motion_step_hz_to_rpm(float(hz), spr, gear, work_m, roller_m, cal);
}
inline long angleToSteps_testable(float degrees, float gear, float work_m, float roller_m, uint32_t spr, float cal) {
  int32_t steps = 0;
  motion_checked_steps(double(degrees) / 360.0 * double(motion_steps_per_workpiece_rev(float(spr) * gear, work_m, roller_m)) * cal, steps);
  return steps;
}
inline float adcToRpm_testable(float adc, float adc_ref, float min_rpm, float max_rpm, float snap_adc = 0.0f) {
  return min_rpm + motion_pot_normalized(adc, adc_ref, snap_adc) * (max_rpm - min_rpm);
}
inline bool pot_should_override_testable(float adc, float last, float threshold) { return motion_pot_takeover(adc, last, threshold); }
inline bool calibration_validate_testable(float factor) { return motion_calibration_factor_valid(factor); }
inline uint32_t microstep_steps_per_rev_testable(uint32_t microstep) { return 200u * microstep; }
inline bool microstep_is_valid_testable(uint32_t microstep) { return motion_microstep_valid(microstep); }
inline uint32_t microstep_init_default_testable(uint32_t stored) { return motion_microstep_or_default(stored); }
inline const char* microstep_get_string_testable(uint32_t microstep) { return motion_microstep_name(microstep); }
inline uint32_t acceleration_clamp_testable(uint32_t accel) { return motion_acceleration_clamp(accel); }
inline uint32_t acceleration_init_default_testable(uint32_t stored) { return motion_acceleration_or_default(stored); }
inline int direction_with_invert_testable(int dir, bool invert) { return motion_direction_cw(dir == 0, invert) ? 0 : 1; }
inline int direction_source_testable(bool override_active, int override_dir, bool switch_enabled, bool switch_high, int stored, bool invert) {
  return motion_direction_cw(motion_requested_cw(override_active, override_dir == 0, switch_enabled, switch_high, stored == 0), invert) ? 0 : 1;
}
inline uint32_t pulse_clamp_on_ms_testable(uint32_t ms) { return motion_pulse_clamp_ms(ms); }
inline uint32_t pulse_clamp_off_ms_testable(uint32_t ms) { return motion_pulse_clamp_ms(ms); }
inline bool step_angle_bounds_valid_testable(float degrees) { return motion_step_angle_valid(degrees); }
