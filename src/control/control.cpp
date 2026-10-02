// Control - State machine core with motion command mailbox
#include "control.h"
#include "../app_state.h"
#include "modes.h"
#include "input_policy.h"
#include "../storage/storage.h"
#include "../config.h"
#include "../event_log.h"
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
  MOTION_CMD_CONFIG,
  MOTION_CMD_START_CONTINUOUS,
  MOTION_CMD_START_PULSE,
  MOTION_CMD_START_STEP,
  MOTION_CMD_START_JOG
};

struct MotionCommand {
  MotionCommandType type;
  uint32_t ticket;
  bool program;
  Preset preset;
  SystemSettings settings;
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
static MotionGate motionGate;
static std::atomic<ConfigApplyStatus> configStatus{CONFIG_NONE};
ConfigApplyStatus control_config_status() { return configStatus.load(); }
static std::atomic<bool> resetStepPending{false};
static std::atomic<uint32_t> jogRenewedMs{0};
static std::atomic<uint32_t> stopRequestedAt{0}, stoppingAt{0}, stoppingBudget{0};
void control_check_stop_deadline(uint32_t now) {
  const uint32_t requested = stopRequestedAt.load();
  const uint32_t stopping = stoppingAt.load();
  if ((requested && now - (requested - 1u) >= 50u) ||
      (stopping && control_get_state() == STATE_STOPPING &&
       now - (stopping - 1u) >= stoppingBudget.load())) {
    safety_report_motor_fault(FAULT_MOTOR_TIMEOUT);
    stopRequestedAt.store(0);
    stoppingAt.store(0);
  }
}

bool control_motion_blocked() { return motionGate.blocked(); }
void control_renew_jog() { jogRenewedMs.store(millis()); }

static bool is_active_motion_state(SystemState state) {
  return state == STATE_RUNNING || state == STATE_PULSE || state == STATE_STEP || state == STATE_JOG;
}

static void control_queue_init() {
  if (controlQueue == nullptr) {
    controlQueue = xQueueCreate(1, sizeof(MotionCommand));
  }
  if (controlQueue != nullptr) {
    xQueueReset(controlQueue);
  } else {
    fatal_halt("Control queue allocation failed");
  }
}

static bool queue_motion_command(const MotionCommand& cmd) {
  if (controlQueue == nullptr) {
    LOG_W("Control queue not ready");
    return false;
  }
  const uint32_t ticket = motionGate.ticket();
  if (motionGate.blocked() || control_get_state() != STATE_IDLE || safety_inhibit_motion()) return false;
  MotionCommand request = cmd;
  request.ticket = ticket;
  return xQueueSend(controlQueue, &request, 0) == pdPASS;
}

static const char* motion_command_name(MotionCommandType type) {
  switch (type) {
    case MOTION_CMD_START_CONTINUOUS:
      return "START CONT";
    case MOTION_CMD_START_PULSE:
      return "START PULSE";
    case MOTION_CMD_START_STEP:
      return "START STEP";
    case MOTION_CMD_START_JOG:
      return "START JOG";
    default:
      return "UNKNOWN";
  }
}

static void clear_pending_motion_requests() {
  if (configStatus.load() == CONFIG_PENDING) configStatus.store(CONFIG_CANCELLED);
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
  event_log_add("CONTROL INIT");
  LOG_I("Control init: state=IDLE");
}

bool control_transition_to(SystemState newState) {
  if (newState == STATE_ESTOP) {
    digitalWrite(PIN_ENA, HIGH);
    motionGate.stop();
    previousState.store(currentState.exchange(STATE_ESTOP));
    return true;
  }
  SystemState expected = currentState.load(std::memory_order_acquire);
  if (!control_is_valid_transition(expected, newState)) {
    LOG_W("Invalid transition: %s -> %s", control_state_name(expected), control_state_name(newState));
    return false;
  }
  if (!currentState.compare_exchange_strong(expected, newState, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
    LOG_W("Race in transition: %s -> %s (state changed to %s)", control_state_name(expected),
          control_state_name(newState), control_state_name(currentState.load(std::memory_order_acquire)));
    return false;
  }

  previousState.store(expected, std::memory_order_release);

  LOG_I("State: %s -> %s", control_state_name(expected), control_state_name(newState));
  event_log_addf("STATE %s>%s", control_state_name(expected), control_state_name(newState));

  switch (newState) {
    case STATE_IDLE:
      stoppingAt.store(0);
      speed_clear_program_direction_override();
      motor_restore_configured_acceleration();
      break;
    case STATE_RUNNING:
      break;
    case STATE_STOPPING:
      stoppingBudget.store(motor_stop_timeout_ms());
      stoppingAt.store(millis() | 1u); // Encodes an even millisecond + 1; never zero across wrap.
      motor_stop();
      break;
    case STATE_ESTOP:
      clear_pending_motion_requests();
      speed_clear_program_direction_override();
      // The safety task has already disabled ENA. Cleanup belongs to controlTask.
      digitalWrite(PIN_ENA, HIGH);
      break;
    default:
      break;
  }

  return true;
}

SystemState control_get_state() { return currentState.load(std::memory_order_acquire); }

const char* control_get_state_string() {
  return control_state_name(currentState.load(std::memory_order_acquire));
}

const char* control_state_name(SystemState s) {
  switch (s) {
    case STATE_IDLE:
      return "IDLE";
    case STATE_RUNNING:
      return "RUNNING";
    case STATE_PULSE:
      return "PULSE";
    case STATE_STEP:
      return "STEP";
    case STATE_JOG:
      return "JOG";
    case STATE_STOPPING:
      return "STOPPING";
    case STATE_ESTOP:
      return "ESTOP";
    default:
      return "UNKNOWN";
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
  uint32_t none = 0;
  stopRequestedAt.compare_exchange_strong(none, millis() | 1u);
  motionGate.stop();
  return true;
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

bool control_start_step(float angle_deg) { return control_start_step_sequence(angle_deg, 1, 0.0f); }

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
  control_renew_jog();
  return queue_motion_command(cmd);
}

bool control_start_jog_ccw() {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_JOG;
  cmd.direction = DIR_CCW;
  control_renew_jog();
  return queue_motion_command(cmd);
}

bool control_stop_jog() {
  // Cancel pending jog as well as active motion; a release must never be lost.
  return control_stop();
}

// ───────────────────────────────────────────────────────────────────────────────
// MODE-SPECIFIC GETTERS (for UI)
// ───────────────────────────────────────────────────────────────────────────────
float control_get_step_accumulated() { return step_get_accumulated(); }

long control_get_step_count() { return step_get_count(); }

float control_get_jog_speed() { return jog_get_speed(); }

void control_set_jog_speed(float rpm) { jog_set_speed(rpm); }

void control_reset_step_accumulator() { resetStepPending.store(true); }

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
    stopRequestedAt.store(0);
    clear_pending_motion_requests();
    return;
  }

  if (controlQueue == nullptr) return;

  if (motionGate.takeStop()) {
    clear_pending_motion_requests();
    if (is_active_motion_state(cur)) stop_active_mode(cur);
    stopRequestedAt.store(0);
    return;
  }
  if (cur == STATE_JOG && millis() - jogRenewedMs.load() > 150u) {
    control_stop();
    stop_active_mode(cur);
    return;
  }
  MotionCommand cmd{};
  if (xQueueReceive(controlQueue, &cmd, 0) != pdTRUE) return;
  if (!motionGate.valid(cmd.ticket) || cur != STATE_IDLE || safety_inhibit_motion()) {
    if (cmd.type == MOTION_CMD_CONFIG) configStatus.store(CONFIG_CANCELLED);
    return;
  }
  if (cmd.type == MOTION_CMD_CONFIG) {
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    g_settings.microstep = cmd.settings.microstep;
    g_settings.acceleration = cmd.settings.acceleration;
    g_settings.max_rpm = cmd.settings.max_rpm;
    g_settings.dir_switch_enabled = cmd.settings.dir_switch_enabled;
    g_settings.invert_direction = cmd.settings.invert_direction;
    xSemaphoreGive(g_settings_mutex);
    g_dir_switch_cache.store(cmd.settings.dir_switch_enabled);
    speed_sync_rpm_limits_from_settings();
    motor_apply_settings();
    if (safety_inhibit_motion()) { configStatus.store(CONFIG_CANCELLED); return; }
    storage_save_settings();
    configStatus.store(CONFIG_APPLIED);
    return;
  }
  if (cmd.program) {
    speed_set_program_direction_override(cmd.preset.direction == DIR_CCW ? DIR_CCW : DIR_CW);
    speed_set_workpiece_diameter_mm(cmd.preset.workpiece_diameter_mm);
    speed_slider_set(cmd.preset.rpm);
  }
  event_log_add(motion_command_name(cmd.type));

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
  if (cmd.program && (control_get_state() == STATE_IDLE || control_get_state() == STATE_ESTOP))
    speed_clear_program_direction_override();
}

// ───────────────────────────────────────────────────────────────────────────────
// CONTROL TASK — Main state machine loop
// ───────────────────────────────────────────────────────────────────────────────
void controlTask(void* pvParameters) {
  LOG_I("Control task started on Core %d", xPortGetCoreID());
  safety_register_watchdog();
  safety_task_ready(4u);
  bool faultCleaned = false;

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    safety_feed_watchdog();

    if (control_get_state() == STATE_IDLE && resetStepPending.exchange(false)) step_reset_accumulator();
    process_pending_requests();

    SystemState curState = currentState.load(std::memory_order_acquire);

    if (curState == STATE_ESTOP) {
      // Potentially blocking library cleanup never runs in safetyTask.
      if (!faultCleaned) {
        if (!motor_halt()) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
        motor_restore_configured_acceleration();
        speed_clear_program_direction_override();
        event_log_addf("FAULT %s", safety_fault_reason_name(safety_get_fault_reason()));
        faultCleaned = true;
      }
      if (safety_check_ui_reset()) {
        control_transition_to(STATE_IDLE);
      }
    }

    if (curState != STATE_ESTOP) faultCleaned = false;

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

bool control_start_program(const Preset& p) {
  MotionCommand cmd{};
  cmd.program = true;
  cmd.preset = p;
  switch (p.mode) {
    case STATE_RUNNING:
      cmd.type = MOTION_CMD_START_CONTINUOUS;
      cmd.continuous_soft_start = p.cont_soft_start;
      cmd.continuous_auto_stop_ms = p.timer_auto_stop ? p.timer_ms : 0;
      break;
    case STATE_PULSE:
      cmd.type = MOTION_CMD_START_PULSE;
      cmd.pulse_on_ms = p.pulse_on_ms;
      cmd.pulse_off_ms = p.pulse_off_ms;
      cmd.pulse_cycles = p.pulse_cycles;
      break;
    case STATE_STEP:
      cmd.type = MOTION_CMD_START_STEP;
      cmd.step_angle = p.step_angle;
      cmd.step_repeats = p.step_repeats;
      cmd.step_dwell_sec = p.step_dwell_sec;
      break;
    default:
      return false;
  }
  return queue_motion_command(cmd);
}

bool control_apply_motor_settings(const SystemSettings& settings) {
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_CONFIG;
  cmd.settings = settings;
  configStatus.store(CONFIG_PENDING);
  if (queue_motion_command(cmd)) return true;
  configStatus.store(CONFIG_CANCELLED);
  return false;
}
