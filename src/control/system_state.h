// System states and their transition rules. Pure logic, shared with the native tests.
#pragma once

typedef enum {
  STATE_IDLE = 0,  // Motor stopped, ready for command
  STATE_RUNNING,   // Continuous rotation mode
  STATE_PULSE,     // Pulse mode (rotate/pause cycles)
  STATE_STEP,      // Step mode (fixed angle steps)
  STATE_JOG,       // Jog mode (run while button held)
  STATE_STOPPING,  // Decelerating to stop
  STATE_ESTOP,     // Emergency stop activated
  // Appended after ESTOP so existing logged/serialized values stay stable.
  STATE_ENABLING   // Validated start waiting out the driver ENA settle window
} SystemState;

inline bool control_is_valid_transition(SystemState from, SystemState to) {
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

inline const char* control_state_name(SystemState s) {
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
