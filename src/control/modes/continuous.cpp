// TIG Rotator Controller - Continuous Rotation Mode
// Motor runs continuously at speed set by pot or slider

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <FastAccelStepper.h>

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE ENTRY
// ───────────────────────────────────────────────────────────────────────────────
void continuous_start() {
  if (control_get_state() != STATE_IDLE) return;

  LOG_I("Continuous mode: start");

  // Get stepper instance
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    // Clear any previous stop state and reset queue
    // This is needed after forceStop() from ESTOP
    stepper->stopMove();  // Clear any forceStop state
    delay(10);  // Brief pause for state to clear
  }

  // CRITICAL: Set speed BEFORE starting motor
  // FastAccelStepper needs speed set before runForward/runBackward
  speed_apply();

  // Set direction and start motor
  Direction dir = speed_get_direction();
  if (dir == DIR_CW) {
    motor_run_cw();
  } else {
    motor_run_ccw();
  }

  // Transition to RUNNING state
  control_transition_to(STATE_RUNNING);
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void continuous_update() {
  // Speed is continuously updated by motorTask via speed_apply()
  // Nothing additional needed here
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE STOP
// ───────────────────────────────────────────────────────────────────────────────
void continuous_stop() {
  if (control_get_state() != STATE_RUNNING) return;

  LOG_I("Continuous mode: stop");
  control_transition_to(STATE_STOPPING);
}
