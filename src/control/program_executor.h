// Program Executor - Runs saved welding programs through controlTask

#pragma once

#include "../storage/storage.h"

typedef enum {
  PROGRAM_EXEC_OK = 0,
  PROGRAM_EXEC_INVALID_PRESET,
  PROGRAM_EXEC_BLOCKED_SAFETY,
  PROGRAM_EXEC_BLOCKED_STATE,
  PROGRAM_EXEC_INVALID_MODE,
  PROGRAM_EXEC_REQUEST_FAILED
} ProgramExecutorResult;

ProgramExecutorResult program_executor_start_preset(const Preset* preset);
const char* program_executor_result_name(ProgramExecutorResult result);
