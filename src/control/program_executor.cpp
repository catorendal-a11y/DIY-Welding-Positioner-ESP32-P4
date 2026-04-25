// Program Executor - Runs saved welding programs through controlTask

#include "program_executor.h"
#include "control.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "../safety/safety.h"

const char* program_executor_result_name(ProgramExecutorResult result) {
  switch (result) {
    case PROGRAM_EXEC_OK: return "OK";
    case PROGRAM_EXEC_INVALID_PRESET: return "INVALID_PRESET";
    case PROGRAM_EXEC_BLOCKED_SAFETY: return "BLOCKED_SAFETY";
    case PROGRAM_EXEC_BLOCKED_STATE: return "BLOCKED_STATE";
    case PROGRAM_EXEC_INVALID_MODE: return "INVALID_MODE";
    case PROGRAM_EXEC_REQUEST_FAILED: return "REQUEST_FAILED";
    default: return "UNKNOWN";
  }
}

static ProgramExecutorResult program_executor_request_result(bool queued) {
  if (queued) return PROGRAM_EXEC_OK;
  speed_clear_program_direction_override();
  motor_restore_configured_acceleration();
  return PROGRAM_EXEC_REQUEST_FAILED;
}

ProgramExecutorResult program_executor_start_preset(const Preset* preset) {
  if (preset == nullptr) {
    return PROGRAM_EXEC_INVALID_PRESET;
  }
  if (safety_inhibit_motion()) {
    LOG_W("ProgramExecutor: start blocked by safety");
    return PROGRAM_EXEC_BLOCKED_SAFETY;
  }
  if (control_get_state() != STATE_IDLE) {
    LOG_W("ProgramExecutor: start blocked, state=%s", control_get_state_string());
    return PROGRAM_EXEC_BLOCKED_STATE;
  }

  Preset run = *preset;
  preset_clamp_mode_to_mask(&run);

  Direction dir = (run.direction == DIR_CCW) ? DIR_CCW : DIR_CW;
  speed_set_program_direction_override(dir);
  speed_slider_set(run.rpm);

  switch (run.mode) {
    case STATE_RUNNING:
      LOG_I("ProgramExecutor: CONT '%s' rpm=%.3f dir=%s soft=%u auto_stop=%lu",
            run.name, run.rpm, dir == DIR_CW ? "CW" : "CCW",
            (unsigned)run.cont_soft_start, (unsigned long)run.timer_ms);
      return program_executor_request_result(control_start_continuous(
          run.cont_soft_start != 0, run.timer_auto_stop ? run.timer_ms : 0u));

    case STATE_PULSE:
      LOG_I("ProgramExecutor: PULSE '%s' rpm=%.3f on=%lu off=%lu cycles=%u dir=%s",
            run.name, run.rpm, (unsigned long)run.pulse_on_ms,
            (unsigned long)run.pulse_off_ms, (unsigned)run.pulse_cycles,
            dir == DIR_CW ? "CW" : "CCW");
      return program_executor_request_result(
          control_start_pulse(run.pulse_on_ms, run.pulse_off_ms, run.pulse_cycles));

    case STATE_STEP:
      LOG_I("ProgramExecutor: STEP '%s' rpm=%.3f angle=%.1f repeats=%u dwell=%.1f dir=%s",
            run.name, run.rpm, run.step_angle, (unsigned)run.step_repeats,
            run.step_dwell_sec, dir == DIR_CW ? "CW" : "CCW");
      return program_executor_request_result(
          control_start_step_sequence(run.step_angle, run.step_repeats, run.step_dwell_sec));

    default:
      LOG_W("ProgramExecutor: invalid mode %d", (int)run.mode);
      speed_clear_program_direction_override();
      return PROGRAM_EXEC_INVALID_MODE;
  }
}
