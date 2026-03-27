// TIG Rotator Controller - Step Mode
// Rotate workpiece by fixed angle: 5, 10, 15, 30, 45, 90 degrees

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static float stepCurrentAngle = 10.0f;  // Default 10 degrees
static float accumulatedAngle = 0.0f;   // Total rotation accumulator
static long stepsTaken = 0;             // Counter for display

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE EXECUTE
// ───────────────────────────────────────────────────────────────────────────────
void step_execute(float angle_deg) {
  // Can execute from IDLE or after previous step completes
  SystemState state = control_get_state();
  if (state != STATE_IDLE && state != STATE_STEP) return;

  // Validate angle (use epsilon for float comparison)
  const float validAngles[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f};
  bool isValid = false;
  for (int i = 0; i < 6; i++) {
    if (fabsf(angle_deg - validAngles[i]) < 0.01f) {
      isValid = true;
      angle_deg = validAngles[i];  // Snap to exact value
      break;
    }
  }
  if (!isValid) {
    LOG_W("Invalid step angle: %.1f", angle_deg);
    return;
  }

  stepCurrentAngle = angle_deg;
  long steps = angleToSteps(angle_deg);

  // Respect direction setting: negate steps for CCW
  Direction dir = speed_get_direction();
  if (dir == DIR_CCW) {
    steps = -steps;
  }

  LOG_I("Step mode: %.1f deg (%ld steps, %s)", angle_deg, abs(steps),
        (dir == DIR_CW) ? "CW" : "CCW");

  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    // Clear any forceStop state from ESTOP before moving
    stepper->stopMove();
    delay(10);

    stepper->setSpeedInHz((uint32_t)rpmToStepHz(1.0f));  // 1 RPM output
    stepper->move(steps);
  }

  // Update accumulator (always positive for display)
  accumulatedAngle += angle_deg;
  stepsTaken++;

  control_transition_to(STATE_STEP);
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void step_update() {
  if (control_get_state() != STATE_STEP) return;

  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr && !stepper->isRunning()) {
    // Step complete
    LOG_D("Step complete: %.1f deg", stepCurrentAngle);
    control_transition_to(STATE_IDLE);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE STOP (manual)
// ───────────────────────────────────────────────────────────────────────────────
void step_stop() {
  SystemState state = control_get_state();
  if (state != STATE_STEP) return;

  LOG_I("Step mode: manual stop");
  motor_halt();  // Immediate stop
  control_transition_to(STATE_IDLE);
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE RESET
// ───────────────────────────────────────────────────────────────────────────────
void step_reset_accumulator() {
  accumulatedAngle = 0.0f;
  stepsTaken = 0;
  LOG_I("Step accumulator reset");
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE GETTERS
// ───────────────────────────────────────────────────────────────────────────────
float step_get_current_angle() { return stepCurrentAngle; }
float step_get_accumulated() { return accumulatedAngle; }
long step_get_count() { return stepsTaken; }
