// Pulse mode: production timing core shared with host tests and simulation.
#include "../control.h"
#include "../motion_policy.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include <atomic>

static PulseTimeline timeline;
static std::atomic<uint32_t> pulseOnMs{500}, pulseOffMs{500}, completedCycles{0};
static std::atomic<uint16_t> pulseCycleLimit{0};
static std::atomic<PulsePhase> publishedPhase{PulsePhase::Complete};

static void publish_pulse() {
  completedCycles.store(timeline.completed());
  publishedPhase.store(timeline.phase());
}
static bool start_motor() {
  const uint32_t rate = motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm());
  if (!rate) return false;
  motor_set_target_milli_hz(rate);
  return speed_get_direction() == DIR_CW ? motor_run_cw() : motor_run_ccw();
}
void pulse_start(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (control_get_state() != STATE_IDLE) return;
  pulseOnMs = constrain(on_ms, PULSE_MS_MIN, PULSE_MS_MAX);
  pulseOffMs = constrain(off_ms, PULSE_MS_MIN, PULSE_MS_MAX);
  pulseCycleLimit = cycles;
  timeline.start(millis(), pulseOnMs.load(), pulseOffMs.load(), cycles);
  if (!start_motor() || !control_transition_to(STATE_PULSE)) {
    timeline.cancel();
    motor_halt();
  }
  publish_pulse();
}
void pulse_update() {
  if (control_get_state() != STATE_PULSE) return;
  const bool moving = motor_is_running();
  if (control_get_state() != STATE_PULSE) return; // Query may latch a driver fault.
  const PulseAction action = timeline.update(millis(), moving);
  switch (action) {
    case PulseAction::Stop:
      control_expect_motion_completion(STATE_PULSE, motor_stop_timeout_ms());
      motor_stop(); break;
    case PulseAction::Start:
      if (!start_motor()) {
        timeline.cancel();
        control_transition_to(STATE_STOPPING);
      }
      break;
    case PulseAction::Complete: control_transition_to(STATE_STOPPING); break;
    default: break;
  }
  if (timeline.phase() != PulsePhase::Decelerating) control_clear_motion_deadline();
  publish_pulse();
}
void pulse_stop() {
  timeline.cancel();
  publish_pulse();
  if (control_get_state() == STATE_PULSE) control_transition_to(STATE_STOPPING);
}
uint32_t pulse_get_on_ms() { return pulseOnMs.load(); }
uint32_t pulse_get_off_ms() { return pulseOffMs.load(); }
bool pulse_is_on_phase() { return publishedPhase.load() == PulsePhase::On; }
uint32_t pulse_get_cycle_count() { return completedCycles.load(); }
uint16_t pulse_get_cycle_limit() { return pulseCycleLimit.load(); }
