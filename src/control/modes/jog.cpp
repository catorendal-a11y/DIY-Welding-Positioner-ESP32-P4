// Jog Mode - Touch-and-hold jog control with configurable speed
#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <atomic>

static std::atomic<float> jogRPM{0.5f};
static std::atomic<float> pendingJogSpeed{-1.0f};

void jog_start(Direction dir) {
  SystemState state = control_get_state();
  if (state != STATE_IDLE && state != STATE_JOG) return;

  LOG_I("Jog mode: %s", (dir == DIR_CW) ? "CW" : "CCW");

  portENTER_CRITICAL(&g_stepperMutex);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    uint32_t hz = (uint32_t)rpmToStepHz(jogRPM.load(std::memory_order_relaxed));
    stepper->setSpeedInHz(hz);
  }
  portEXIT_CRITICAL(&g_stepperMutex);

  if (dir == DIR_CW) motor_run_cw();
  else motor_run_ccw();

  control_transition_to(STATE_JOG);
}

void jog_stop() {
  if (control_get_state() != STATE_JOG) return;

  LOG_D("Jog stop");
  control_transition_to(STATE_STOPPING);
}

void jog_set_speed(float rpm) {
  float constrained = constrain(rpm, MIN_RPM, MAX_RPM);
  jogRPM.store(constrained, std::memory_order_relaxed);
  pendingJogSpeed.store(constrained, std::memory_order_release);
}

void jog_update() {
  float pending = pendingJogSpeed.load(std::memory_order_acquire);
  if (pending >= 0.0f && control_get_state() == STATE_JOG) {
    pendingJogSpeed.store(-1.0f, std::memory_order_relaxed);
    portENTER_CRITICAL(&g_stepperMutex);
    FastAccelStepper* stepper = motor_get_stepper();
    if (stepper != nullptr) {
      uint32_t hz = (uint32_t)rpmToStepHz(jogRPM.load(std::memory_order_relaxed));
      stepper->setSpeedInHz(hz);
    }
    portEXIT_CRITICAL(&g_stepperMutex);
  }
}

float jog_get_speed() {
  return jogRPM.load(std::memory_order_relaxed);
}
