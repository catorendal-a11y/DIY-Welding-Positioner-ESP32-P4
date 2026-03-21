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
  STATE_TIMER,         // Timer mode (run for duration)
  STATE_STOPPING,      // Decelerating to stop
  STATE_ESTOP          // Emergency stop activated
} SystemState;

// ───────────────────────────────────────────────────────────────────────────────
// STATE MACHINE FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void control_init();                     // Initialize state machine
void control_transition_to(SystemState s); // State transition with validation
SystemState control_get_state();         // Get current state
const char* control_get_state_string(); // Get human-readable state name
const char* control_state_name(SystemState s); // Internal: state enum to string

// ───────────────────────────────────────────────────────────────────────────────
// MODE CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
// Continuous mode
void control_start_continuous();
void control_stop();

// Pulse mode
void control_start_pulse(uint32_t on_ms, uint32_t off_ms);

// Step mode
void control_start_step(float angle_deg);
void control_reset_step_accumulator();
float control_get_step_accumulated();   // For UI display
long control_get_step_count();           // For UI display

// Jog mode
void control_start_jog_cw();
void control_start_jog_ccw();
void control_stop_jog();
void control_set_jog_speed(float rpm);
float control_get_jog_speed();

// Timer mode
void control_start_timer(uint32_t duration_sec);
uint32_t control_get_timer_remaining();

// ───────────────────────────────────────────────────────────────────────────────
// FREERTOS TASKS
// ───────────────────────────────────────────────────────────────────────────────
void controlTask(void* pvParameters);   // Main control logic task
void safetyTask(void* pvParameters);    // ESTOP monitoring (Phase 3)
void motorTask(void* pvParameters);     // Motor speed updates
void ioTask(void* pvParameters);        // ADC and input polling
void storageTask(void* pvParameters);   // Program save/load (Phase 5)
