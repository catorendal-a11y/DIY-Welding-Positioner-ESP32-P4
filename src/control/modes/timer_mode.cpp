// TIG Rotator Controller - Timer Mode
// Continuous rotation for set duration, then auto-stop

#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// TIMER MODE STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t timerDurationSec = 30;   // Default 30 seconds
static uint32_t timerStartMs = 0;
static bool timerRunning = false;

// ───────────────────────────────────────────────────────────────────────────────
// TIMER MODE START
// ───────────────────────────────────────────────────────────────────────────────
void timer_start(uint32_t duration_sec) {
  if (control_get_state() != STATE_IDLE) return;

  timerDurationSec = duration_sec;
  timerStartMs = millis();
  timerRunning = true;

  LOG_I("Timer mode: %lu seconds", timerDurationSec);

  Direction dir = speed_get_direction();
  if (dir == DIR_CW) {
    motor_run_cw();
  } else {
    motor_run_ccw();
  }

  control_transition_to(STATE_TIMER);
}

// ───────────────────────────────────────────────────────────────────────────────
// TIMER MODE UPDATE (called from controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void timer_update() {
  if (control_get_state() != STATE_TIMER || !timerRunning) return;

  uint32_t elapsedSec = (millis() - timerStartMs) / 1000;

  if (elapsedSec >= timerDurationSec) {
    // Timer complete
    LOG_I("Timer complete: %lu seconds", timerDurationSec);
    timerRunning = false;
    control_transition_to(STATE_STOPPING);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// TIMER MODE STOP (manual)
// ───────────────────────────────────────────────────────────────────────────────
void timer_stop() {
  if (control_get_state() != STATE_TIMER) return;

  LOG_I("Timer mode: manual stop");
  timerRunning = false;
  control_transition_to(STATE_STOPPING);
}

// ───────────────────────────────────────────────────────────────────────────────
// TIMER MODE GETTERS
// ───────────────────────────────────────────────────────────────────────────────
uint32_t timer_get_duration() {
  return timerDurationSec;
}

uint32_t timer_get_remaining_sec() {
  if (!timerRunning || control_get_state() != STATE_TIMER) {
    return 0;
  }

  uint32_t elapsedSec = (millis() - timerStartMs) / 1000;
  int32_t remaining = (int32_t)timerDurationSec - (int32_t)elapsedSec;
  return (remaining > 0) ? (uint32_t)remaining : 0;
}
