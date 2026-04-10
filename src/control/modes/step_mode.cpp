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
  if (state != STATE_IDLE) return;

  // Validate angle range
  if (angle_deg <= 0.0f || angle_deg > 3600.0f) {
    LOG_W("Invalid step angle: %.1f", angle_deg);
    return;
  }

  stepCurrentAngle = angle_deg;
  long steps = angleToSteps(angle_deg);
  if (speed_get_direction() == DIR_CCW) {
    steps = -steps;
  }

  LOG_I("Step mode: %.1f deg (%ld steps)", angle_deg, steps);

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    uint32_t hz = (uint32_t)rpmToStepHzCalibrated(speed_get_target_rpm());
    stepper->setSpeedInHz(hz);
    stepper->applySpeedAcceleration();
    stepper->move(steps);
    digitalWrite(PIN_ENA, LOW);
  }
  xSemaphoreGive(g_stepperMutex);

  accumulatedAngle += angle_deg;
  stepsTaken++;

  control_transition_to(STATE_STEP);
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void step_update() {
  if (control_get_state() != STATE_STEP) return;

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  FastAccelStepper* stepper = motor_get_stepper();
  bool done = (stepper != nullptr && !stepper->isRunning());
  xSemaphoreGive(g_stepperMutex);

  if (done) {
    LOG_D("Step complete: %.1f deg", stepCurrentAngle);
    control_transition_to(STATE_STOPPING);
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
