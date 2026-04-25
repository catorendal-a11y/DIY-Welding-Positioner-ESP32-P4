// TIG Rotator Controller - State Machine and Control Logic
// System states, mode transitions, FreeRTOS task coordination

#pragma once
#include <Arduino.h>

// ───────────────────────────────────────────────────────────────────────────────
// SYSTEM STATE ENUM
// ───────────────────────────────────────────────────────────────────────────────
typedef enum {
  STATE_IDLE = 0,      // Motor stopped, ready for command
  STATE_RUNNING,       // Continuous rotation mode
  STATE_PULSE,         // Pulse mode (rotate/pause cycles)
  STATE_STEP,          // Step mode (fixed angle steps)
  STATE_JOG,           // Jog mode (run while button held)
  STATE_STOPPING,      // Decelerating to stop
  STATE_ESTOP          // Emergency stop activated
} SystemState;

// ───────────────────────────────────────────────────────────────────────────────
// STATE MACHINE FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void control_init();                     // Initialize state machine
bool control_transition_to(SystemState s); // State transition with validation
SystemState control_get_state();         // Get current state
const char* control_get_state_string(); // Get human-readable state name
const char* control_state_name(SystemState s); // Internal: state enum to string

// ───────────────────────────────────────────────────────────────────────────────
// MODE CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
// Continuous mode
bool control_start_continuous(bool soft_start = false, uint32_t auto_stop_ms = 0);
bool control_stop();

// Pulse mode (cycles=0 means infinite)
bool control_start_pulse(uint32_t on_ms, uint32_t off_ms, uint16_t cycles = 0);

// Step mode
bool control_start_step(float angle_deg);
bool control_start_step_sequence(float angle_deg, uint16_t repeats, float dwell_sec);
void control_reset_step_accumulator();
float control_get_step_accumulated();   // For UI display
long control_get_step_count();           // For UI display

// Jog mode
bool control_start_jog_cw();
bool control_start_jog_ccw();
bool control_stop_jog();
void control_set_jog_speed(float rpm);
float control_get_jog_speed();

// ───────────────────────────────────────────────────────────────────────────────
// FREERTOS TASKS
// ───────────────────────────────────────────────────────────────────────────────
void controlTask(void* pvParameters);   // Main control logic task
