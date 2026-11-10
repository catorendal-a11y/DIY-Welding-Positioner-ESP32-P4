#pragma once
#include <atomic>
#include <cstdint>

// Shared by production and native regression tests. A stop invalidates every
// ticket issued before it; no producer can clear the stop latch.
class MotionGate {
 public:
  uint32_t ticket() const { return generation.load(); }
  void stop() {
    pending.store(true);
    generation.fetch_add(1);
  }
  bool blocked() const { return pending.load(); }
  bool valid(uint32_t value) const { return !blocked() && value == ticket(); }
  bool takeStop() { return pending.exchange(false); }

 private:
  std::atomic<uint32_t> generation{0};
  std::atomic<bool> pending{false};
};

enum class PedalEdge { None, Start, Stop };
// Asymmetric pedal debounce: START requires ~25 ms of stable LOW (5 samples
// at the 5 ms input rate) so a TIG/HF glitch on GPIO33 cannot command a start;
// STOP reacts on the first released sample — release latency stays minimal.
constexpr uint32_t kPedalStartDebounceMs = 25;
class PedalInterlock {
 public:
  PedalEdge update(bool enabled, bool safe, bool pressed, uint32_t now) {
    if (!enabled || !safe) {
      const bool stop = down;
      armed = down = released = false;
      pressSinceMs = 0;
      return stop ? PedalEdge::Stop : PedalEdge::None;
    }
    if (!pressed) {
      pressSinceMs = 0;
      if (!released) {
        released = true;
        releasedAt = now;
      }
      if (now - releasedAt >= 50u) armed = true;
      if (down) {
        down = false;
        return PedalEdge::Stop;
      }
    } else {
      released = false;
      if (down) return PedalEdge::None;
      if (pressSinceMs == 0u) pressSinceMs = now;  // First pressed sample.
      if (armed && now - pressSinceMs >= kPedalStartDebounceMs) {
        armed = false;
        down = true;
        pressSinceMs = 0;
        return PedalEdge::Start;
      }
    }
    return PedalEdge::None;
  }

 private:
  bool armed = false, down = false, released = false;
  uint32_t releasedAt = 0, pressSinceMs = 0;
};

// The panel pot legitimately reaches 0 V (max speed), so 0 V alone proves nothing.
// A lost supply lead jumps to the rail within one 20 ms sample; a hand cannot.
// Keep the last speed while suspect, latch after three samples, clear on a real reading.
class PotBreakDetector {
 public:
  void update(int raw) {
    if (raw > kRail) {
      last = raw; suspect = 0; latched = false;
    } else if (last >= kJump && !latched && ++suspect >= kConfirm) {
      latched = true;
    }
  }
  bool holding() const { return suspect > 0 || latched; }
  bool broken() const { return latched; }

 private:
  static constexpr int kRail = 40, kJump = 1500, kConfirm = 3;
  int last = 0, suspect = 0;
  bool latched = false;
};

// ADS1115 single-ended codes dip a little below zero at 0 V, which is full pedal.
constexpr int kAdsMinValid = -64, kAdsMaxValid = 28000, kAdsInvalidLimit = 3;
inline bool ads_code_in_range(int code) { return code >= kAdsMinValid && code <= kAdsMaxValid; }

// Another task can stamp between the reader's millis() and its load, leaving the stamp a
// few ms ahead of `now`. Unsigned subtraction reads that as 49 days old: a live task as dead.
inline bool timestamp_within(uint32_t now, uint32_t stamp, uint32_t deadline) {
  const int32_t age = static_cast<int32_t>(now - stamp);
  return age >= -5 && age <= static_cast<int32_t>(deadline);
}

inline bool input_sample_fresh(bool valid, uint32_t sample, uint32_t now, uint32_t deadline = 150u) {
  return valid && timestamp_within(now, sample, deadline);
}

// An enabled pedal is healthy when no ADS1115 was detected at boot (GPIO33
// switch-only operation with panel-pot speed) or when the ADS1115 sample is
// fresh. A detected ADS1115 that stops delivering samples stays unhealthy so
// motion blocks instead of silently falling back to the panel pot.
inline bool pedal_input_healthy(bool enabled, bool adsConnected, bool adsValid, uint32_t adsSampleMs,
                                uint32_t now, uint32_t deadline = 150u) {
  if (!enabled || !adsConnected) return true;
  return input_sample_fresh(adsValid, adsSampleMs, now, deadline);
}
