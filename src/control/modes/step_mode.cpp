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

  // Validate angle
  const float validAngles[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f};
  bool isValid = false;
  for (int i = 0; i < 6; i++) {
    if (angle_deg == validAngles[i]) {
      isValid = true;
      break;
    }
  }
  if (!isValid) {
    LOG_W("Invalid step angle: %.1f", angle_deg);
    return;
  }

  stepCurrentAngle = angle_deg;
  long steps = angleToSteps(angle_deg);

  LOG_I("Step mode: %.1f deg (%ld steps)", angle_deg, steps);

  // Set direction
  digitalWrite(PIN_DIR, (speed_get_direction() == DIR_CW) ? HIGH : LOW);

  // Enable and move
  digitalWrite(PIN_ENA, LOW);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->move(steps);
  }

  // Update accumulator
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
