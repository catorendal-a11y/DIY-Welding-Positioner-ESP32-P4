// Jog Mode - Touch-and-hold jog control with configurable speed
#include "../control.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <atomic>
#include <cmath>

static_assert(std::atomic<float>::is_always_lock_free,
              "std::atomic<float> must be lock-free for inter-core jog speed sharing");

static std::atomic<float> jogRPM{0.5f};
static std::atomic<float> pendingJogSpeed{-1.0f};

void jog_start(Direction dir) {
  SystemState state = control_get_state();
  // ENABLING = replay of an already-admitted start after the driver settle.
  if (state != STATE_IDLE && state != STATE_JOG && state != STATE_ENABLING) return;

  LOG_I("Jog mode: %s", (dir == DIR_CW) ? "CW" : "CCW");

  const float effective = speed_clamp_rpm(jogRPM.load(std::memory_order_relaxed));
  jogRPM.store(effective);
  const uint32_t rate = motor_milli_hz_for_rpm_calibrated(effective);
  if (!rate) return;
  motor_set_target_milli_hz(rate);

  bool started = (dir == DIR_CW) ? motor_run_cw() : motor_run_ccw();
  if (!started) {
    LOG_W("Jog mode: start blocked");
    return;
  }

  if (!control_transition_to(STATE_JOG)) {
    motor_halt();
  }
}

void jog_stop() {
  if (control_get_state() != STATE_JOG) return;

  LOG_D("Jog stop");
  control_transition_to(STATE_STOPPING);
}

void jog_set_speed(float rpm) {
  if (!std::isfinite(rpm)) return;
  float constrained = speed_clamp_rpm(rpm);
  jogRPM.store(constrained, std::memory_order_relaxed);
  pendingJogSpeed.store(constrained, std::memory_order_release);
}

void jog_update() {
  float pending = pendingJogSpeed.load(std::memory_order_acquire);
  if (pending >= 0.0f && control_get_state() == STATE_JOG) {
    pendingJogSpeed.store(-1.0f, std::memory_order_relaxed);
    const float effective = speed_clamp_rpm(jogRPM.load(std::memory_order_relaxed));
    jogRPM.store(effective);
    const uint32_t rate = motor_milli_hz_for_rpm_calibrated(effective);
    if (!rate) { jog_stop(); return; }
    motor_set_target_milli_hz(rate);
  }
}

float jog_get_speed() {
  return speed_clamp_rpm(jogRPM.load(std::memory_order_relaxed));
}
