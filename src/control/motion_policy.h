#pragma once
#include <cstdint>
#include <limits>
#include <cmath>

// Position moves are limited to less than half the 32-bit counter range.
inline uint32_t motion_position_distance(int32_t current, int32_t start) {
  const uint32_t delta = static_cast<uint32_t>(current) - static_cast<uint32_t>(start);
  return delta <= INT32_MAX ? delta : 0u - delta;
}

inline bool motion_direction_cw(bool requested_cw, bool inverted) {
  return requested_cw != inverted;
}

// Operational timeout, not a certified physical stop time. Allows linear ramp
// overhead plus the worst configured deceleration from the published step rate.
inline uint32_t motion_stop_timeout_ms(uint32_t milli_hz, uint32_t acceleration) {
  if (acceleration < 1000u) acceleration = 1000u;
  return 2000u + static_cast<uint32_t>((static_cast<uint64_t>(milli_hz) + acceleration - 1) / acceleration);
}

// The acceleration setting is steps/s2 at 1/16 microstepping, so a ramp takes the same
// physical time at every microstep setting. Existing 1/16 machines are unchanged.
inline uint32_t motion_scaled_acceleration(uint32_t acceleration, int microstep) {
  if (microstep <= 0) microstep = 16;
  return static_cast<uint32_t>(uint64_t(acceleration) * uint32_t(microstep) / 16u);
}

// Workpiece degrees travelled decelerating from rpm, where hz_at_16 is the step rate
// for that rpm at 1/16 and acceleration is the setting above.
inline float motion_stop_angle_deg(float rpm, float hz_at_16, uint32_t acceleration) {
  if (acceleration == 0u) return 0.0f;
  return rpm * 6.0f * (hz_at_16 / float(acceleration)) / 2.0f;
}

// Limits and conversions shared by the firmware and the native tests.
constexpr uint32_t MOTION_ACCEL_MIN = 1000u, MOTION_ACCEL_MAX = 30000u, MOTION_ACCEL_DEFAULT = 7500u;
constexpr uint32_t PULSE_MS_MIN = 100u, PULSE_MS_MAX = 10000u;

inline uint32_t motion_acceleration_clamp(uint32_t accel) {
  return accel < MOTION_ACCEL_MIN ? MOTION_ACCEL_MIN : accel > MOTION_ACCEL_MAX ? MOTION_ACCEL_MAX : accel;
}
inline uint32_t motion_acceleration_or_default(uint32_t stored) {
  return stored < MOTION_ACCEL_MIN || stored > MOTION_ACCEL_MAX ? MOTION_ACCEL_DEFAULT : stored;
}
inline uint32_t motion_pulse_clamp_ms(uint32_t ms) {
  return ms < PULSE_MS_MIN ? PULSE_MS_MIN : ms > PULSE_MS_MAX ? PULSE_MS_MAX : ms;
}
inline bool motion_calibration_factor_valid(float factor) {
  return std::isfinite(factor) && factor >= 0.5f && factor <= 1.5f;
}
inline bool motion_microstep_valid(uint32_t microstep) {
  return microstep == 4u || microstep == 8u || microstep == 16u || microstep == 32u;
}
inline uint32_t motion_microstep_or_default(uint32_t stored) {
  return motion_microstep_valid(stored) ? stored : 16u;
}
inline const char* motion_microstep_name(uint32_t microstep) {
  switch (microstep) {
    case 4u: return "1/4";
    case 8u: return "1/8";
    case 32u: return "1/32";
    default: return "1/16";
  }
}
inline bool motion_step_angle_valid(float degrees) {
  return std::isfinite(degrees) && degrees > 0.0f && degrees <= 3600.0f;
}
inline bool motion_requested_cw(bool program_override, bool program_cw, bool switch_enabled, bool switch_high,
                                bool stored_cw) {
  return program_override ? program_cw : switch_enabled ? switch_high : stored_cw;
}

