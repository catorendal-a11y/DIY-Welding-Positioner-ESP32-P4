// TIG Rotator Controller - State Machine Implementation
// Controls mode transitions and coordinates all subsystems

#include "control.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "esp_task_wdt.h"

// ───────────────────────────────────────────────────────────────────────────────
// MODE IMPLEMENTATION FUNCTIONS (from modes/*.cpp)
// ───────────────────────────────────────────────────────────────────────────────
extern void continuous_start();
extern void continuous_stop();
extern void pulse_start(uint32_t on_ms, uint32_t off_ms);
extern void pulse_stop();
extern void pulse_update();
extern void step_execute(float angle_deg);
extern void step_reset_accumulator();
extern void step_update();
extern void jog_start(Direction dir);
extern void jog_stop();
extern void jog_set_speed(float rpm);
extern void timer_start(uint32_t duration_sec);
extern void timer_stop();
extern void timer_update();

// Mode getters
extern float step_get_accumulated();
extern long step_get_count();
extern float jog_get_speed();
extern uint32_t timer_get_remaining_sec();
extern uint32_t timer_get_duration();

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static SystemState currentState = STATE_IDLE;
static SystemState previousState = STATE_IDLE;

// ───────────────────────────────────────────────────────────────────────────────
// STATE TRANSITION VALIDATION
// ───────────────────────────────────────────────────────────────────────────────
bool control_is_valid_transition(SystemState from, SystemState to) {
  // ESTOP can be reached from any state
  if (to == STATE_ESTOP) return true;

  // From ESTOP, only IDLE is allowed (requires manual reset)
  if (from == STATE_ESTOP) return (to == STATE_IDLE);

  // From IDLE, can go to any active mode or ESTOP
  if (from == STATE_IDLE) {
    switch (to) {
      case STATE_RUNNING:
      case STATE_PULSE:
      case STATE_STEP:
      case STATE_JOG:
      case STATE_TIMER:
      case STATE_ESTOP:
        return true;
      default:
        return false;
    }
  }

  // From RUNNING, can only go to STOPPING or ESTOP
  if (from == STATE_RUNNING) {
    return (to == STATE_STOPPING || to == STATE_ESTOP);
  }

  // From PULSE/STEP/JOG/TIMER, can go to STOPPING or ESTOP
  if (from == STATE_PULSE || from == STATE_STEP || from == STATE_JOG || from == STATE_TIMER) {
    return (to == STATE_STOPPING || to == STATE_ESTOP);
  }

  // From STOPPING, can go to IDLE or ESTOP
  if (from == STATE_STOPPING) {
    return (to == STATE_IDLE || to == STATE_ESTOP);
  }

  return false;
}

// ───────────────────────────────────────────────────────────────────────────────
// STATE MACHINE CORE
// ───────────────────────────────────────────────────────────────────────────────
void control_init() {
  currentState = STATE_IDLE;
  previousState = STATE_IDLE;
  LOG_I("Control init: state=IDLE");
}

void control_transition_to(SystemState newState) {
  // Validate transition
  if (!control_is_valid_transition(currentState, newState)) {
    LOG_W("Invalid transition: %s -> %s",
          control_get_state_string(),
          control_state_name(newState));
    return;
  }

  previousState = currentState;
  currentState = newState;

  LOG_I("State: %s -> %s",
        control_state_name(previousState),
        control_state_name(newState));

  // Handle state entry actions
  switch (newState) {
    case STATE_IDLE:
      // Motor should already be stopped from STOPPING or ESTOP
      break;

    case STATE_RUNNING:
      // Continuous mode - speed applied by motorTask
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
  return currentState;
}

const char* control_get_state_string() {
  return control_state_name(currentState);
}

const char* control_state_name(SystemState s) {
  switch (s) {
    case STATE_IDLE:     return "IDLE";
    case STATE_RUNNING:  return "RUNNING";
    case STATE_PULSE:    return "PULSE";
    case STATE_STEP:     return "STEP";
    case STATE_JOG:      return "JOG";
    case STATE_TIMER:    return "TIMER";
    case STATE_STOPPING: return "STOPPING";
    case STATE_ESTOP:    return "ESTOP";
    default:             return "UNKNOWN";
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// MODE CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void control_start_continuous() {
  if (currentState != STATE_IDLE) control_stop();
  // Wait for STOPPING -> IDLE transition (max 500ms)
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  continuous_start();
}

void control_stop() {
  if (currentState == STATE_IDLE || currentState == STATE_STOPPING || currentState == STATE_ESTOP) {
    return;
  }
  if (currentState == STATE_RUNNING) {
    continuous_stop();
  } else if (currentState == STATE_PULSE) {
    pulse_stop();
  } else if (currentState == STATE_JOG) {
    jog_stop();
  } else if (currentState == STATE_TIMER) {
    timer_stop();
  } else if (currentState == STATE_STEP) {
    // Step mode: force stop motor and go to STOPPING
    motor_stop();
    control_transition_to(STATE_STOPPING);
  }
}

void control_start_pulse(uint32_t on_ms, uint32_t off_ms) {
  if (currentState != STATE_IDLE) control_stop();
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  pulse_start(on_ms, off_ms);
}

void control_start_step(float angle_deg) {
  if (currentState != STATE_IDLE) control_stop();
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  step_execute(angle_deg);
}

void control_start_jog_cw() {
  if (currentState != STATE_IDLE && currentState != STATE_JOG) control_stop();
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  jog_start(DIR_CW);
}

void control_start_jog_ccw() {
  if (currentState != STATE_IDLE && currentState != STATE_JOG) control_stop();
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  jog_start(DIR_CCW);
}

void control_stop_jog() {
  jog_stop();
}

void control_start_timer(uint32_t duration_sec) {
  if (currentState != STATE_IDLE) control_stop();
  for (int i = 0; i < 50 && currentState != STATE_IDLE; i++) delay(10);
  timer_start(duration_sec);
}

uint32_t control_get_timer_remaining() {
  return timer_get_remaining_sec();
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
// CONTROL TASK — Main state machine loop
// ───────────────────────────────────────────────────────────────────────────────
void controlTask(void* pvParameters) {
  LOG_I("Control task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);  // Register with watchdog

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    esp_task_wdt_reset();  // Feed watchdog

    // Check if motor has stopped (STOPPING -> IDLE transition)
    if (currentState == STATE_STOPPING) {
      if (!motor_is_running()) {
        motor_disable();  // Disable motor (ENA HIGH) after smooth stop
        control_transition_to(STATE_IDLE);
      }
    }

    // Mode-specific updates
    switch (currentState) {
      case STATE_PULSE:
        pulse_update();
        break;
      case STATE_STEP:
        step_update();
        break;
      case STATE_TIMER:
        timer_update();
        break;
      default:
        break;
    }

    vTaskDelayUntil(&t, pdMS_TO_TICKS(10));  // 10ms cycle
  }
}
