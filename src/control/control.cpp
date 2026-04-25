// Control - State machine core with motion command mailbox
#include "control.h"
#include "modes.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static std::atomic<SystemState> currentState{STATE_IDLE};
static std::atomic<SystemState> previousState{STATE_IDLE};

// UI/Core 1 producers post one command; controlTask/Core 0 executes it.
enum MotionCommandType : uint8_t {
  MOTION_CMD_NONE = 0,
  MOTION_CMD_START_CONTINUOUS,
  MOTION_CMD_START_PULSE,
  MOTION_CMD_START_STEP,
  MOTION_CMD_START_JOG,
  MOTION_CMD_STOP,
  MOTION_CMD_STOP_JOG
};

struct MotionCommand {
  MotionCommandType type;
  Direction direction;
  bool continuous_soft_start;
  uint32_t continuous_auto_stop_ms;
  uint32_t pulse_on_ms;
  uint32_t pulse_off_ms;
  uint16_t pulse_cycles;
  float step_angle;
  uint16_t step_repeats;
  float step_dwell_sec;
};

static QueueHandle_t controlQueue = nullptr;

static bool is_start_command(MotionCommandType type) {
  return type == MOTION_CMD_START_CONTINUOUS || type == MOTION_CMD_START_PULSE ||
         type == MOTION_CMD_START_STEP || type == MOTION_CMD_START_JOG;
}

static bool is_active_motion_state(SystemState state) {
  return state == STATE_RUNNING || state == STATE_PULSE || state == STATE_STEP ||
         state == STATE_JOG;
}

static void control_queue_init() {
  if (controlQueue == nullptr) {
    controlQueue = xQueueCreate(1, sizeof(MotionCommand));
  }
  if (controlQueue != nullptr) {
    xQueueReset(controlQueue);
  } else {
    LOG_E("Control queue allocation failed");
  }
}

static bool queue_motion_command(const MotionCommand& cmd) {
  if (controlQueue == nullptr) {
    LOG_W("Control queue not ready");
    return false;
  }
  return xQueueOverwrite(controlQueue, &cmd) == pdPASS;
}

static void cancel_pending_jog_request() {
  if (controlQueue == nullptr) return;
  MotionCommand cmd{};
  if (xQueuePeek(controlQueue, &cmd, 0) == pdTRUE && cmd.type == MOTION_CMD_START_JOG) {
    xQueueReset(controlQueue);
  }
}

static void clear_pending_motion_requests() {
  if (controlQueue != nullptr) {
    xQueueReset(controlQueue);
  }
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
  control_queue_init();
  clear_pending_motion_requests();
  currentState.store(STATE_IDLE, std::memory_order_release);
  previousState.store(STATE_IDLE, std::memory_order_release);
  LOG_I("Control init: state=IDLE");
}

bool control_transition_to(SystemState newState) {
  SystemState expected = currentState.load(std::memory_order_acquire);
  if (!control_is_valid_transition(expected, newState)) {
    LOG_W("Invalid transition: %s -> %s",
          control_state_name(expected),
          control_state_name(newState));
    return false;
  }
  if (!currentState.compare_exchange_strong(expected, newState, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
    LOG_W("Race in transition: %s -> %s (state changed to %s)", control_state_name(expected),
          control_state_name(newState),
          control_state_name(currentState.load(std::memory_order_acquire)));
    return false;
  }

  previousState.store(expected, std::memory_order_release);

  LOG_I("State: %s -> %s",
        control_state_name(expected),
        control_state_name(newState));

  switch (newState) {
    case STATE_IDLE:
      speed_clear_program_direction_override();
      motor_restore_configured_acceleration();
      break;
    case STATE_RUNNING:
      break;
    case STATE_STOPPING:
      motor_stop();
      break;
    case STATE_ESTOP:
      clear_pending_motion_requests();
      speed_clear_program_direction_override();
      motor_restore_configured_acceleration();
      motor_halt();
      break;
    default:
      break;
  }

  return true;
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
bool control_start_continuous(bool soft_start, uint32_t auto_stop_ms) {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_CONTINUOUS;
  cmd.continuous_soft_start = soft_start;
  cmd.continuous_auto_stop_ms = auto_stop_ms;
  return queue_motion_command(cmd);
}

bool control_stop() {
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_STOP;
  return queue_motion_command(cmd);
}

bool control_start_pulse(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_PULSE;
  cmd.pulse_on_ms = on_ms;
  cmd.pulse_off_ms = off_ms;
  cmd.pulse_cycles = cycles;
  return queue_motion_command(cmd);
}

bool control_start_step(float angle_deg) {
  return control_start_step_sequence(angle_deg, 1, 0.0f);
}

bool control_start_step_sequence(float angle_deg, uint16_t repeats, float dwell_sec) {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_STEP;
  cmd.step_angle = angle_deg;
  cmd.step_repeats = repeats;
  cmd.step_dwell_sec = dwell_sec;
  return queue_motion_command(cmd);
}

bool control_start_jog_cw() {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_JOG;
  cmd.direction = DIR_CW;
  return queue_motion_command(cmd);
}

bool control_start_jog_ccw() {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_JOG;
  cmd.direction = DIR_CCW;
  return queue_motion_command(cmd);
}

bool control_stop_jog() {
  cancel_pending_jog_request();
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_STOP_JOG;
  return queue_motion_command(cmd);
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
// MOTION COMMAND PROCESSOR (runs in controlTask on Core 0)
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

  if (cur == STATE_ESTOP) {
    clear_pending_motion_requests();
    return;
  }

  if (controlQueue == nullptr) return;

  MotionCommand cmd{};
  if (xQueuePeek(controlQueue, &cmd, 0) != pdTRUE) return;

  if (cmd.type == MOTION_CMD_STOP) {
    xQueueReceive(controlQueue, &cmd, 0);
    if (is_active_motion_state(cur)) {
      stop_active_mode(cur);
    }
    return;
  }

  if (cmd.type == MOTION_CMD_STOP_JOG) {
    xQueueReceive(controlQueue, &cmd, 0);
    if (cur == STATE_JOG) {
      jog_stop();
    }
    return;
  }

  if (!is_start_command(cmd.type)) {
    xQueueReceive(controlQueue, &cmd, 0);
    return;
  }

  if (cur != STATE_IDLE) {
    if (is_active_motion_state(cur)) {
      stop_active_mode(cur);
    }
    return;
  }

  xQueueReceive(controlQueue, &cmd, 0);

  switch (cmd.type) {
    case MOTION_CMD_START_CONTINUOUS:
      continuous_start(cmd.continuous_soft_start, cmd.continuous_auto_stop_ms);
      break;
    case MOTION_CMD_START_PULSE:
      pulse_start(cmd.pulse_on_ms, cmd.pulse_off_ms, cmd.pulse_cycles);
      break;
    case MOTION_CMD_START_STEP:
      step_execute_sequence(cmd.step_angle, cmd.step_repeats, cmd.step_dwell_sec);
      break;
    case MOTION_CMD_START_JOG:
      jog_start(cmd.direction);
      break;
    default:
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

    SystemState curState = currentState.load(std::memory_order_acquire);

    if (curState == STATE_ESTOP) {
      if (safety_check_ui_reset()) {
        control_transition_to(STATE_IDLE);
      }
    }

    if (curState == STATE_STOPPING) {
      if (!motor_is_running()) {
        motor_disable();
        control_transition_to(STATE_IDLE);
      }
    }

    switch (curState) {
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
