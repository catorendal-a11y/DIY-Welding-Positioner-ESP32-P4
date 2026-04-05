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
static std::atomic<uint8_t> pendingModeRequest{0};
static std::atomic<bool> pendingStop{false};
static std::atomic<uint32_t> pendingPulseOnMs{0};
static std::atomic<uint32_t> pendingPulseOffMs{0};
static std::atomic<uint16_t> pendingPulseCycles{0};
static std::atomic<float> pendingStepAngle{0.0f};
static std::atomic<bool> pendingStopJog{false};

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
  currentState.store(STATE_IDLE, std::memory_order_relaxed);
  previousState.store(STATE_IDLE, std::memory_order_relaxed);
  LOG_I("Control init: state=IDLE");
}

void control_transition_to(SystemState newState) {
  SystemState expected = currentState.load(std::memory_order_relaxed);
  if (!control_is_valid_transition(expected, newState)) {
    LOG_W("Invalid transition: %s -> %s",
          control_state_name(expected),
          control_state_name(newState));
    return;
  }
  if (!currentState.compare_exchange_strong(expected, newState)) {
    LOG_W("Race in transition: %s -> %s (state changed to %s)", control_state_name(expected), control_state_name(newState), control_state_name(currentState.load(std::memory_order_relaxed)));
    return;
  }

  previousState.store(expected, std::memory_order_relaxed);

  LOG_I("State: %s -> %s",
        control_state_name(previousState),
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
  return currentState.load(std::memory_order_relaxed);
}

const char* control_get_state_string() {
  return control_state_name(currentState.load(std::memory_order_relaxed));
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
  if (safety_is_estop_active()) return;
  pendingModeRequest.store(1, std::memory_order_relaxed);
}

void control_stop() {
  pendingStop.store(true, std::memory_order_relaxed);
}

void control_start_pulse(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (safety_is_estop_active()) return;
  pendingPulseOnMs.store(on_ms, std::memory_order_release);
  pendingPulseOffMs.store(off_ms, std::memory_order_release);
  pendingPulseCycles.store(cycles, std::memory_order_release);
  pendingModeRequest.store(2, std::memory_order_relaxed);
}

void control_start_step(float angle_deg) {
  if (safety_is_estop_active()) return;
  pendingStepAngle.store(angle_deg, std::memory_order_release);
  pendingModeRequest.store(3, std::memory_order_relaxed);
}

void control_start_jog_cw() {
  if (safety_is_estop_active()) return;
  pendingModeRequest.store(4, std::memory_order_relaxed);
}

void control_start_jog_ccw() {
  if (safety_is_estop_active()) return;
  pendingModeRequest.store(5, std::memory_order_relaxed);
}

void control_stop_jog() {
  pendingStopJog.store(true, std::memory_order_relaxed);
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
static void process_pending_requests() {
  SystemState cur = currentState.load(std::memory_order_relaxed);

  if (pendingStopJog.load(std::memory_order_relaxed)) {
    pendingStopJog.store(false, std::memory_order_relaxed);
    if (cur == STATE_JOG) {
      jog_stop();
    }
  }

  if (pendingStop.load(std::memory_order_relaxed)) {
    pendingStop.store(false, std::memory_order_relaxed);
    pendingModeRequest.store(0, std::memory_order_relaxed);
    if (cur != STATE_IDLE && cur != STATE_STOPPING && cur != STATE_ESTOP) {
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
    return;
  }

  if (pendingModeRequest.load(std::memory_order_relaxed) == 0) return;

  if (cur != STATE_IDLE) {
    if (cur != STATE_STOPPING && cur != STATE_ESTOP) {
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
    return;
  }

  uint8_t req = pendingModeRequest.load(std::memory_order_relaxed);
  pendingModeRequest.store(0, std::memory_order_relaxed);

  switch (req) {
    case 1:
      continuous_start();
      break;
    case 2:
      pulse_start(pendingPulseOnMs.load(std::memory_order_acquire),
                  pendingPulseOffMs.load(std::memory_order_acquire),
                  pendingPulseCycles.load(std::memory_order_acquire));
      break;
    case 3:
      step_execute(pendingStepAngle.load(std::memory_order_acquire));
      break;
    case 4:
      jog_start(DIR_CW);
      break;
    case 5:
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

    if (currentState.load(std::memory_order_relaxed) == STATE_ESTOP) {
      if (safety_check_ui_reset()) {
        control_transition_to(STATE_IDLE);
      }
    }

    if (currentState.load(std::memory_order_relaxed) == STATE_STOPPING) {
      if (!motor_is_running()) {
        motor_disable();
        control_transition_to(STATE_IDLE);
      }
    }

    switch (currentState.load(std::memory_order_relaxed)) {
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
