// TIG Rotator Controller - Step Mode (fixed workpiece angle per move)

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../safety/safety.h"
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
static long step_sequence_signed_steps = 0;
static uint16_t step_sequence_target_repeats = 1;
static uint16_t step_sequence_completed = 0;
static uint32_t step_sequence_dwell_ms = 0;
static bool step_waiting_dwell = false;
static uint32_t step_dwell_until_ms = 0;

static void start_step_move_locked(FastAccelStepper* stepper) {
  accumulatedAngle = 0.0f;
  step_move_start_pos = stepper->getCurrentPosition();
  motor_apply_speed_for_rpm_locked(speed_get_target_rpm());
  digitalWrite(PIN_ENA, LOW);
  stepper->move(step_sequence_signed_steps);
  step_move_steps_total = labs(step_sequence_signed_steps);
  step_finish_earliest_ms = millis() + 50u;
  step_waiting_dwell = false;
}

void step_execute_sequence(float angle_deg, uint16_t repeats, float dwell_sec);

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE EXECUTE
// ───────────────────────────────────────────────────────────────────────────────
void step_execute(float angle_deg) {
  step_execute_sequence(angle_deg, 1, 0.0f);
}

void step_execute_sequence(float angle_deg, uint16_t repeats, float dwell_sec) {
  if (control_get_state() != STATE_IDLE) return;

  // Validate angle range
  if (angle_deg <= 0.0f || angle_deg > 3600.0f) {
    LOG_W("Invalid step angle: %.1f", angle_deg);
    return;
  }

  long steps = angleToSteps(angle_deg);
  if (speed_get_direction() == DIR_CCW) {
    steps = -steps;
  }
  if (steps == 0) {
    LOG_W("Step mode: zero steps for angle %.1f", angle_deg);
    return;
  }
  if (repeats < 1) repeats = 1;
  if (repeats > 99) repeats = 99;
  if (dwell_sec < 0.0f) dwell_sec = 0.0f;
  if (dwell_sec > 30.0f) dwell_sec = 30.0f;

  LOG_I("Step mode: %.1f deg (%ld steps) x%u dwell=%.1fs", angle_deg, steps, repeats, dwell_sec);

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (safety_inhibit_motion()) {
    xSemaphoreGive(g_stepperMutex);
    return;
  }
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper == nullptr) {
    LOG_W("Step mode: no stepper");
    xSemaphoreGive(g_stepperMutex);
    return;
  }

  stepCurrentAngle = angle_deg;
  accumulatedAngle = 0.0f;
  step_sequence_signed_steps = steps;
  step_sequence_target_repeats = repeats;
  step_sequence_completed = 0;
  step_sequence_dwell_ms = (uint32_t)(dwell_sec * 1000.0f + 0.5f);
  step_waiting_dwell = false;
  step_dwell_until_ms = 0;
  start_step_move_locked(stepper);
  // STATE_STEP before give: otherwise speed_apply() can setSpeedInMilliHz during move().
  if (!control_transition_to(STATE_STEP)) {
    stepper->forceStop();
    digitalWrite(PIN_ENA, HIGH);
    step_move_steps_total = 0;
    step_finish_earliest_ms = 0u;
  }
  xSemaphoreGive(g_stepperMutex);
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void step_update() {
  if (control_get_state() != STATE_STEP) return;

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  FastAccelStepper* stepper = motor_get_stepper();
  if (step_waiting_dwell) {
    const uint32_t now = millis();
    if (stepper == nullptr) {
      step_sequence_completed = step_sequence_target_repeats;
      xSemaphoreGive(g_stepperMutex);
      control_transition_to(STATE_STOPPING);
      return;
    }
    if ((int32_t)(now - step_dwell_until_ms) >= 0) {
      start_step_move_locked(stepper);
    }
    xSemaphoreGive(g_stepperMutex);
    return;
  }

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
    step_sequence_completed++;
    LOG_D("Step complete: %.1f deg", stepCurrentAngle);
    if (step_sequence_completed < step_sequence_target_repeats && stepper != nullptr) {
      if (step_sequence_dwell_ms > 0u) {
        digitalWrite(PIN_ENA, HIGH);
        step_waiting_dwell = true;
        step_dwell_until_ms = millis() + step_sequence_dwell_ms;
      } else {
        xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
        FastAccelStepper* nextStepper = motor_get_stepper();
        if (nextStepper != nullptr) {
          start_step_move_locked(nextStepper);
        } else {
          step_sequence_completed = step_sequence_target_repeats;
        }
        xSemaphoreGive(g_stepperMutex);
      }
      if (step_sequence_completed < step_sequence_target_repeats) return;
    }
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
  step_sequence_signed_steps = 0;
  step_sequence_target_repeats = 1;
  step_sequence_completed = 0;
  step_sequence_dwell_ms = 0;
  step_waiting_dwell = false;
  step_dwell_until_ms = 0;
  step_finish_earliest_ms = 0u;
  LOG_I("Step accumulator reset");
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP MODE GETTERS
// ───────────────────────────────────────────────────────────────────────────────
float step_get_current_angle() { return stepCurrentAngle; }
float step_get_accumulated() { return accumulatedAngle; }
long step_get_count() { return stepsTaken; }
