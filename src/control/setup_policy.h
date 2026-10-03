#pragma once
#include <cstdint>
enum class SetupStage : uint8_t { Motor, Direction, Calibration, Check, Saving, Complete };
struct SetupProgress {
  SetupStage stage = SetupStage::Motor;
  bool motor_saved = false, direction_confirmed = false, calibration_saved = false;
  bool estop_seen = false, estop_released = false, reset_seen = false;
  bool start_requested = false, running_seen = false, stop_requested = false, stop_seen = false;
  void observe(bool physical, bool locked, bool idle, bool running) {
    if (stage != SetupStage::Check) return;
    if (physical && locked) estop_seen = true;
    if (estop_seen && !physical) estop_released = true;
    if (estop_released && !locked && idle) reset_seen = true;
    if (reset_seen && start_requested && running) running_seen = true;
    if (running_seen && stop_requested && idle && !locked) stop_seen = true;
  }
  bool ready() const {
    switch (stage) {
      case SetupStage::Motor: return motor_saved;
      case SetupStage::Direction: return direction_confirmed;
      case SetupStage::Calibration: return calibration_saved;
      case SetupStage::Check: return reset_seen && stop_seen;
      default: return false;
    }
  }
  bool advance() {
    if (!ready()) return false;
    stage = static_cast<SetupStage>(static_cast<uint8_t>(stage) + 1); return true;
  }
};
// Existing valid settings without the new key remain configured.
inline bool setup_migration_completed(bool key_present, bool value) { return !key_present || value; }
