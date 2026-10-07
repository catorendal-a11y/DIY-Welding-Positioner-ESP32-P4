#include "../motor/calibration.h"
// Control - State machine core with motion command mailbox
#include "control.h"
#include "../app_state.h"
#include "modes.h"
#include "../motor/acceleration.h"
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
static std::atomic<uint32_t> configSaveTicket{0};
uint32_t control_config_save_ticket() { return configSaveTicket.load(); }
ConfigApplyStatus control_config_status() { return configStatus.load(); }
static std::atomic<bool> resetStepPending{false};
static std::atomic<bool> setupActive{false}, calibrationActive{false};
static SnapshotMailbox<ControlSnapshot> snapshotMailbox;
static uint32_t snapshotSequence = 0;
static std::atomic<uint32_t> lastCycleAt{0};
static std::atomic<bool> cyclePublished{false};
static bool faultCleaned = false;
static bool configRollbackPending = false;
void control_set_setup_active(bool active) { setupActive.store(active); }
bool control_setup_active() { return setupActive.load() || calibrationActive.load(); }
void control_set_calibration_active(bool active) { if (active) control_stop(); calibrationActive.store(active); }
bool control_read_snapshot(ControlSnapshot& out) { return snapshotMailbox.read(out); }
static void publish_snapshot() {
  ControlSnapshot s;
  s.sequence = ++snapshotSequence; s.timestamp_ms = millis();
  s.state = control_get_state(); s.target_rpm = speed_get_target_rpm();
  s.estimated_rpm = speed_get_actual_rpm(); s.direction = motor_direction_is_cw() ? DIR_CW : DIR_CCW;
  if (s.state == STATE_IDLE) s.direction = speed_get_direction();
  s.source = speed_get_input_source(); s.progress_degrees = step_get_accumulated();
  s.completed_steps = step_get_count(); s.pulse_cycles = pulse_get_cycle_count();
  s.motor_running = motor_is_running();
  s.state = control_get_state(); // Motor queries may have latched a fault.
  snapshotMailbox.publish(s);
  lastCycleAt.store(s.timestamp_ms); cyclePublished.store(true);
}
static std::atomic<uint32_t> jogRenewedMs{0};
static std::atomic<uint32_t> stopRequestedAt{0}, stoppingAt{0}, stoppingBudget{0};
static std::atomic<uint32_t> motionDeadlineAt{0}, motionDeadlineBudget{0};
static bool staleInvalidated = false;
static std::atomic<SystemState> motionDeadlineState{STATE_IDLE};
void control_clear_motion_deadline() { motionDeadlineState.store(STATE_IDLE, std::memory_order_release); }
void control_expect_motion_completion(SystemState state, uint32_t timeout_ms) {
  control_clear_motion_deadline();
  motionDeadlineAt.store(millis()); motionDeadlineBudget.store(timeout_ms);
  motionDeadlineState.store(state, std::memory_order_release);
}
uint32_t control_motion_generation() { return motionGate.ticket(); }
// safetyTask dead-man channel: true when the control loop has not published a
// fresh cycle within the freshness window. safetyTask latches FAULT_CONTROL_STALE.
bool control_heartbeat_stale(uint32_t now) {
  return cyclePublished.load() && !control_timestamp_fresh(now, lastCycleAt.load(), true);
}
bool control_deferred_start_valid(uint32_t generation) {
  return motionGate.valid(generation) && control_get_state() == STATE_IDLE && !safety_inhibit_motion() &&
         control_timestamp_fresh(millis(), lastCycleAt.load(), cyclePublished.load());
}
void control_check_stop_deadline(uint32_t now) {
  const bool stale = cyclePublished.load() && !control_timestamp_fresh(now, lastCycleAt.load(), true);
  if (stale && !staleInvalidated) motionGate.stop();
  staleInvalidated = stale;
  const SystemState deadlineState = motionDeadlineState.load(std::memory_order_acquire);
  const uint32_t motionAge = now - motionDeadlineAt.load();
  const uint32_t requested = stopRequestedAt.load();
  const uint32_t stopping = stoppingAt.load();
  if ((requested && static_cast<int32_t>(now - (requested - 1u)) >= 50) ||
      (deadlineState != STATE_IDLE && control_get_state() == deadlineState &&
       motionAge <= INT32_MAX && motionAge >= motionDeadlineBudget.load()) ||
      (stopping && control_get_state() == STATE_STOPPING &&
       static_cast<int32_t>(now - (stopping - 1u)) >= static_cast<int32_t>(stoppingBudget.load()))) {
    safety_report_motor_fault(FAULT_MOTOR_TIMEOUT);
    stopRequestedAt.store(0);
    stoppingAt.store(0); control_clear_motion_deadline();
  }
}

