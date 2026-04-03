// TIG Rotator Controller - Pulse Mode
// Rotate for ON ms, pause for OFF ms, repeat

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <FastAccelStepper.h>

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t pulseOnMs = 500;
static uint32_t pulseOffMs = 500;
static uint32_t pulseStateStartMs = 0;
static bool pulseIsOn = false;

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE ENTRY
// ───────────────────────────────────────────────────────────────────────────────
void pulse_start(uint32_t on_ms, uint32_t off_ms) {
  if (control_get_state() != STATE_IDLE) return;

  pulseOnMs = constrain(on_ms, 100, 5000);
  pulseOffMs = constrain(off_ms, 100, 5000);
  pulseIsOn = true;
  pulseStateStartMs = millis();

  LOG_I("Pulse mode: ON=%lu OFF=%lu", pulseOnMs, pulseOffMs);

  portENTER_CRITICAL(&g_stepperMutex);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    uint32_t hz = (uint32_t)rpmToStepHz(speed_get_target_rpm());
    stepper->setSpeedInHz(hz);
  }
  portEXIT_CRITICAL(&g_stepperMutex);

  if (speed_get_direction() == DIR_CW) motor_run_cw();
  else motor_run_ccw();

  control_transition_to(STATE_PULSE);
}

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void pulse_update() {
  if (control_get_state() != STATE_PULSE) return;

  uint32_t elapsed = millis() - pulseStateStartMs;
  uint32_t currentDuration = pulseIsOn ? pulseOnMs : pulseOffMs;

  if (elapsed >= currentDuration) {
    // Toggle state
    pulseIsOn = !pulseIsOn;
    pulseStateStartMs = millis();

    if (pulseIsOn) {
      portENTER_CRITICAL(&g_stepperMutex);
      FastAccelStepper* s = motor_get_stepper();
      if (s != nullptr) {
        uint32_t hz = (uint32_t)rpmToStepHz(speed_get_target_rpm());
        s->setSpeedInHz(hz);
      }
      portEXIT_CRITICAL(&g_stepperMutex);
      if (speed_get_direction() == DIR_CW) motor_run_cw();
      else motor_run_ccw();
      LOG_D("Pulse: ON");
    } else {
      portENTER_CRITICAL(&g_stepperMutex);
      FastAccelStepper* stepper = motor_get_stepper();
      if (stepper != nullptr) stepper->stopMove();
      portEXIT_CRITICAL(&g_stepperMutex);
      LOG_D("Pulse: OFF");
    }
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE STOP
// ───────────────────────────────────────────────────────────────────────────────
void pulse_stop() {
  if (control_get_state() != STATE_PULSE) return;

  LOG_I("Pulse mode: stop");
  control_transition_to(STATE_STOPPING);
}

// ───────────────────────────────────────────────────────────────────────────────
// PULSE PARAMETER GETTERS
// ───────────────────────────────────────────────────────────────────────────────
uint32_t pulse_get_on_ms() { return pulseOnMs; }
uint32_t pulse_get_off_ms() { return pulseOffMs; }
bool pulse_is_on_phase() { return pulseIsOn; }
