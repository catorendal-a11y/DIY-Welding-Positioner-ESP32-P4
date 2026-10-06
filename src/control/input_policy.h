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

inline bool input_sample_fresh(bool valid, uint32_t sample, uint32_t now, uint32_t deadline = 150u) {
  return valid && now - sample <= deadline;
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