bool control_motion_blocked() { return motionGate.blocked(); }
void control_renew_jog() { jogRenewedMs.store(millis()); }

static bool is_active_motion_state(SystemState state) {
  return state == STATE_RUNNING || state == STATE_PULSE || state == STATE_STEP || state == STATE_JOG ||
         state == STATE_ENABLING;
}

// Command parked while STATE_ENABLING waits out the driver ENA settle window.
static MotionCommand s_enablingCmd{};

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

static bool queue_motion_command(const MotionCommand& cmd, uint32_t ticket) {
  if (!control_timestamp_fresh(millis(), lastCycleAt.load(), cyclePublished.load())) return false;
  if (controlQueue == nullptr) {
    LOG_W("Control queue not ready");
    return false;
  }
  if (calibrationActive.load() && (cmd.program || (cmd.type != MOTION_CMD_START_STEP && cmd.type != MOTION_CMD_START_JOG))) return false;
  if (!motionGate.valid(ticket)) return false;
  if (motionGate.blocked() || control_get_state() != STATE_IDLE || safety_inhibit_motion()) return false;
  if (!cmd.program && cmd.type != MOTION_CMD_CONFIG && cmd.type != MOTION_CMD_START_JOG &&
      !motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm())) return false;
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
      case STATE_ENABLING:
      case STATE_ESTOP:
        return true;
      default:
        return false;
    }
  }
  if (from == STATE_ENABLING) {
    // Settle elapsed -> the requested mode; aborted/stopped before pulses.
    switch (to) {
      case STATE_RUNNING:
      case STATE_PULSE:
      case STATE_STEP:
      case STATE_JOG:
      case STATE_STOPPING:
      case STATE_IDLE:
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
  control_clear_motion_deadline(); staleInvalidated = false;
  control_queue_init();
  clear_pending_motion_requests();
  faultCleaned = false; cyclePublished.store(false);
  // Motor/display/storage initialization precedes this call. Keep any fault
  // already latched during boot visible to cleanup and the reset overlay.
  const SystemState initialState = safety_get_fault_reason() == FAULT_NONE ? STATE_IDLE : STATE_ESTOP;
  currentState.store(initialState, std::memory_order_release);
  previousState.store(initialState, std::memory_order_release);
  if (initialState == STATE_ESTOP) { digitalWrite(PIN_ENA, HIGH); motionGate.stop(); }
  event_log_add("CONTROL INIT");
  LOG_I("Control init: state=%s", control_state_name(initialState));
}

bool control_transition_to(SystemState newState) {
  if (newState == STATE_ESTOP) {
    control_clear_motion_deadline();
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

  if (newState != STATE_STEP && newState != STATE_PULSE) control_clear_motion_deadline();
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
    case STATE_ENABLING:
      return "ENABLING";
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
  return queue_motion_command(cmd, motionGate.ticket());
}

bool control_start_deferred_continuous(uint32_t generation) {
  if (!control_deferred_start_valid(generation)) return false;
  MotionCommand cmd{}; cmd.type = MOTION_CMD_START_CONTINUOUS;
  return queue_motion_command(cmd, generation); // Never mint a fresh ticket after a fault/reset.
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
  return queue_motion_command(cmd, motionGate.ticket());
}

bool control_start_step(float angle_deg) { return control_start_step_sequence(angle_deg, 1, 0.0f); }

bool control_start_step_sequence(float angle_deg, uint16_t repeats, float dwell_sec) {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_STEP;
  if (!std::isfinite(angle_deg) || angle_deg <= 0 || angle_deg > 3600 ||
      !std::isfinite(dwell_sec) || dwell_sec < 0 || dwell_sec > 30 || repeats < 1 || repeats > 99) return false;
  if (angleToSteps(angle_deg) <= 0) return false;
  cmd.step_angle = angle_deg;
  cmd.step_repeats = repeats;
  cmd.step_dwell_sec = dwell_sec;
  return queue_motion_command(cmd, motionGate.ticket());
}

bool control_start_jog_cw() {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_JOG;
  cmd.direction = DIR_CW;
  control_renew_jog();
  return queue_motion_command(cmd, motionGate.ticket());
}

bool control_start_jog_ccw() {
  if (safety_inhibit_motion()) return false;
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_START_JOG;
  cmd.direction = DIR_CCW;
  control_renew_jog();
  return queue_motion_command(cmd, motionGate.ticket());
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
  } else if (cur == STATE_STEP || cur == STATE_ENABLING) {
    // ENABLING has emitted no pulses yet; STOPPING completes at rest and
    // disables ENA.
    control_transition_to(STATE_STOPPING);
  }
}

// Runs a fully admitted motion command in the mode it requests. Called from
// process_pending_requests (settle already satisfied or not required) and from
// the STATE_ENABLING replay once the driver settle window has elapsed.
static void dispatch_motion_command(const MotionCommand& cmd);

static void copy_motor_settings(SystemSettings& destination, const SystemSettings& source) {
  destination.microstep = source.microstep;
  destination.acceleration = source.acceleration;
  destination.max_rpm = source.max_rpm;
  destination.dir_switch_enabled = source.dir_switch_enabled;
  destination.invert_direction = source.invert_direction;
  destination.stepper_driver = source.stepper_driver;
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
  if ((cur == STATE_JOG || (cur == STATE_ENABLING && s_enablingCmd.type == MOTION_CMD_START_JOG)) &&
      millis() - jogRenewedMs.load() > 150u) {
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
    // Keep the proposal private until hardware accepts it. A concurrent
    // settings save must only ever snapshot committed motor fields.
    SystemSettings previous{};
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    previous = g_settings;
    xSemaphoreGive(g_settings_mutex);
    const bool applied = motor_apply_settings(cmd.settings);
    if (!applied || safety_inhibit_motion()) {
      configRollbackPending = !motor_apply_settings(previous);
      configStatus.store(CONFIG_CANCELLED);
      return;
    }
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    copy_motor_settings(g_settings, cmd.settings);
    xSemaphoreGive(g_settings_mutex);
    g_dir_switch_cache.store(cmd.settings.dir_switch_enabled);
    speed_sync_rpm_limits_from_settings();
    configSaveTicket.store(storage_request_settings_save());
    configStatus.store(CONFIG_APPLIED);
    return;
  }
  if (motor_ena_settle_pending()) {
    // Validated start, driver enable not yet settled: assert ENA now and hold
    // all pulses until the datasheet window elapses. STATE_ENABLING replays
    // the command; E-STOP/ALM keep their immediate paths during the wait.
    if (!motor_prepare_start()) return;
    s_enablingCmd = cmd;
    control_transition_to(STATE_ENABLING);
    return;
  }
  dispatch_motion_command(cmd);
}

// Runs a fully admitted motion command in the mode it requests. Called from
// process_pending_requests (settle already satisfied or not required) and from
// the STATE_ENABLING replay once the driver settle window has elapsed.
static void dispatch_motion_command(const MotionCommand& cmd) {
  if (cmd.program) {
    if (!control_program_feasible(cmd.preset)) return;
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
      jog_start(speed_resolve_direction(cmd.direction));
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
void control_run_cycle() {
  if (control_get_state() == STATE_IDLE) calibration_process_pending();
  // STOP/ESTOP dispatch precedes any live speed changes.
    if (control_get_state() == STATE_IDLE && resetStepPending.exchange(false)) step_reset_accumulator();
    process_pending_requests();
    speed_apply();
    if (control_get_state() == STATE_IDLE && acceleration_has_pending_apply()) {
      acceleration_clear_pending(); motor_apply_settings();
    }

    SystemState curState = currentState.load(std::memory_order_acquire);

    if (curState == STATE_ESTOP) {
      // Potentially blocking library cleanup never runs in safetyTask.
      if (!faultCleaned) {
        if (!motor_halt()) { publish_snapshot(); return; }
        if (configRollbackPending) {
          if (!motor_apply_settings()) { publish_snapshot(); return; }
          configRollbackPending = false;
        }
        if (!motor_restore_configured_acceleration()) { publish_snapshot(); return; }
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
      case STATE_ENABLING: {
        // Non-blocking driver settle. safetyTask/ISR keep their immediate
        // E-STOP paths during this window; a STOP request transitions to
        // STOPPING via process_pending_requests.
        if (motor_ena_settle_pending()) break;
        if (!motionGate.valid(s_enablingCmd.ticket)) {
          control_stop(); break;
        }
        dispatch_motion_command(s_enablingCmd);
        if (currentState.load(std::memory_order_acquire) == STATE_ENABLING) {
          // The mode rejected the start (inhibit race, invalid rate): the
          // admission checks passed earlier, so fail closed without pulses.
          motor_disable();
          control_transition_to(STATE_IDLE);
        }
        break;
      }
      default:
        break;
    }

  motor_refresh_hz_cache();
  publish_snapshot();
}
void controlTask(void* pvParameters) {
  LOG_I("Control task started on Core %d", xPortGetCoreID());
  motor_bind_owner();
  safety_register_watchdog(); safety_task_ready(4u);
  TickType_t t = xTaskGetTickCount();
  for (;;) {
    safety_feed_watchdog(); control_run_cycle();
    vTaskDelayUntil(&t, pdMS_TO_TICKS(5));
  }
}

bool control_program_feasible(const Preset& p) {
  if (!std::isfinite(p.rpm) || p.rpm < MIN_RPM || p.rpm > speed_get_rpm_max() ||
      !std::isfinite(p.workpiece_diameter_mm) || p.workpiece_diameter_mm < 0 || p.workpiece_diameter_mm > 20000 ||
      !std::isfinite(p.step_angle) || !std::isfinite(p.step_dwell_sec) ||
      (p.mode == STATE_STEP && (p.step_angle <= 0 || p.step_angle > 3600 || p.step_repeats < 1 ||
                               p.step_repeats > 99 || p.step_dwell_sec < 0 || p.step_dwell_sec > 30))) return false;
  if (p.rpm < speed_get_rpm_min_for_diameter(p.workpiece_diameter_mm)) return false;
  if (p.mode == STATE_STEP && angleToStepsForDiameter(p.step_angle, p.workpiece_diameter_mm) <= 0) return false;
  return p.mode == STATE_RUNNING || p.mode == STATE_PULSE || p.mode == STATE_STEP;
}
bool control_start_program(const Preset& p) {
  if (!control_program_feasible(p)) return false;
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
  return queue_motion_command(cmd, motionGate.ticket());
}

bool control_apply_motor_settings(const SystemSettings& settings) {
  if (!std::isfinite(settings.max_rpm) || settings.max_rpm < MIN_RPM || settings.max_rpm > MAX_RPM ||
      !std::isfinite(settings.calibration_factor) || settings.calibration_factor < 0.5f || settings.calibration_factor > 1.5f ||
      settings.acceleration < 1000 || settings.acceleration > 30000 || settings.stepper_driver > 1 ||
      (settings.microstep != 4 && settings.microstep != 8 && settings.microstep != 16 && settings.microstep != 32)) {
    configStatus.store(CONFIG_CANCELLED); return false;
  }
  MotionCommand cmd{};
  cmd.type = MOTION_CMD_CONFIG;
  cmd.settings = settings;
  configStatus.store(CONFIG_PENDING);
  if (queue_motion_command(cmd, motionGate.ticket())) return true;
  configStatus.store(CONFIG_CANCELLED);
  return false;
}
