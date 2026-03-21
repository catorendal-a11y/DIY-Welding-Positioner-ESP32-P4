// TIG Rotator Controller - Continuous Rotation Mode
// Motor runs continuously at speed set by pot or slider

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONTINUOUS MODE ENTRY
// ───────────────────────────────────────────────────────────────────────────────
void continuous_start() {
  if (control_get_state() != STATE_IDLE) return;

  LOG_I("Continuous mode: start");

  // Set direction and enable motor
  digitalWrite(PIN_ENA, LOW);
  digitalWrite(PIN_DIR, (speed_get_direction() == DIR_CW) ? HIGH : LOW);

  // Apply target speed
  speed_apply();

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
