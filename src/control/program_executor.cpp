// Program Executor - Runs saved welding programs through controlTask

#include "program_executor.h"
#include "control.h"
#include "../config.h"
#include "../event_log.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "../safety/safety.h"

const char* program_executor_result_name(ProgramExecutorResult result) {
  switch (result) {
    case PROGRAM_EXEC_OK:
      return "OK";
    case PROGRAM_EXEC_INVALID_PRESET:
      return "INVALID_PRESET";
    case PROGRAM_EXEC_BLOCKED_SAFETY:
      return "BLOCKED_SAFETY";
    case PROGRAM_EXEC_BLOCKED_STATE:
      return "BLOCKED_STATE";
    case PROGRAM_EXEC_INVALID_MODE:
      return "INVALID_MODE";
    case PROGRAM_EXEC_REQUEST_FAILED:
      return "REQUEST_FAILED";
    default:
      return "UNKNOWN";
  }
}

ProgramExecutorResult program_executor_start_preset(const Preset* preset) {
  if (preset == nullptr) {
    event_log_add("PROGRAM INVALID");
    return PROGRAM_EXEC_INVALID_PRESET;
  }
  if (safety_inhibit_motion()) {
    LOG_W("ProgramExecutor: start blocked by safety");
    event_log_add("PROGRAM BLOCK SAFETY");
    return PROGRAM_EXEC_BLOCKED_SAFETY;
  }
  if (control_get_state() != STATE_IDLE) {
    LOG_W("ProgramExecutor: start blocked, state=%s", control_get_state_string());
    event_log_addf("PROGRAM BLOCK %s", control_get_state_string());
    return PROGRAM_EXEC_BLOCKED_STATE;
  }

  Preset run = *preset;
  preset_clamp_mode_to_mask(&run);

  if (run.mode != STATE_RUNNING && run.mode != STATE_PULSE && run.mode != STATE_STEP)
    return PROGRAM_EXEC_INVALID_MODE;
  return control_start_program(run) ? PROGRAM_EXEC_OK : PROGRAM_EXEC_REQUEST_FAILED;
}
