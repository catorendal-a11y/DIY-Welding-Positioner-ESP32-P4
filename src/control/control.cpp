// Control - State machine core with pending request pattern
#include "control.h"
#include "modes.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include "esp_task_wdt.h"
#include <atomic>

static_assert(std::atomic<float>::is_always_lock_free,
              "std::atomic<float> must be lock-free for inter-core state sharing");

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static std::atomic<SystemState> currentState{STATE_IDLE};
static std::atomic<SystemState> previousState{STATE_IDLE};

// Pending request pattern — UI callbacks set flags, controlTask executes
enum ModeRequest : uint8_t {
  MODE_REQ_NONE = 0,
  MODE_REQ_CONTINUOUS = 1,
  MODE_REQ_PULSE = 2,
  MODE_REQ_STEP = 3,
  MODE_REQ_JOG_CW = 4,
  MODE_REQ_JOG_CCW = 5
};

static std::atomic<uint8_t> pendingModeRequest{MODE_REQ_NONE};
static std::atomic<bool> pendingStop{false};
static std::atomic<uint32_t> pendingPulseOnMs{0};
static std::atomic<uint32_t> pendingPulseOffMs{0};
static std::atomic<uint16_t> pendingPulseCycles{0};
static std::atomic<float> pendingStepAngle{0.0f};
static std::atomic<bool> pendingStopJog{false};

static bool is_jog_request(uint8_t req) {
  return req == MODE_REQ_JOG_CW || req == MODE_REQ_JOG_CCW;
}

static void cancel_pending_jog_request() {
  uint8_t req = pendingModeRequest.load(std::memory_order_acquire);
  if (is_jog_request(req)) {
    pendingModeRequest.compare_exchange_strong(req, MODE_REQ_NONE, std::memory_order_acq_rel,
                                               std::memory_order_acquire);
  }
}

static void queue_mode_request(uint8_t req) {
  pendingStop.store(false, std::memory_order_release);
  pendingModeRequest.store(req, std::memory_order_release);
}

// ───────────────────────────────────────────────────────────────────────────────
// STATE TRANSITION VALIDATION
// ───────────────────────────────────────────────────────────────────────────────
bool control_is_valid_transition(SystemState from, SystemState to) {
  if (to == STATE_ESTOP) return true;
  if (from == STATE_ESTOP) return (to == STATE_IDLE);
  if (from == STATE_IDLE) {
    switch (to) {
      case STATE_RUNNING:
      case STATE_PULSE:
      case STATE_STEP:
      case STATE_JOG:
      case STATE_ESTOP:
        return true;
      default:
        return false;
    }
  }
  if (from == STATE_RUNNING) {
    return (to == STATE_STOPPING || to == STATE_ESTOP);
  }
  if (from == STATE_PULSE || from == STATE_STEP || from == STATE_JOG) {
    return (to == STATE_STOPPING || to == STATE_ESTOP);
  }
  if (from == STATE_STOPPING) {
    return (to == STATE_IDLE || to == STATE_ESTOP);
  }
  return false;
}

// ───────────────────────────────────────────────────────────────────────────────
// STATE MACHINE CORE
// ───────────────────────────────────────────────────────────────────────────────
void control_init() {
  currentState.store(STATE_IDLE, std::memory_order_release);
  previousState.store(STATE_IDLE, std::memory_order_release);
  LOG_I("Control init: state=IDLE");
}

