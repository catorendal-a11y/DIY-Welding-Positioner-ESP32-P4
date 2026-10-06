#pragma once

typedef enum {
  FAULT_NONE = 0,
  FAULT_ESTOP_PRESSED,
  FAULT_ESTOP_GLITCH,
  FAULT_DRIVER_ALARM,
  FAULT_MOTOR_INIT_FAILED,
  FAULT_DISPLAY_INIT_FAILED,
  FAULT_LVGL_INIT_FAILED,
  FAULT_STORAGE_CORRUPT,
  FAULT_PEDAL_INPUT,
  FAULT_WATCHDOG_RESET,
  FAULT_MOTOR_COMMAND,
  FAULT_MOTOR_TIMEOUT,
  // Supervisor faults (safetyTask dead-man detection). Keep last: the
  // safety_get_fault_reason() range check bounds to the final member.
  FAULT_CONTROL_STALE,
  FAULT_INPUT_STALE
} FaultReason;

inline const char* safety_fault_reason_name(FaultReason reason) {
  switch (reason) {
    case FAULT_MOTOR_COMMAND: return "MOTOR COMMAND";
    case FAULT_MOTOR_TIMEOUT: return "MOTOR TIMEOUT";
    case FAULT_CONTROL_STALE: return "CONTROL STALE";
    case FAULT_INPUT_STALE: return "INPUT STALE";
    case FAULT_PEDAL_INPUT:
      return "PEDAL INPUT";
    case FAULT_NONE:
      return "NONE";
    case FAULT_ESTOP_PRESSED:
      return "E-STOP";
    case FAULT_ESTOP_GLITCH:
      return "E-STOP GLITCH";
    case FAULT_DRIVER_ALARM:
      return "DRIVER ALM";
    case FAULT_MOTOR_INIT_FAILED:
      return "MOTOR INIT";
    case FAULT_DISPLAY_INIT_FAILED:
      return "DISPLAY INIT";
    case FAULT_LVGL_INIT_FAILED:
      return "LVGL INIT";
    case FAULT_STORAGE_CORRUPT:
      return "STORAGE";
    case FAULT_WATCHDOG_RESET:
      return "WATCHDOG";
    default:
      return "UNKNOWN";
  }
}

inline const char* safety_fault_reason_message(FaultReason reason) {
  switch (reason) {
    case FAULT_MOTOR_COMMAND: return "Motor rejected command; inspect drive and reset";
    case FAULT_MOTOR_TIMEOUT: return "Motor response timed out; inspect drive and reset";
    case FAULT_CONTROL_STALE: return "Control updates stopped; motion inhibited - restart the controller";
    case FAULT_INPUT_STALE: return "Input task stopped responding; motion inhibited - restart the controller";
    case FAULT_PEDAL_INPUT:
      return "Pedal measurement unavailable; release pedal and restore input";
    case FAULT_NONE:
      return "No latched fault";
    case FAULT_ESTOP_PRESSED:
      return "Physical E-STOP input is active";
    case FAULT_ESTOP_GLITCH:
      return "E-STOP input changed during debounce";
    case FAULT_DRIVER_ALARM:
      return "DM542T driver alarm input is active";
    case FAULT_MOTOR_INIT_FAILED:
      return "Motor driver init failed";
    case FAULT_DISPLAY_INIT_FAILED:
      return "Display init failed";
    case FAULT_LVGL_INIT_FAILED:
      return "LVGL init failed";
    case FAULT_STORAGE_CORRUPT:
      return "Storage data was invalid";
    case FAULT_WATCHDOG_RESET:
      return "Watchdog reset was detected";
    default:
      return "Unknown fault";
  }
}
