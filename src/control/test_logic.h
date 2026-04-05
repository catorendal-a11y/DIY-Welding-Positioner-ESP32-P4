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

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN NEEDS REBUILD — mirrors screen_needs_rebuild() in screens.cpp
// ScreenId values from screens.h enum:
//   SCREEN_PROGRAM_EDIT=8, SCREEN_EDIT_PULSE=12, SCREEN_EDIT_STEP=13,
//   SCREEN_EDIT_CONT=14
// ───────────────────────────────────────────────────────────────────────────────
inline bool screen_needs_rebuild_testable(int id) {
  return id == 8 || id == 12 || id == 13 || id == 14;
}