// No slip at the roller contact: workpiece revolutions scale by roller / workpiece diameter.
inline float motion_steps_per_workpiece_rev(float steps_per_output_rev, float workpiece_m, float roller_m) {
  return steps_per_output_rev * (workpiece_m / roller_m);
}
inline float motion_step_hz_to_rpm(float hz, uint32_t steps_per_rev, float gear, float workpiece_m, float roller_m, float cal) {
  if (hz < 1e-9f || steps_per_rev == 0u) return 0.0f;
  return hz * 60.0f / float(steps_per_rev) / gear * (roller_m / workpiece_m) / cal;
}
// Pot wiper to 0..1, where ADC 0 is full speed. At or below snap_adc the result is full speed.
inline float motion_pot_normalized(float adc, float adc_ref, float snap_adc) {
  const float clamped = adc < 0.0f ? 0.0f : adc > 4095.0f ? 4095.0f : adc;
  if (snap_adc > 0.0f && clamped <= snap_adc) return 1.0f;
  const float normalized = (adc_ref - clamped) / adc_ref;
  return normalized < 0.0f ? 0.0f : normalized > 1.0f ? 1.0f : normalized;
}
inline bool motion_pot_takeover(float adc, float last_adc, float threshold) {
  return std::fabs(adc - last_adc) > threshold;
}
// Zero means rejected: a rate below the operational floor is never rounded up.
inline uint32_t motion_milli_hz(double hz, uint32_t floor_hz) {
  const double milli_hz = hz * 1000.0;
  if (!std::isfinite(milli_hz) || milli_hz < floor_hz * 1000.0 || milli_hz > UINT32_MAX) return 0u;
  return static_cast<uint32_t>(milli_hz);
}

enum class PulsePhase : uint8_t { On, Decelerating, Off, Complete };
enum class PulseAction : uint8_t { None, Stop, Start, Complete };

// Shared production timing core. Finite programs include their final OFF pause.
class PulseTimeline {
 public:
  void start(uint32_t now, uint32_t on, uint32_t off, uint16_t limit) {
    phase_ = PulsePhase::On;
    since_ = now;
    on_ = on;
    off_ = off;
    limit_ = limit;
    completed_ = 0;
  }
  PulseAction update(uint32_t now, bool moving) {
    if (phase_ == PulsePhase::On && now - since_ >= on_) {
      if (completed_ != UINT32_MAX) ++completed_;
      phase_ = PulsePhase::Decelerating;
      return PulseAction::Stop;
    }
    if (phase_ == PulsePhase::Decelerating && !moving) {
      phase_ = PulsePhase::Off;
      since_ = now;
    }
    if (phase_ == PulsePhase::Off && now - since_ >= off_) {
      if (limit_ && completed_ >= limit_) {
        phase_ = PulsePhase::Complete;
        return PulseAction::Complete;
      }
      phase_ = PulsePhase::On;
      since_ = now;
      return PulseAction::Start;
    }
    return PulseAction::None;
  }
  void cancel() { phase_ = PulsePhase::Complete; }
  PulsePhase phase() const { return phase_; }
  uint32_t completed() const { return completed_; }
 private:
  PulsePhase phase_ = PulsePhase::Complete;
  uint32_t since_ = 0, on_ = 0, off_ = 0, completed_ = 0;
  uint16_t limit_ = 0;
};

// FastAccelStepper relative moves and wrapped-position progress share this bound.
// Reject before narrowing: no negative sentinel can become a reversed move.
inline bool motion_checked_steps(double pulses, int32_t& out) {
  out = 0;
  if (!std::isfinite(pulses) || pulses < 1.0 || pulses > INT32_MAX) return false;
  out = static_cast<int32_t>(pulses);
  return true;
}
inline bool motion_move_timeout_ms(uint32_t pulses, uint32_t milli_hz, uint32_t stop_budget, uint32_t& out) {
  if (!pulses || !milli_hz) return false;
  const uint64_t budget = (uint64_t(pulses) * 1000000u + milli_hz - 1u) / milli_hz + 2ull * stop_budget;
  if (budget > INT32_MAX) return false; // Keep deadline comparisons wrap-safe.
  out = static_cast<uint32_t>(budget);
  return true;
}