void control_transition_to(SystemState newState) {
  SystemState expected = currentState.load(std::memory_order_acquire);
  if (!control_is_valid_transition(expected, newState)) {
    LOG_W("Invalid transition: %s -> %s",
          control_state_name(expected),
          control_state_name(newState));
    return;
  }
  if (!currentState.compare_exchange_strong(expected, newState, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
    LOG_W("Race in transition: %s -> %s (state changed to %s)", control_state_name(expected),
          control_state_name(newState),
          control_state_name(currentState.load(std::memory_order_acquire)));
    return;
  }

  previousState.store(expected, std::memory_order_release);

  LOG_I("State: %s -> %s",
        control_state_name(expected),
        control_state_name(newState));

  switch (newState) {
    case STATE_IDLE:
      break;
    case STATE_RUNNING:
      break;
    case STATE_STOPPING:
      motor_stop();
      break;
    case STATE_ESTOP:
      motor_halt();
      break;
    default:
      break;
  }
}

SystemState control_get_state() {
  return currentState.load(std::memory_order_acquire);
}

const char* control_get_state_string() {
  return control_state_name(currentState.load(std::memory_order_acquire));
}

const char* control_state_name(SystemState s) {
  switch (s) {
    case STATE_IDLE:     return "IDLE";
    case STATE_RUNNING:  return "RUNNING";
    case STATE_PULSE:    return "PULSE";
    case STATE_STEP:     return "STEP";
    case STATE_JOG:      return "JOG";
    case STATE_STOPPING: return "STOPPING";
    case STATE_ESTOP:    return "ESTOP";
    default:             return "UNKNOWN";
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// MODE CONTROL FUNCTIONS (non-blocking — set flags for controlTask)
// ───────────────────────────────────────────────────────────────────────────────
void control_start_continuous() {
  if (safety_inhibit_motion()) return;
  queue_mode_request(MODE_REQ_CONTINUOUS);
}

void control_stop() {
  pendingStop.store(true, std::memory_order_release);
}

void control_start_pulse(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (safety_inhibit_motion()) return;
  pendingPulseOnMs.store(on_ms, std::memory_order_release);
  pendingPulseOffMs.store(off_ms, std::memory_order_release);
  pendingPulseCycles.store(cycles, std::memory_order_release);
  queue_mode_request(MODE_REQ_PULSE);
}

void control_start_step(float angle_deg) {
  if (safety_inhibit_motion()) return;
  pendingStepAngle.store(angle_deg, std::memory_order_release);
  queue_mode_request(MODE_REQ_STEP);
}

void control_start_jog_cw() {
  if (safety_inhibit_motion()) return;
  queue_mode_request(MODE_REQ_JOG_CW);
}

void control_start_jog_ccw() {
  if (safety_inhibit_motion()) return;
  queue_mode_request(MODE_REQ_JOG_CCW);
}

void control_stop_jog() {
  cancel_pending_jog_request();
  pendingStopJog.store(true, std::memory_order_release);
}

// ───────────────────────────────────────────────────────────────────────────────
// MODE-SPECIFIC GETTERS (for UI)
// ───────────────────────────────────────────────────────────────────────────────
float control_get_step_accumulated() {
  return step_get_accumulated();
}

long control_get_step_count() {
  return step_get_count();
}

float control_get_jog_speed() {
  return jog_get_speed();
}

void control_set_jog_speed(float rpm) {
  jog_set_speed(rpm);
}

void control_reset_step_accumulator() {
  step_reset_accumulator();
}

// ───────────────────────────────────────────────────────────────────────────────
// PENDING REQUEST PROCESSOR (runs in controlTask on Core 0)
// ───────────────────────────────────────────────────────────────────────────────
static void stop_active_mode(SystemState cur) {
  if (cur == STATE_RUNNING) {
    continuous_stop();
  } else if (cur == STATE_PULSE) {
    pulse_stop();
  } else if (cur == STATE_JOG) {
    jog_stop();
  } else if (cur == STATE_STEP) {
    control_transition_to(STATE_STOPPING);
  }
}

static void process_pending_requests() {
  SystemState cur = currentState.load(std::memory_order_acquire);

  if (pendingStopJog.exchange(false, std::memory_order_acq_rel)) {
    if (cur == STATE_JOG) {
      jog_stop();
    } else {
      cancel_pending_jog_request();
    }
  }

  if (pendingStop.exchange(false, std::memory_order_acq_rel)) {
    pendingModeRequest.store(MODE_REQ_NONE, std::memory_order_release);
    if (cur != STATE_IDLE && cur != STATE_STOPPING && cur != STATE_ESTOP) {
      stop_active_mode(cur);
    }
    return;
  }

  if (pendingModeRequest.load(std::memory_order_acquire) == MODE_REQ_NONE) return;

  if (cur != STATE_IDLE) {
    if (cur != STATE_STOPPING && cur != STATE_ESTOP) {
      stop_active_mode(cur);
    }
    return;
  }

  uint8_t req = pendingModeRequest.exchange(MODE_REQ_NONE, std::memory_order_acq_rel);

  switch (req) {
    case MODE_REQ_CONTINUOUS:
      continuous_start();
      break;
    case MODE_REQ_PULSE:
      pulse_start(pendingPulseOnMs.load(std::memory_order_acquire),
                  pendingPulseOffMs.load(std::memory_order_acquire),
                  pendingPulseCycles.load(std::memory_order_acquire));
      break;
    case MODE_REQ_STEP:
      step_execute(pendingStepAngle.load(std::memory_order_acquire));
      break;
    case MODE_REQ_JOG_CW:
      jog_start(DIR_CW);
      break;
    case MODE_REQ_JOG_CCW:
      jog_start(DIR_CCW);
      break;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTROL TASK — Main state machine loop
// ───────────────────────────────────────────────────────────────────────────────
void controlTask(void* pvParameters) {
  LOG_I("Control task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    esp_task_wdt_reset();

    process_pending_requests();

    if (currentState.load(std::memory_order_acquire) == STATE_ESTOP) {
      if (safety_check_ui_reset()) {
        control_transition_to(STATE_IDLE);
      }
    }

    if (currentState.load(std::memory_order_acquire) == STATE_STOPPING) {
      if (!motor_is_running()) {
        motor_disable();
        control_transition_to(STATE_IDLE);
      }
    }

    switch (currentState.load(std::memory_order_acquire)) {
      case STATE_RUNNING:
        continuous_update();
        break;
      case STATE_PULSE:
        pulse_update();
        break;
      case STATE_STEP:
        step_update();
        break;
      case STATE_JOG:
        jog_update();
        break;
      default:
        break;
    }

    vTaskDelayUntil(&t, pdMS_TO_TICKS(10));
  }
}
