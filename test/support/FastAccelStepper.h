#pragma once
#define TICKS_PER_S 16000000L
#include <cstdint>
enum class MoveResultCode : int8_t {
  OK = 0, ErrorNoDirectionPin = -1,
  ErrorSpeedIsUndefined = -2, ErrorAccelerationIsUndefined = -3
};
enum class FasDriver : uint8_t { RMT = 1, DONT_CARE = 255 };
class FastAccelStepper {
 public:
  uint16_t directionDelay = 200;
  const char* driverTypeString() const { return "RMT"; }
  uint16_t getDirChangeBeforeTicks() const { return 24000; }
  uint8_t getDirChangeBeforePauseCount() const { return 1; }
  uint16_t getDirChangeAfterTicks() const { return directionDelay * 16; }
  bool running = false;
  int32_t position = 0, milliHz = 0;
  MoveResultCode commandResult = MoveResultCode::OK;
  int8_t speedResult = 0, accelerationResult = 0;
  int32_t acceleration = 0;
  void (*beforeAcceleration)() = nullptr;
  bool drainForceStop = false;
  unsigned directionWrites = 0;
  unsigned starts = 0, moves = 0;
  bool isRunning() { return running; }
  void setDirectionPin(uint8_t, bool, uint16_t delay) { directionDelay = delay; ++directionWrites; }
  int8_t setAcceleration(int32_t value) {
    if (beforeAcceleration) beforeAcceleration();
    if (!accelerationResult) acceleration = value;
    return accelerationResult;
  }
  void setLinearAcceleration(uint32_t) {}
  int8_t setSpeedInHz(uint32_t hz) { return setSpeedInMilliHz(hz * 1000u); }
  int8_t setSpeedInMilliHz(uint32_t hz) { if (!speedResult) milliHz = static_cast<int32_t>(hz); return speedResult; }
  void applySpeedAcceleration() {}
  MoveResultCode runForward() { if (commandResult == MoveResultCode::OK) { running = true; ++starts; } return commandResult; }
  MoveResultCode runBackward() { return runForward(); }
  MoveResultCode move(int32_t) { if (commandResult == MoveResultCode::OK) { running = true; ++moves; } return commandResult; }
  void stopMove() {} // Deceleration completion is controlled explicitly by each test.
  void forceStop() { if (!drainForceStop) running = false; }
  int32_t getCurrentPosition() { return position; }
  int32_t getCurrentSpeedInMilliHz() { return running ? milliHz : 0; }
};
inline FastAccelStepper testStepper;
class FastAccelStepperEngine {
 public:
  void init(int) {}
  FasDriver selectedDriver = FasDriver::DONT_CARE;
  FastAccelStepper* stepperConnectToPin(uint8_t, FasDriver driver) { selectedDriver = driver; return &testStepper; }
};
