#pragma once
#include <cstdint>
using MoveResultCode = int8_t;
inline constexpr MoveResultCode MOVE_OK = 0;
class FastAccelStepper {
 public:
  bool running = false;
  int32_t position = 0, milliHz = 0;
  int8_t commandResult = 0, speedResult = 0, accelerationResult = 0;
  unsigned starts = 0, moves = 0;
  bool isRunning() { return running; }
  void setDirectionPin(int, bool, uint16_t) {}
  int8_t setAcceleration(int32_t) { return accelerationResult; }
  void setLinearAcceleration(uint32_t) {}
  int8_t setSpeedInHz(uint32_t hz) { return setSpeedInMilliHz(hz * 1000u); }
  int8_t setSpeedInMilliHz(uint32_t hz) { if (!speedResult) milliHz = static_cast<int32_t>(hz); return speedResult; }
  void applySpeedAcceleration() {}
  MoveResultCode runForward() { if (!commandResult) { running = true; ++starts; } return commandResult; }
  MoveResultCode runBackward() { return runForward(); }
  MoveResultCode move(int32_t) { if (!commandResult) { running = true; ++moves; } return commandResult; }
  void stopMove() {} // Deceleration completion is controlled explicitly by each test.
  void forceStop() { running = false; }
  int32_t getCurrentPosition() { return position; }
  int32_t getCurrentSpeedInMilliHz() { return running ? milliHz : 0; }
};
inline FastAccelStepper testStepper;
class FastAccelStepperEngine {
 public:
  void init(int) {}
  FastAccelStepper* stepperConnectToPin(int) { return &testStepper; }
};
