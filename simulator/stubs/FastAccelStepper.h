#pragma once
#define TICKS_PER_S 16000000L
#include <Arduino.h>
// Public 1.4.0 result types; do not let host tests accept integer comparisons.
enum class MoveResultCode : int8_t {
  OK = 0, ErrorNoDirectionPin = -1,
  ErrorSpeedIsUndefined = -2, ErrorAccelerationIsUndefined = -3
};
enum class FasDriver : uint8_t { RMT = 1, DONT_CARE = 255 };
// Only hardware is simulated; dispatcher and modes are production code.
class FastAccelStepper {
 public:
  uint16_t directionDelay = 200;
  const char* driverTypeString() const { return "RMT"; }
  uint16_t getDirChangeBeforeTicks() const { return 24000; }
  uint8_t getDirChangeBeforePauseCount() const { return 1; }
  uint16_t getDirChangeAfterTicks() const { return directionDelay * 16; }
  MoveResultCode commandResult = MoveResultCode::OK;
  double motionScale = 1; // Test-only acceleration of physical travel, not control timers.
  void setDirectionPin(int, bool, uint16_t delay) { directionDelay = delay; }
  int8_t setAcceleration(int32_t) { return 0; }
  void setLinearAcceleration(uint32_t) {}
  int8_t setSpeedInHz(uint32_t hz) { return setSpeedInMilliHz(hz * 1000u); }
  int8_t setSpeedInMilliHz(uint32_t hz) { update(); milliHz = hz; return 0; }
  void applySpeedAcceleration() {}
  MoveResultCode runForward() { return start(1, false, 0); }
  MoveResultCode runBackward() { return start(-1, false, 0); }
  MoveResultCode move(int32_t steps) { return start(steps >= 0 ? 1 : -1, true, std::abs(int64_t(steps))); }
  bool isRunning() { update(); return running; }
  void stopMove() { update(); stopping = true; stopAt = millis(); }
  // The real forceStop drains queued pulses; ENA inhibition is separate.
  void forceStop() { update(); if (!stopping) { stopping = true; stopAt = millis(); } }
  int32_t getCurrentPosition() { update(); return static_cast<int32_t>(position); }
  int32_t getCurrentSpeedInMilliHz() { update(); return running ? int32_t(milliHz) * direction : 0; }
 private:
  bool running = false, finite = false, stopping = false;
  int direction = 1;
  uint32_t position = 0, milliHz = 0, lastMs = 0, stopAt = 0;
  double remaining = 0, fraction = 0;
  MoveResultCode start(int dir, bool bounded, double distance) {
    if (commandResult != MoveResultCode::OK) return commandResult;
    update(); direction = dir; finite = bounded; remaining = distance;
    stopping = false; running = true; lastMs = millis(); return MoveResultCode::OK;
  }
  void update() {
    const uint32_t now = millis(), elapsed = now - lastMs; lastMs = now;
    if (!running) return;
    if (stopping && now - stopAt >= 30u) { running = false; stopping = false; return; }
    double travel = double(milliHz) * elapsed / 1000000.0 * motionScale;
    if (finite) { travel = std::min(travel, remaining); remaining -= travel; if (remaining <= 0) running = false; }
    fraction += travel;
    const uint32_t steps = uint32_t(fraction); fraction -= steps;
    position += direction > 0 ? steps : uint32_t(0u - steps);
  }
};
inline FastAccelStepper simStepper;
class FastAccelStepperEngine {
 public:
  void init(int) {}
  FastAccelStepper* stepperConnectToPin(uint8_t, FasDriver) { return &simStepper; }
};
