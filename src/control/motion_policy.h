#pragma once
#include <cstdint>
#include <limits>

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
