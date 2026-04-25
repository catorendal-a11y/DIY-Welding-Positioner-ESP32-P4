// TIG Rotator Controller - Pure Logic for State Machine Testing
// Extracted from control.cpp for native unit testing (no Arduino dependencies)

#pragma once

// ───────────────────────────────────────────────────────────────────────────────
// SYSTEM STATE ENUM (mirror of control.h for native tests)
// ───────────────────────────────────────────────────────────────────────────────
#ifndef SYSTEM_STATE_DEFINED
#define SYSTEM_STATE_DEFINED
typedef enum {
  STATE_IDLE = 0,
  STATE_RUNNING,
  STATE_PULSE,
  STATE_STEP,
  STATE_JOG,
  STATE_STOPPING,
  STATE_ESTOP
} SystemState;
#endif

// ───────────────────────────────────────────────────────────────────────────────
// STATE TRANSITION VALIDATION — pure logic, no side effects
// ───────────────────────────────────────────────────────────────────────────────
inline bool control_is_valid_transition(SystemState from, SystemState to) {
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
// STATE NAME — enum to string for debug output
// ───────────────────────────────────────────────────────────────────────────────
inline const char* control_state_name(SystemState s) {
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

enum ModeRequestTestable : unsigned char {
  MODE_REQ_NONE_TESTABLE = 0,
  MODE_REQ_CONTINUOUS_TESTABLE = 1,
  MODE_REQ_PULSE_TESTABLE = 2,
  MODE_REQ_STEP_TESTABLE = 3,
  MODE_REQ_JOG_CW_TESTABLE = 4,
  MODE_REQ_JOG_CCW_TESTABLE = 5
};

inline bool control_request_is_jog_testable(unsigned char req) {
  return req == MODE_REQ_JOG_CW_TESTABLE || req == MODE_REQ_JOG_CCW_TESTABLE;
}

inline unsigned char control_cancel_jog_request_testable(unsigned char req) {
  return control_request_is_jog_testable(req) ? MODE_REQ_NONE_TESTABLE : req;
}

enum MotionCommandTypeTestable : unsigned char {
  MOTION_CMD_NONE_TESTABLE = 0,
  MOTION_CMD_START_CONTINUOUS_TESTABLE = 1,
  MOTION_CMD_START_PULSE_TESTABLE = 2,
  MOTION_CMD_START_STEP_TESTABLE = 3,
  MOTION_CMD_START_JOG_TESTABLE = 4,
  MOTION_CMD_STOP_TESTABLE = 5,
  MOTION_CMD_STOP_JOG_TESTABLE = 6
};

inline bool control_command_is_start_testable(unsigned char cmd) {
  return cmd == MOTION_CMD_START_CONTINUOUS_TESTABLE ||
         cmd == MOTION_CMD_START_PULSE_TESTABLE ||
         cmd == MOTION_CMD_START_STEP_TESTABLE ||
         cmd == MOTION_CMD_START_JOG_TESTABLE;
}

inline unsigned char control_mailbox_overwrite_testable(unsigned char first,
                                                        unsigned char second) {
  (void)first;
  return second;
}

inline bool control_start_command_waits_for_idle_testable(SystemState state,
                                                          unsigned char cmd) {
  return control_command_is_start_testable(cmd) && state != STATE_IDLE;
}

inline unsigned int continuous_auto_stop_request_testable(bool timer_auto_stop,
                                                          unsigned int timer_ms) {
  return timer_auto_stop ? timer_ms : 0u;
}

inline unsigned int soft_start_acceleration_testable(unsigned int configured) {
  unsigned int soft = configured / 4u;
  if (soft < 1000u) soft = 1000u;
  if (soft > 30000u) soft = 30000u;
  return soft;
}

// ───────────────────────────────────────────────────────────────────────────────
// PENDING REQUEST SAFETY - mirrors ESTOP request cleanup in control.cpp
// ───────────────────────────────────────────────────────────────────────────────
struct ControlPendingSnapshot {
  SystemState state;
  unsigned char mode_req;
  bool stop;
  bool stop_jog;
  unsigned int pulse_on_ms;
  unsigned int pulse_off_ms;
  unsigned short pulse_cycles;
  float step_angle;
};

inline void control_clear_pending_motion_testable(ControlPendingSnapshot& s) {
  s.mode_req = MODE_REQ_NONE_TESTABLE;
  s.stop = false;
  s.stop_jog = false;
  s.pulse_on_ms = 0;
  s.pulse_off_ms = 0;
  s.pulse_cycles = 0;
  s.step_angle = 0.0f;
}

inline void control_enter_estop_testable(ControlPendingSnapshot& s) {
  if (control_is_valid_transition(s.state, STATE_ESTOP)) {
    s.state = STATE_ESTOP;
    control_clear_pending_motion_testable(s);
  }
}

inline void control_process_estop_tick_testable(ControlPendingSnapshot& s) {
  if (s.state == STATE_ESTOP) {
    control_clear_pending_motion_testable(s);
  }
}

// SCREEN NEEDS REBUILD - mirrors screen_needs_rebuild() in screens.cpp
// ScreenId values from screens.h enum:
//   SCREEN_PROGRAM_EDIT=8, SCREEN_EDIT_PULSE=12, SCREEN_EDIT_STEP=13,
//   SCREEN_EDIT_CONT=14
inline bool screen_needs_rebuild_testable(int id) {
  return id == 8 || id == 12 || id == 13 || id == 14;
}
