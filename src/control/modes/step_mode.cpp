// TIG Rotator Controller - Step Mode (fixed workpiece angle per move)

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <cstdlib>

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static float stepCurrentAngle = 10.0f;  // Default 10 degrees
// During STATE_STEP: degrees of travel completed in the *current* move (from stepper position).
// After each new step_execute: reset to 0, then ramps to stepCurrentAngle.
static float accumulatedAngle = 0.0f;
static long stepsTaken = 0;  // Completed step moves (increment when move finishes)
static int32_t step_move_start_pos = 0;
static long step_move_steps_total = 0;
// controlTask calls step_update() in the same loop iteration as step_execute(); FastAccelStepper
// may still report !isRunning() for a short window right after move(). Ignore completion until then.
static uint32_t step_finish_earliest_ms = 0;

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE EXECUTE
// ───────────────────────────────────────────────────────────────────────────────
void step_execute(float angle_deg) {
  if (control_get_state() != STATE_IDLE) return;

  // Validate angle range
  if (angle_deg <= 0.0f || angle_deg > 3600.0f) {
    LOG_W("Invalid step angle: %.1f", angle_deg);
    return;
  }

  stepCurrentAngle = angle_deg;
  accumulatedAngle = 0.0f;

  long steps = angleToSteps(angle_deg);
  if (speed_get_direction() == DIR_CCW) {
    steps = -steps;
  }

  LOG_I("Step mode: %.1f deg (%ld steps)", angle_deg, steps);

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    step_move_start_pos = stepper->getCurrentPosition();
    motor_apply_speed_for_rpm_locked(speed_get_target_rpm());
    stepper->move(steps);
    digitalWrite(PIN_ENA, LOW);
    step_move_steps_total = labs(steps);
    step_finish_earliest_ms = millis() + 50u;
  } else {
    step_move_steps_total = 0;
    step_finish_earliest_ms = 0u;
  }
  // STATE_STEP before give: otherwise speed_apply() can setSpeedInMilliHz during move().
  control_transition_to(STATE_STEP);
  xSemaphoreGive(g_stepperMutex);
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void step_update() {
  if (control_get_state() != STATE_STEP) return;

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr && step_move_steps_total > 0) {
    int32_t p = stepper->getCurrentPosition();
    int64_t delta = (int64_t)p - (int64_t)step_move_start_pos;
    int64_t moved = llabs(delta);
    if (moved > (int64_t)step_move_steps_total) moved = (int64_t)step_move_steps_total;
    float frac = (float)moved / (float)step_move_steps_total;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    accumulatedAngle = frac * stepCurrentAngle;
  }
  const uint32_t now = millis();
  const bool past_grace =
      (step_finish_earliest_ms == 0u) || ((int32_t)(now - step_finish_earliest_ms) >= 0);
  bool motion_idle = (stepper != nullptr && !stepper->isRunning());
  bool done = false;
  if (step_move_steps_total > 0) {
    done = past_grace && motion_idle;
  } else {
    done = past_grace && (stepper == nullptr || motion_idle);
  }
  xSemaphoreGive(g_stepperMutex);

  if (done) {
    accumulatedAngle = stepCurrentAngle;
    stepsTaken++;
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
  step_move_steps_total = 0;
  step_finish_earliest_ms = 0u;
  LOG_I("Step accumulator reset");
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE GETTERS
// ───────────────────────────────────────────────────────────────────────────────
float step_get_current_angle() { return stepCurrentAngle; }
float step_get_accumulated() { return accumulatedAngle; }
long step_get_count() { return stepsTaken; }
