// TIG Rotator Controller - Jog Mode
// Motor runs only while button held, stops on release

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// JOG MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static float jogRPM = 0.5f;  // Default jog speed (workpiece RPM)

// ───────────────────────────────────────────────────────────────────────────────
// JOG MODE START
// ───────────────────────────────────────────────────────────────────────────────
void jog_start(Direction dir) {
  // Allow jog from IDLE or while already jogging (direction change)
  SystemState state = control_get_state();
  if (state != STATE_IDLE && state != STATE_JOG) return;

  LOG_I("Jog mode: %s", (dir == DIR_CW) ? "CW" : "CCW");

  // Set direction
  digitalWrite(PIN_DIR, (dir == DIR_CW) ? HIGH : LOW);

  // Enable motor at jog speed
  digitalWrite(PIN_ENA, LOW);

  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    uint32_t hz = (uint32_t)rpmToStepHz(jogRPM);
    stepper->setSpeedInHz(hz);
    if (dir == DIR_CW) {
      stepper->runForward();
    } else {
      stepper->runBackward();
    }
  }

  control_transition_to(STATE_JOG);
}

// ───────────────────────────────────────────────────────────────────────────────
// JOG MODE STOP (immediate)
// ───────────────────────────────────────────────────────────────────────────────
void jog_stop() {
  if (control_get_state() != STATE_JOG) return;

  LOG_D("Jog stop");

  // Transition through STOPPING (motor_stop triggers deceleration)
  control_transition_to(STATE_STOPPING);
}

// ───────────────────────────────────────────────────────────────────────────────
// JOG SPEED SET
// ───────────────────────────────────────────────────────────────────────────────
void jog_set_speed(float rpm) {
  jogRPM = constrain(rpm, MIN_RPM, MAX_RPM);

  // Update speed if currently jogging
  if (control_get_state() == STATE_JOG) {
    FastAccelStepper* stepper = motor_get_stepper();
    if (stepper != nullptr) {
      uint32_t hz = (uint32_t)rpmToStepHz(jogRPM);
      stepper->setSpeedInHz(hz);
    }
  }
}

float jog_get_speed() {
  return jogRPM;
}
