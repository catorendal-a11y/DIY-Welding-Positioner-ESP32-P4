// TIG Rotator Controller - Pulse Mode
// Rotate for ON ms, pause for OFF ms, repeat

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t pulseOnMs = 500;
static uint32_t pulseOffMs = 500;
static uint32_t pulseStateStartMs = 0;
static bool pulseIsOn = false;
static uint16_t pulseCycleLimit = 0;
static uint16_t pulseCycleCount = 0;

// ───────────────────────────────────────────────────────────────────────────────
// PULSE MODE ENTRY
// ───────────────────────────────────────────────────────────────────────────────
void pulse_start(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (control_get_state() != STATE_IDLE) return;

  pulseOnMs = constrain(on_ms, 100, 5000);
  pulseOffMs = constrain(off_ms, 100, 5000);
  pulseCycleLimit = cycles;
  pulseCycleCount = 0;
  pulseIsOn = true;
  pulseStateStartMs = millis();

  LOG_I("Pulse mode: ON=%lu OFF=%lu cycles=%u", pulseOnMs, pulseOffMs, cycles);

  motor_set_target_milli_hz(motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm()));

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
      pulseCycleCount++;
      if (pulseCycleLimit > 0 && pulseCycleCount >= pulseCycleLimit) {
        LOG_I("Pulse: cycle limit reached (%u/%u)", pulseCycleCount, pulseCycleLimit);
        control_transition_to(STATE_STOPPING);
        return;
      }
      motor_set_target_milli_hz(motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm()));
      if (speed_get_direction() == DIR_CW) motor_run_cw();
      else motor_run_ccw();
      LOG_D("Pulse: ON (%u/%u)", pulseCycleCount, pulseCycleLimit);
    } else {
      motor_stop();
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
uint16_t pulse_get_cycle_count() { return pulseCycleCount; }
uint16_t pulse_get_cycle_limit() { return pulseCycleLimit; }
