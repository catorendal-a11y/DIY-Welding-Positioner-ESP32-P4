// TIG Rotator Controller - Continuous Rotation Mode
// Motor runs continuously at speed set by pot or slider

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

static uint32_t continuousStartMs = 0;
static uint32_t continuousAutoStopMs = 0;
static bool continuousAutoStopActive = false;

void continuous_stop();

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE ENTRY
// ───────────────────────────────────────────────────────────────────────────────
void continuous_start(bool soft_start, uint32_t auto_stop_ms) {
  if (control_get_state() != STATE_IDLE) return;

  LOG_I("Continuous mode: start soft=%d auto_stop_ms=%lu", soft_start ? 1 : 0,
        (unsigned long)auto_stop_ms);

  if (soft_start) {
    motor_apply_soft_start_acceleration();
  }

  motor_set_target_milli_hz(motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm()));

  // Set direction and enable motor
  Direction dir = speed_get_direction();
  bool started = (dir == DIR_CW) ? motor_run_cw() : motor_run_ccw();
  if (!started) {
    motor_restore_configured_acceleration();
    LOG_W("Continuous mode: start blocked");
    return;
  }

  // Transition to RUNNING state
  if (!control_transition_to(STATE_RUNNING)) {
    motor_halt();
    motor_restore_configured_acceleration();
    return;
  }

  continuousStartMs = millis();
  continuousAutoStopMs = auto_stop_ms;
  continuousAutoStopActive = (auto_stop_ms > 0u);
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void continuous_update() {
  if (continuousAutoStopActive && control_get_state() == STATE_RUNNING) {
    uint32_t elapsed = millis() - continuousStartMs;
    if (elapsed >= continuousAutoStopMs) {
      continuousAutoStopActive = false;
      LOG_I("Continuous mode: auto stop after %lu ms", (unsigned long)continuousAutoStopMs);
      continuous_stop();
    }
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE STOP
// ───────────────────────────────────────────────────────────────────────────────
void continuous_stop() {
  if (control_get_state() != STATE_RUNNING) return;

  continuousAutoStopActive = false;
  LOG_I("Continuous mode: stop");
  control_transition_to(STATE_STOPPING);
}
