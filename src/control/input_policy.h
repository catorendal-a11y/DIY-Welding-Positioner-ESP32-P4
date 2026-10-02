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
class PedalInterlock {
 public:
  PedalEdge update(bool enabled, bool safe, bool pressed, uint32_t now) {
    if (!enabled || !safe) {
      const bool stop = down;
      armed = down = released = false;
      return stop ? PedalEdge::Stop : PedalEdge::None;
    }
    if (!pressed) {
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
      if (armed && !down) {
        armed = false;
        down = true;
        return PedalEdge::Start;
      }
    }
    return PedalEdge::None;
  }

 private:
  bool armed = false, down = false, released = false;
  uint32_t releasedAt = 0;
};

inline bool input_sample_fresh(bool valid, uint32_t sample, uint32_t now, uint32_t deadline = 150u) {
  return valid && now - sample <= deadline;
}
