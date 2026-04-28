// TIG Rotator Controller - Comprehensive Native Unit Tests
// Tests ALL pure-logic functions without hardware dependencies
// Run: pio test -e native

#include <unity.h>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <climits>
#include <cstdio>

// Pull in testable pure-logic headers (no Arduino/LVGL/FreeRTOS deps)
#include "../../src/control/test_logic.h"
#include "../../src/motor/test_speed_logic.h"

// Unity requires these even if empty
void setUp(void) {}
void tearDown(void) {}

// ───────────────────────────────────────────────────────────────────────────────
// HARDWARE CONSTANTS (mirror of config.h for native builds)
// ───────────────────────────────────────────────────────────────────────────────
static const float GEAR_RATIO     = 60.0f * 72.0f / 40.0f;  // 108
static const float D_EMNE         = 0.300f;   // Workpiece diameter 300mm
static const float D_RULLE        = 0.080f;   // Roller diameter 80mm
static const float DIAMETER_RATIO = D_EMNE / D_RULLE;  // 3.75
static const float MIN_RPM        = 0.001f;
static const float MAX_RPM        = 3.0f;
static const float ADC_REF        = 3315.0f;
static const float POT_ADC_SNAP_TEST_BAND = 380.0f;  // explicit snap for unit tests (production often 0)
static const uint32_t START_SPEED_HZ = 20;
static const uint32_t STEPS_1_4   = 800;      // 200 * 4 (1/4 microstepping)
static const uint32_t STEPS_1_8   = 1600;     // 200 * 8
static const uint32_t STEPS_1_16  = 3200;     // 200 * 16
static const uint32_t STEPS_1_32  = 6400;     // 200 * 32

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 1: STATE TRANSITION MATRIX — FULL 7x7 (49 COMBINATIONS)
// Every possible (from, to) pair tested explicitly
// ═══════════════════════════════════════════════════════════════════════════════

// --- From IDLE (7 tests) ---
void test_trans_idle_to_idle() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_IDLE, STATE_IDLE));
}
void test_trans_idle_to_running() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_IDLE, STATE_RUNNING));
}
void test_trans_idle_to_pulse() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_IDLE, STATE_PULSE));
}
void test_trans_idle_to_step() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_IDLE, STATE_STEP));
}
void test_trans_idle_to_jog() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_IDLE, STATE_JOG));
}
void test_trans_idle_to_stopping() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_IDLE, STATE_STOPPING));
}
void test_trans_idle_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_IDLE, STATE_ESTOP));
}

// --- From RUNNING (7 tests) ---
void test_trans_running_to_idle() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_RUNNING, STATE_IDLE));
}
void test_trans_running_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_RUNNING, STATE_RUNNING));
}
void test_trans_running_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_RUNNING, STATE_PULSE));
}
void test_trans_running_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_RUNNING, STATE_STEP));
}
void test_trans_running_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_RUNNING, STATE_JOG));
}
void test_trans_running_to_stopping() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_RUNNING, STATE_STOPPING));
}
void test_trans_running_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_RUNNING, STATE_ESTOP));
}

// --- From PULSE (7 tests) ---
void test_trans_pulse_to_idle() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_PULSE, STATE_IDLE));
}
void test_trans_pulse_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_PULSE, STATE_RUNNING));
}
void test_trans_pulse_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_PULSE, STATE_PULSE));
}
void test_trans_pulse_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_PULSE, STATE_STEP));
}
void test_trans_pulse_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_PULSE, STATE_JOG));
}
void test_trans_pulse_to_stopping() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_PULSE, STATE_STOPPING));
}
void test_trans_pulse_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_PULSE, STATE_ESTOP));
}

// --- From STEP (7 tests) ---
void test_trans_step_to_idle() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STEP, STATE_IDLE));
}
void test_trans_step_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STEP, STATE_RUNNING));
}
void test_trans_step_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STEP, STATE_PULSE));
}
void test_trans_step_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STEP, STATE_STEP));
}
void test_trans_step_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STEP, STATE_JOG));
}
void test_trans_step_to_stopping() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_STEP, STATE_STOPPING));
}
void test_trans_step_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_STEP, STATE_ESTOP));
}

// --- From JOG (7 tests) ---
void test_trans_jog_to_idle() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_JOG, STATE_IDLE));
}
void test_trans_jog_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_JOG, STATE_RUNNING));
}
void test_trans_jog_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_JOG, STATE_PULSE));
}
void test_trans_jog_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_JOG, STATE_STEP));
}
void test_trans_jog_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_JOG, STATE_JOG));
}
void test_trans_jog_to_stopping() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_JOG, STATE_STOPPING));
}
void test_trans_jog_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_JOG, STATE_ESTOP));
}

// --- From STOPPING (7 tests) ---
void test_trans_stopping_to_idle() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_STOPPING, STATE_IDLE));
}
void test_trans_stopping_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STOPPING, STATE_RUNNING));
}
void test_trans_stopping_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STOPPING, STATE_PULSE));
}
void test_trans_stopping_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STOPPING, STATE_STEP));
}
void test_trans_stopping_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STOPPING, STATE_JOG));
}
void test_trans_stopping_to_stopping() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_STOPPING, STATE_STOPPING));
}
void test_trans_stopping_to_estop() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_STOPPING, STATE_ESTOP));
}

// --- From ESTOP (7 tests) ---
void test_trans_estop_to_idle() {
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_ESTOP, STATE_IDLE));
}
void test_trans_estop_to_running() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_ESTOP, STATE_RUNNING));
}
void test_trans_estop_to_pulse() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_ESTOP, STATE_PULSE));
}
void test_trans_estop_to_step() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_ESTOP, STATE_STEP));
}
void test_trans_estop_to_jog() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_ESTOP, STATE_JOG));
}
void test_trans_estop_to_stopping() {
  TEST_ASSERT_FALSE(control_is_valid_transition(STATE_ESTOP, STATE_STOPPING));
}
void test_trans_estop_to_estop() {
  // Any -> ESTOP is always valid (safety invariant)
  TEST_ASSERT_TRUE(control_is_valid_transition(STATE_ESTOP, STATE_ESTOP));
}

// --- ANY -> ESTOP invariant (parametric) ---
void test_trans_any_to_estop_invariant() {
  SystemState all[] = {STATE_IDLE, STATE_RUNNING, STATE_PULSE,
                       STATE_STEP, STATE_JOG, STATE_STOPPING, STATE_ESTOP};
  for (int i = 0; i < 7; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%s -> ESTOP", control_state_name(all[i]));
    TEST_ASSERT_TRUE_MESSAGE(
      control_is_valid_transition(all[i], STATE_ESTOP), msg);
  }
}

// --- ESTOP -> only IDLE (parametric) ---
void test_trans_estop_only_idle_or_estop() {
  SystemState all[] = {STATE_RUNNING, STATE_PULSE, STATE_STEP,
                       STATE_JOG, STATE_STOPPING};
  for (int i = 0; i < 5; i++) {
    char msg[32];
    snprintf(msg, sizeof(msg), "ESTOP -> %s", control_state_name(all[i]));
    TEST_ASSERT_FALSE_MESSAGE(
      control_is_valid_transition(STATE_ESTOP, all[i]), msg);
  }
}

// --- Out-of-range enum values ---
void test_trans_invalid_enum_from() {
  // Out-of-range 'from' state should return false for non-ESTOP targets
  TEST_ASSERT_FALSE(control_is_valid_transition((SystemState)99, STATE_IDLE));
  TEST_ASSERT_FALSE(control_is_valid_transition((SystemState)99, STATE_RUNNING));
}
void test_trans_invalid_enum_to_estop() {
  // Even from an invalid state, ESTOP target is always valid
  TEST_ASSERT_TRUE(control_is_valid_transition((SystemState)99, STATE_ESTOP));
}
void test_trans_negative_enum() {
  TEST_ASSERT_FALSE(control_is_valid_transition((SystemState)-1, STATE_IDLE));
  TEST_ASSERT_TRUE(control_is_valid_transition((SystemState)-1, STATE_ESTOP));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2: STATE NAME — ALL 7 STATES + EDGE CASES
// ═══════════════════════════════════════════════════════════════════════════════
void test_state_name_idle() {
  TEST_ASSERT_EQUAL_STRING("IDLE", control_state_name(STATE_IDLE));
}
void test_state_name_running() {
  TEST_ASSERT_EQUAL_STRING("RUNNING", control_state_name(STATE_RUNNING));
}
void test_state_name_pulse() {
  TEST_ASSERT_EQUAL_STRING("PULSE", control_state_name(STATE_PULSE));
}
void test_state_name_step() {
  TEST_ASSERT_EQUAL_STRING("STEP", control_state_name(STATE_STEP));
}
void test_state_name_jog() {
  TEST_ASSERT_EQUAL_STRING("JOG", control_state_name(STATE_JOG));
}
void test_state_name_stopping() {
  TEST_ASSERT_EQUAL_STRING("STOPPING", control_state_name(STATE_STOPPING));
}
void test_state_name_estop() {
  TEST_ASSERT_EQUAL_STRING("ESTOP", control_state_name(STATE_ESTOP));
}
void test_state_name_unknown_99() {
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", control_state_name((SystemState)99));
}
void test_state_name_unknown_neg() {
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", control_state_name((SystemState)-1));
}
void test_state_name_unknown_255() {
  TEST_ASSERT_EQUAL_STRING("UNKNOWN", control_state_name((SystemState)255));
}
void test_state_name_returns_nonnull() {
  // Every possible state name is a valid C string (not null)
  for (int i = -1; i < 20; i++) {
    const char* name = control_state_name((SystemState)i);
    TEST_ASSERT_NOT_NULL(name);
    TEST_ASSERT_TRUE(strlen(name) > 0);
  }
}

void test_control_request_is_jog() {
  TEST_ASSERT_FALSE(control_request_is_jog_testable(MODE_REQ_NONE_TESTABLE));
  TEST_ASSERT_FALSE(control_request_is_jog_testable(MODE_REQ_CONTINUOUS_TESTABLE));
  TEST_ASSERT_FALSE(control_request_is_jog_testable(MODE_REQ_PULSE_TESTABLE));
  TEST_ASSERT_FALSE(control_request_is_jog_testable(MODE_REQ_STEP_TESTABLE));
  TEST_ASSERT_TRUE(control_request_is_jog_testable(MODE_REQ_JOG_CW_TESTABLE));
  TEST_ASSERT_TRUE(control_request_is_jog_testable(MODE_REQ_JOG_CCW_TESTABLE));
}

void test_control_cancel_jog_request() {
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_NONE_TESTABLE,
                          control_cancel_jog_request_testable(MODE_REQ_JOG_CW_TESTABLE));
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_NONE_TESTABLE,
                          control_cancel_jog_request_testable(MODE_REQ_JOG_CCW_TESTABLE));
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_CONTINUOUS_TESTABLE,
                          control_cancel_jog_request_testable(MODE_REQ_CONTINUOUS_TESTABLE));
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_PULSE_TESTABLE,
                          control_cancel_jog_request_testable(MODE_REQ_PULSE_TESTABLE));
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_STEP_TESTABLE,
                          control_cancel_jog_request_testable(MODE_REQ_STEP_TESTABLE));
}

void test_control_mailbox_latest_command_wins() {
  TEST_ASSERT_EQUAL_UINT8(
      MOTION_CMD_STOP_TESTABLE,
      control_mailbox_overwrite_testable(MOTION_CMD_START_CONTINUOUS_TESTABLE,
                                         MOTION_CMD_STOP_TESTABLE));
  TEST_ASSERT_EQUAL_UINT8(
      MOTION_CMD_START_PULSE_TESTABLE,
      control_mailbox_overwrite_testable(MOTION_CMD_START_CONTINUOUS_TESTABLE,
                                         MOTION_CMD_START_PULSE_TESTABLE));
}

void test_control_start_command_waits_for_idle() {
  TEST_ASSERT_FALSE(control_start_command_waits_for_idle_testable(
      STATE_IDLE, MOTION_CMD_START_PULSE_TESTABLE));
  TEST_ASSERT_TRUE(control_start_command_waits_for_idle_testable(
      STATE_RUNNING, MOTION_CMD_START_PULSE_TESTABLE));
  TEST_ASSERT_TRUE(control_start_command_waits_for_idle_testable(
      STATE_STOPPING, MOTION_CMD_START_STEP_TESTABLE));
  TEST_ASSERT_FALSE(control_start_command_waits_for_idle_testable(
      STATE_RUNNING, MOTION_CMD_STOP_TESTABLE));
}

void test_continuous_auto_stop_request_from_preset_timer() {
  TEST_ASSERT_EQUAL_UINT32(30000, continuous_auto_stop_request_testable(true, 30000));
  TEST_ASSERT_EQUAL_UINT32(0, continuous_auto_stop_request_testable(false, 30000));
}

void test_soft_start_acceleration_profile() {
  TEST_ASSERT_EQUAL_UINT32(1000, soft_start_acceleration_testable(1000));
  TEST_ASSERT_EQUAL_UINT32(1875, soft_start_acceleration_testable(7500));
  TEST_ASSERT_EQUAL_UINT32(7500, soft_start_acceleration_testable(30000));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2b: PENDING REQUEST SAFETY
// ═══════════════════════════════════════════════════════════════════════════════

// --- ESTOP clears queued motion requests ---
void test_estop_clears_pending_continuous_start() {
  ControlPendingSnapshot s = {
    STATE_IDLE,
    MODE_REQ_CONTINUOUS_TESTABLE,
    false,
    false,
    0,
    0,
    0,
    0.0f
  };

  control_enter_estop_testable(s);

  TEST_ASSERT_EQUAL(STATE_ESTOP, s.state);
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_NONE_TESTABLE, s.mode_req);
  TEST_ASSERT_FALSE(s.stop);
  TEST_ASSERT_FALSE(s.stop_jog);
}

void test_estop_clears_all_pending_mode_parameters() {
  ControlPendingSnapshot s = {
    STATE_RUNNING,
    MODE_REQ_PULSE_TESTABLE,
    true,
    true,
    1200,
    800,
    7,
    45.0f
  };

  control_enter_estop_testable(s);

  TEST_ASSERT_EQUAL(STATE_ESTOP, s.state);
  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_NONE_TESTABLE, s.mode_req);
  TEST_ASSERT_FALSE(s.stop);
  TEST_ASSERT_FALSE(s.stop_jog);
  TEST_ASSERT_EQUAL_UINT32(0, s.pulse_on_ms);
  TEST_ASSERT_EQUAL_UINT32(0, s.pulse_off_ms);
  TEST_ASSERT_EQUAL_UINT16(0, s.pulse_cycles);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.step_angle);
}

void test_estop_tick_rejects_late_pending_start() {
  ControlPendingSnapshot s = {
    STATE_ESTOP,
    MODE_REQ_CONTINUOUS_TESTABLE,
    false,
    false,
    0,
    0,
    0,
    0.0f
  };

  control_process_estop_tick_testable(s);
  s.state = STATE_IDLE;

  TEST_ASSERT_EQUAL_UINT8(MODE_REQ_NONE_TESTABLE, s.mode_req);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 3: RPM TO STEP FREQUENCY
// ═══════════════════════════════════════════════════════════════════════════════

// --- Zero and boundary ---
void test_rpm_zero() {
  float hz = rpmToStepHz_testable(0.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, hz);
}

void test_rpm_min() {
  float hz = rpmToStepHz_testable(MIN_RPM, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_TRUE(hz > 0.0f);
  float expected = MIN_RPM * GEAR_RATIO * DIAMETER_RATIO * (float)STEPS_1_8 / 60.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, hz);
}

void test_rpm_max() {
  float hz = rpmToStepHz_testable(MAX_RPM, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float expected = MAX_RPM * GEAR_RATIO * DIAMETER_RATIO * (float)STEPS_1_8 / 60.0f;
  TEST_ASSERT_FLOAT_WITHIN(2.0f, expected, hz);
  // ~64800 Hz at MAX_RPM=3 with 1/16 + default rollers (21600 Hz per 1.0 RPM workpiece)
  TEST_ASSERT_TRUE(hz > 31800.0f);
  TEST_ASSERT_TRUE(hz < 33000.0f);
}

void test_rpm_half() {
  float hz = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float expected = 0.5f * GEAR_RATIO * DIAMETER_RATIO * (float)STEPS_1_8 / 60.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, hz);
}

// --- Linearity ---
void test_rpm_linearity_2x() {
  float hz1 = rpmToStepHz_testable(0.25f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float hz2 = rpmToStepHz_testable(0.50f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz1 * 2.0f, hz2);
}

void test_rpm_linearity_4x() {
  float hz1 = rpmToStepHz_testable(0.25f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float hz4 = rpmToStepHz_testable(1.00f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, hz1 * 4.0f, hz4);
}

void test_rpm_linearity_10x() {
  float hz1 = rpmToStepHz_testable(0.1f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float hz10 = rpmToStepHz_testable(1.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(2.0f, hz1 * 10.0f, hz10);
}

// --- Microstepping proportionality ---
void test_rpm_microstep_4_to_8() {
  float hz4 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_4);
  float hz8 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz4 * 2.0f, hz8);
}

void test_rpm_microstep_8_to_16() {
  float hz8 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float hz16 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_16);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz8 * 2.0f, hz16);
}

void test_rpm_microstep_16_to_32() {
  float hz16 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_16);
  float hz32 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_32);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz16 * 2.0f, hz32);
}

void test_rpm_microstep_4_to_32() {
  float hz4 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_4);
  float hz32 = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_32);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, hz4 * 8.0f, hz32);
}

// --- Diameter ratio effects ---
void test_rpm_larger_workpiece() {
  // Larger workpiece = higher step frequency (more surface to cover)
  float hz_small = rpmToStepHz_testable(0.5f, GEAR_RATIO, 0.200f, D_RULLE, STEPS_1_8);
  float hz_large = rpmToStepHz_testable(0.5f, GEAR_RATIO, 0.400f, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz_small * 2.0f, hz_large);
}

void test_rpm_equal_diameters() {
  // When workpiece = roller, ratio = 1.0
  float hz = rpmToStepHz_testable(1.0f, GEAR_RATIO, 0.100f, 0.100f, STEPS_1_8);
  float expected = 1.0f * GEAR_RATIO * 1.0f * (float)STEPS_1_8 / 60.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, hz);
}

// --- Edge cases ---
void test_rpm_negative() {
  float hz = rpmToStepHz_testable(-0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  // Negative RPM produces negative Hz (direction handled separately)
  TEST_ASSERT_TRUE(hz < 0.0f);
}

void test_rpm_very_large() {
  // 100 RPM (way beyond MAX_RPM but formula should still work)
  float hz = rpmToStepHz_testable(100.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_32);
  TEST_ASSERT_TRUE(hz > 1000000.0f);  // ~6.4M Hz
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 4: REVERSE RPM CONVERSION (Hz -> RPM)
// ═══════════════════════════════════════════════════════════════════════════════
void test_reverse_rpm_zero() {
  float rpm = stepHzToRpm_testable(0, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, rpm);
}

void test_reverse_rpm_roundtrip() {
  // Forward: RPM -> Hz, then reverse: Hz -> RPM. Should match.
  float original_rpm = 0.5f;
  float hz = rpmToStepHz_testable(original_rpm, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float recovered_rpm = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  // Integer truncation of Hz causes slight error
  TEST_ASSERT_FLOAT_WITHIN(0.001f, original_rpm, recovered_rpm);
}

void test_reverse_rpm_with_cal_factor() {
  // If cal_factor > 1.0 (e.g. 1.2), workpiece rotates *slower* for the same Hz,
  // so the resulting RPM should be lower.
  float original_rpm = 0.5f;
  float hz = rpmToStepHz_testable(original_rpm, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8); // Target 0.5 RPM
  // We feed that Hz into reverse with cal_factor=1.2
  float rev = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.2f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, original_rpm / 1.2f, rev);
}

void test_calibrated_motor_command_matches_display_rpm() {
  // Mirrors rpmToStepHzCalibrated + speed_get_actual_rpm: command C with cal K uses
  // Hz = rpmToStepHz(C*K); displayed RPM = geometric_rpm_from_Hz / K ~= C
  float cal = 1.27f;
  float cmd = 1.0f;
  float hz = rpmToStepHz_testable(cmd * cal, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float displayed = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, cal);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, cmd, displayed);
}

void test_reverse_rpm_at_min() {
  float hz = rpmToStepHz_testable(MIN_RPM, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float rpm = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, MIN_RPM, rpm);
}

void test_reverse_rpm_at_max() {
  float hz = rpmToStepHz_testable(MAX_RPM, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float rpm = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, rpm);
}

void test_reverse_rpm_all_microsteps() {
  uint32_t usteps[] = {STEPS_1_4, STEPS_1_8, STEPS_1_16, STEPS_1_32};
  for (int i = 0; i < 4; i++) {
    float hz = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, usteps[i]);
    float rpm = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, usteps[i]);
    char msg[48];
    snprintf(msg, sizeof(msg), "steps=%u", usteps[i]);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.005f, 0.5f, rpm, msg);
  }
}

void test_reverse_rpm_with_calibration() {
  float original_rpm = 0.5f;
  float cal = 1.1f;
  float hz = rpmToStepHz_testable(original_rpm, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float recovered = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, cal);
  TEST_ASSERT_TRUE(recovered < original_rpm);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, original_rpm / cal, recovered);
}

void test_reverse_rpm_cal_unity_matches_no_cal() {
  float hz = rpmToStepHz_testable(0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float rpm_no_cal = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float rpm_unity = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, rpm_no_cal, rpm_unity);
}

void test_reverse_rpm_cal_below_1() {
  float original_rpm = 0.5f;
  float cal = 0.9f;
  float hz = rpmToStepHz_testable(original_rpm, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float recovered = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, cal);
  TEST_ASSERT_TRUE(recovered > original_rpm);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, original_rpm / cal, recovered);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 5: ANGLE TO STEPS
// ═══════════════════════════════════════════════════════════════════════════════
void test_angle_zero() {
  long steps = angleToSteps_testable(0.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_EQUAL(0, steps);
}

void test_angle_full_rotation_range() {
  long steps = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  // Same as 1 workpiece RPM for 60 s: GEAR * (D_EMNE/D_RULLE) * steps_per_rev
  float expected = 360.0f * GEAR_RATIO * (D_EMNE / D_RULLE) / 360.0f * (float)STEPS_1_8;
  TEST_ASSERT_INT_WITHIN(2, (long)expected, steps);
}

void test_angle_quarter_rotation() {
  long full = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long quarter = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_INT_WITHIN(2, full / 4, quarter);
}

void test_angle_half_rotation() {
  long full = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long half = angleToSteps_testable(180.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_INT_WITHIN(2, full / 2, half);
}

void test_angle_one_degree() {
  long steps = angleToSteps_testable(1.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  // Should be roughly full / 360
  long full = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_INT_WITHIN(2, full / 360, steps);
}

void test_angle_small_01() {
  long steps = angleToSteps_testable(0.1f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_TRUE(steps > 0);
}

void test_angle_negative() {
  // Negative angle should produce negative steps (motor reverses)
  long steps = angleToSteps_testable(-90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_TRUE(steps < 0);
}

void test_angle_large_720() {
  // Two full rotations
  long steps_360 = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long steps_720 = angleToSteps_testable(720.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_INT_WITHIN(2, steps_360 * 2, steps_720);
}

// --- Microstepping proportionality ---
void test_angle_microstep_4_to_8() {
  long s4 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_4, 1.0f);
  long s8 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_INT_WITHIN(1, s4 * 2, s8);
}

void test_angle_microstep_8_to_16() {
  long s8 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long s16 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_16, 1.0f);
  TEST_ASSERT_INT_WITHIN(1, s8 * 2, s16);
}

void test_angle_microstep_16_to_32() {
  long s16 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_16, 1.0f);
  long s32 = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_32, 1.0f);
  TEST_ASSERT_INT_WITHIN(1, s16 * 2, s32);
}

// --- Calibration factor ---
void test_angle_cal_unity() {
  long base = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long same = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  TEST_ASSERT_EQUAL(base, same);
}

void test_angle_cal_1_1() {
  long base = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long adj = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.1f);
  float ratio = (float)adj / (float)base;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.1f, ratio);
}

void test_angle_cal_0_9() {
  long base = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long adj = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 0.9f);
  TEST_ASSERT_TRUE(adj < base);
  float ratio = (float)adj / (float)base;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.9f, ratio);
}

void test_angle_cal_extreme_0_5() {
  long base = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long adj = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 0.5f);
  float ratio = (float)adj / (float)base;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f, ratio);
}

void test_angle_cal_extreme_1_5() {
  long base = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long adj = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.5f);
  float ratio = (float)adj / (float)base;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.5f, ratio);
}

void test_angle_cal_zero_gives_zero() {
  long steps = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 0.0f);
  TEST_ASSERT_EQUAL(0, steps);
}

void test_angle_cal_roundtrip_with_apply() {
  // calibration_apply_steps(steps, cal) == angleToSteps(angle, cal)
  // Both multiply the base steps by the calibration factor
  float cal = 1.2f;
  long steps_via_angle = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, cal);
  long base_steps = angleToSteps_testable(90.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  long steps_via_apply = calibration_apply_steps_testable(base_steps, cal);
  TEST_ASSERT_INT_WITHIN(1, steps_via_apply, steps_via_angle);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 6: ADC TO RPM NORMALIZATION
// ═══════════════════════════════════════════════════════════════════════════════
void test_adc_at_zero() {
  // ADC=0 = pot fully CCW = maximum normalized value = MAX_RPM
  float rpm = adcToRpm_testable(0.0f, ADC_REF, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, rpm);
}

void test_adc_at_ref() {
  // ADC=3315 = pot fully CW = normalized=0 = MIN_RPM
  float rpm = adcToRpm_testable(ADC_REF, ADC_REF, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MIN_RPM, rpm);
}

void test_adc_at_half_ref() {
  // ADC=1657.5 = half range
  float rpm = adcToRpm_testable(ADC_REF / 2.0f, ADC_REF, MIN_RPM, MAX_RPM);
  float expected = MIN_RPM + 0.5f * (MAX_RPM - MIN_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, rpm);
}

void test_adc_above_ref() {
  // ADC>3315 should clamp normalized to 0 -> MIN_RPM
  float rpm = adcToRpm_testable(4000.0f, ADC_REF, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MIN_RPM, rpm);
}

void test_adc_at_4095() {
  // Maximum ADC value (12-bit)
  float rpm = adcToRpm_testable(4095.0f, ADC_REF, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MIN_RPM, rpm);
}

void test_adc_negative() {
  // ADC below 0 clamped to 0 -> MAX_RPM
  float rpm = adcToRpm_testable(-100.0f, ADC_REF, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, rpm);
}

void test_adc_never_below_min_rpm() {
  // No matter what ADC value, result is always >= MIN_RPM
  float values[] = {0.0f, 1000.0f, 2000.0f, 3315.0f, 4095.0f, -50.0f, 10000.0f};
  for (int i = 0; i < 7; i++) {
    float rpm = adcToRpm_testable(values[i], ADC_REF, MIN_RPM, MAX_RPM);
    TEST_ASSERT_TRUE(rpm >= MIN_RPM - 0.001f);
    TEST_ASSERT_TRUE(rpm <= MAX_RPM + 0.001f);
  }
}

void test_adc_monotonic() {
  // Higher ADC = lower RPM (inverted pot)
  float prev = adcToRpm_testable(0.0f, ADC_REF, MIN_RPM, MAX_RPM);
  for (int adc = 100; adc <= 3300; adc += 100) {
    float rpm = adcToRpm_testable((float)adc, ADC_REF, MIN_RPM, MAX_RPM);
    TEST_ASSERT_TRUE(rpm <= prev + 0.001f);  // Must decrease or stay same
    prev = rpm;
  }
}

void test_adc_snap_zone_reaches_max_rpm() {
  // Optional snap band: ADC inside band maps to max (EMI helper when enabled in config)
  float rpm = adcToRpm_testable(300.0f, ADC_REF, MIN_RPM, MAX_RPM, POT_ADC_SNAP_TEST_BAND);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, rpm);
}

void test_adc_snap_running_threshold_covers_emi() {
  // Snap off: full linear curve toward top (matches default firmware).
  float mid = adcToRpm_testable(350.0f, ADC_REF, MIN_RPM, MAX_RPM, 0.0f);
  float want = MIN_RPM + (ADC_REF - 350.0f) / ADC_REF * (MAX_RPM - MIN_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, want, mid);
  float hi = adcToRpm_testable(900.0f, ADC_REF, MIN_RPM, MAX_RPM, 0.0f);
  float want_hi = MIN_RPM + (ADC_REF - 900.0f) / ADC_REF * (MAX_RPM - MIN_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, want_hi, hi);
}

void test_adc_snap_zone_monotonic() {
  float prev = adcToRpm_testable(0.0f, ADC_REF, MIN_RPM, MAX_RPM, POT_ADC_SNAP_TEST_BAND);
  for (int adc = 100; adc <= 3300; adc += 100) {
    float rpm = adcToRpm_testable((float)adc, ADC_REF, MIN_RPM, MAX_RPM, POT_ADC_SNAP_TEST_BAND);
    TEST_ASSERT_TRUE(rpm <= prev + 0.001f);
    prev = rpm;
  }
}

void test_adc_snap_off_linear_at_mid_high_adc() {
  float rpm = adcToRpm_testable(350.0f, ADC_REF, MIN_RPM, MAX_RPM, 0.0f);
  float expected = MIN_RPM + (ADC_REF - 350.0f) / ADC_REF * (MAX_RPM - MIN_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, rpm);
}

// Pot span is MIN_RPM .. speed_get_rpm_max() (NVS / Motor Config), not always MAX_RPM.
void test_adc_user_rpm_cap_not_hardcoded_max() {
  const float cap = 0.5f;
  float at_snap = adcToRpm_testable(300.0f, ADC_REF, MIN_RPM, cap, POT_ADC_SNAP_TEST_BAND);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, cap, at_snap);
  float at_zero = adcToRpm_testable(0.0f, ADC_REF, MIN_RPM, cap, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, cap, at_zero);
  float mid = adcToRpm_testable(ADC_REF / 2.0f, ADC_REF, MIN_RPM, cap, 0.0f);
  float want_mid = MIN_RPM + 0.5f * (cap - MIN_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, want_mid, mid);
  float at_ref = adcToRpm_testable(ADC_REF, ADC_REF, MIN_RPM, cap, 0.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MIN_RPM, at_ref);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 7: CALIBRATION LOGIC
// ═══════════════════════════════════════════════════════════════════════════════
void test_cal_apply_steps_unity() {
  TEST_ASSERT_EQUAL(1000, calibration_apply_steps_testable(1000, 1.0f));
}

void test_cal_apply_steps_above() {
  TEST_ASSERT_EQUAL(1100, calibration_apply_steps_testable(1000, 1.1f));
}

void test_cal_apply_steps_below() {
  TEST_ASSERT_EQUAL(900, calibration_apply_steps_testable(1000, 0.9f));
}

void test_cal_apply_steps_zero() {
  TEST_ASSERT_EQUAL(0, calibration_apply_steps_testable(0, 1.5f));
}

void test_cal_apply_steps_negative() {
  TEST_ASSERT_EQUAL(-1000, calibration_apply_steps_testable(-1000, 1.0f));
}

void test_cal_apply_angle_unity() {
  float angle = calibration_apply_angle_testable(90.0f, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, angle);
}

void test_cal_apply_angle_above() {
  // Factor > 1 means fewer displayed degrees per actual degree
  float angle = calibration_apply_angle_testable(90.0f, 1.1f);
  TEST_ASSERT_TRUE(angle < 90.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f / 1.1f, angle);
}

void test_cal_validate_in_range() {
  TEST_ASSERT_TRUE(calibration_validate_testable(0.5f));
  TEST_ASSERT_TRUE(calibration_validate_testable(1.0f));
  TEST_ASSERT_TRUE(calibration_validate_testable(1.5f));
  TEST_ASSERT_TRUE(calibration_validate_testable(0.75f));
  TEST_ASSERT_TRUE(calibration_validate_testable(1.25f));
}

void test_cal_validate_out_of_range() {
  TEST_ASSERT_FALSE(calibration_validate_testable(0.0f));
  TEST_ASSERT_FALSE(calibration_validate_testable(0.49f));
  TEST_ASSERT_FALSE(calibration_validate_testable(1.51f));
  TEST_ASSERT_FALSE(calibration_validate_testable(2.0f));
  TEST_ASSERT_FALSE(calibration_validate_testable(-1.0f));
}

void test_cal_validate_reset() {
  // Valid stays valid
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_validate_with_reset_testable(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, calibration_validate_with_reset_testable(1.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calibration_validate_with_reset_testable(0.5f));
  
  // Invalid resets to 1.0f
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_validate_with_reset_testable(0.49f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_validate_with_reset_testable(1.51f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_validate_with_reset_testable(9.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_validate_with_reset_testable(-1.0f));
}

void test_cal_clamp_in_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.75f, calibration_clamp_testable(0.75f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, calibration_clamp_testable(1.0f));
}

void test_cal_clamp_below() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calibration_clamp_testable(0.1f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, calibration_clamp_testable(-1.0f));
}

void test_cal_clamp_above() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, calibration_clamp_testable(2.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, calibration_clamp_testable(100.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 8: MICROSTEP LOGIC
// ═══════════════════════════════════════════════════════════════════════════════
void test_microstep_spr_4() {
  TEST_ASSERT_EQUAL(800, microstep_steps_per_rev_testable(4));
}

void test_microstep_spr_8() {
  TEST_ASSERT_EQUAL(1600, microstep_steps_per_rev_testable(8));
}

void test_microstep_spr_16() {
  TEST_ASSERT_EQUAL(3200, microstep_steps_per_rev_testable(16));
}

void test_microstep_spr_32() {
  TEST_ASSERT_EQUAL(6400, microstep_steps_per_rev_testable(32));
}

void test_microstep_valid_values() {
  TEST_ASSERT_TRUE(microstep_is_valid_testable(4));
  TEST_ASSERT_TRUE(microstep_is_valid_testable(8));
  TEST_ASSERT_TRUE(microstep_is_valid_testable(16));
  TEST_ASSERT_TRUE(microstep_is_valid_testable(32));
}

void test_microstep_invalid_values() {
  TEST_ASSERT_FALSE(microstep_is_valid_testable(0));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(1));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(2));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(3));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(5));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(10));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(64));
  TEST_ASSERT_FALSE(microstep_is_valid_testable(128));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 9: ACCELERATION CLAMPING
// ═══════════════════════════════════════════════════════════════════════════════
void test_accel_in_range() {
  TEST_ASSERT_EQUAL(5000, acceleration_clamp_testable(5000));
  TEST_ASSERT_EQUAL(1000, acceleration_clamp_testable(1000));
  TEST_ASSERT_EQUAL(20000, acceleration_clamp_testable(20000));
  TEST_ASSERT_EQUAL(30000, acceleration_clamp_testable(30000));
  TEST_ASSERT_EQUAL(10000, acceleration_clamp_testable(10000));
}

void test_accel_below_min() {
  TEST_ASSERT_EQUAL(1000, acceleration_clamp_testable(0));
  TEST_ASSERT_EQUAL(1000, acceleration_clamp_testable(500));
  TEST_ASSERT_EQUAL(1000, acceleration_clamp_testable(999));
}

void test_accel_above_max() {
  TEST_ASSERT_EQUAL(30000, acceleration_clamp_testable(30001));
  TEST_ASSERT_EQUAL(30000, acceleration_clamp_testable(50000));
  TEST_ASSERT_EQUAL(30000, acceleration_clamp_testable(UINT32_MAX));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 10: DIRECTION INVERSION
// ═══════════════════════════════════════════════════════════════════════════════
void test_dir_cw_no_invert() {
  TEST_ASSERT_EQUAL(0, direction_with_invert_testable(0, false));  // CW stays CW
}

void test_dir_ccw_no_invert() {
  TEST_ASSERT_EQUAL(1, direction_with_invert_testable(1, false));  // CCW stays CCW
}

void test_dir_cw_with_invert() {
  TEST_ASSERT_EQUAL(1, direction_with_invert_testable(0, true));   // CW -> CCW
}

void test_dir_ccw_with_invert() {
  TEST_ASSERT_EQUAL(0, direction_with_invert_testable(1, true));   // CCW -> CW
}

void test_dir_double_invert() {
  int original = 0;  // CW
  int once = direction_with_invert_testable(original, true);      // -> CCW
  int twice = direction_with_invert_testable(once, true);         // -> CW
  TEST_ASSERT_EQUAL(original, twice);
}

void test_dir_program_override_wins_over_switch() {
  TEST_ASSERT_EQUAL(1, direction_source_testable(true, 1, true, true, 0, false));
  TEST_ASSERT_EQUAL(0, direction_source_testable(true, 0, true, false, 1, false));
}

void test_dir_switch_used_without_program_override() {
  TEST_ASSERT_EQUAL(0, direction_source_testable(false, 1, true, true, 1, false));
  TEST_ASSERT_EQUAL(1, direction_source_testable(false, 0, true, false, 0, false));
}

void test_dir_program_override_still_obeys_invert() {
  TEST_ASSERT_EQUAL(0, direction_source_testable(true, 1, true, false, 0, true));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 11: GEAR RATIO VERIFICATION
// ═══════════════════════════════════════════════════════════════════════════════
void test_gear_ratio_value() {
  // NMRV030 60:1 * spur 72/40 = 108 (motor revs per output rev)
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 108.0f, GEAR_RATIO);
}

void test_diameter_ratio_value() {
  // 300mm / 80mm = 3.75
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.75f, DIAMETER_RATIO);
}

void test_combined_ratio() {
  // GEAR_RATIO * DIAMETER_RATIO = 108 * 3.75 = 405
  float combined = GEAR_RATIO * DIAMETER_RATIO;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 405.0f, combined);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 12: POT OVERRIDE LOGIC
// ═══════════════════════════════════════════════════════════════════════════════
void test_pot_override_under_threshold() {
  // Threshold is usually 200.0f
  TEST_ASSERT_FALSE(pot_should_override_testable(2000.0f, 2000.0f, 200.0f));
  TEST_ASSERT_FALSE(pot_should_override_testable(2199.0f, 2000.0f, 200.0f));
  TEST_ASSERT_FALSE(pot_should_override_testable(1801.0f, 2000.0f, 200.0f));
}

void test_pot_override_at_threshold() {
  // Strict greater-than means exactly 200 difference doesn't trigger override
  TEST_ASSERT_FALSE(pot_should_override_testable(2200.0f, 2000.0f, 200.0f));
  TEST_ASSERT_FALSE(pot_should_override_testable(1800.0f, 2000.0f, 200.0f));
}

void test_pot_override_over_threshold() {
  TEST_ASSERT_TRUE(pot_should_override_testable(2201.0f, 2000.0f, 200.0f));
  TEST_ASSERT_TRUE(pot_should_override_testable(1799.0f, 2000.0f, 200.0f));
  TEST_ASSERT_TRUE(pot_should_override_testable(4000.0f, 2000.0f, 200.0f));
  TEST_ASSERT_TRUE(pot_should_override_testable(0.0f, 2000.0f, 200.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 13: CROSS-VALIDATION (RPM vs ANGLE consistency)
// ═══════════════════════════════════════════════════════════════════════════════
void test_rpm_angle_formula_relationship() {
  // 360 deg workpiece at 1 RPM for 60 s must equal integral of step Hz (cal = 1)
  float hz = rpmToStepHz_testable(1.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float steps_from_rpm = hz * 60.0f;
  long steps_from_angle = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8, 1.0f);
  float ratio = steps_from_rpm / (float)steps_from_angle;
  TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, ratio);
}

void test_rpm_angle_all_microsteps_consistent() {
  uint32_t usteps[] = {STEPS_1_4, STEPS_1_8, STEPS_1_16, STEPS_1_32};
  for (int i = 0; i < 4; i++) {
    float hz = rpmToStepHz_testable(1.0f, GEAR_RATIO, D_EMNE, D_RULLE, usteps[i]);
    float steps_rpm = hz * 60.0f;
    long steps_angle = angleToSteps_testable(360.0f, GEAR_RATIO, D_EMNE, D_RULLE, usteps[i], 1.0f);
    float ratio = steps_rpm / (float)steps_angle;
    char msg[48];
    snprintf(msg, sizeof(msg), "ustep=%u", usteps[i]);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.05f, 1.0f, ratio, msg);
  }
}

void test_reverse_rpm_inverse_of_forward() {
  float test_rpms[] = {MIN_RPM, 0.1f, 0.25f, 0.5f, 0.75f, MAX_RPM};
  for (int i = 0; i < 6; i++) {
    float hz = rpmToStepHz_testable(test_rpms[i], GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
    float recovered = stepHzToRpm_testable((uint32_t)hz, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
    char msg[48];
    snprintf(msg, sizeof(msg), "rpm=%.3f", test_rpms[i]);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, test_rpms[i], recovered, msg);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 14: MICROSTEP STRING MAPPING
// ═══════════════════════════════════════════════════════════════════════════════
void test_microstep_string_4() {
  TEST_ASSERT_EQUAL_STRING("1/4", microstep_get_string_testable(4));
}
void test_microstep_string_8() {
  TEST_ASSERT_EQUAL_STRING("1/8", microstep_get_string_testable(8));
}
void test_microstep_string_16() {
  TEST_ASSERT_EQUAL_STRING("1/16", microstep_get_string_testable(16));
}
void test_microstep_string_32() {
  TEST_ASSERT_EQUAL_STRING("1/32", microstep_get_string_testable(32));
}
void test_microstep_string_invalid() {
  TEST_ASSERT_EQUAL_STRING("1/16", microstep_get_string_testable(0));
  TEST_ASSERT_EQUAL_STRING("1/16", microstep_get_string_testable(1));
  TEST_ASSERT_EQUAL_STRING("1/16", microstep_get_string_testable(64));
  TEST_ASSERT_EQUAL_STRING("1/16", microstep_get_string_testable(255));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 15: ACCELERATION INIT DEFAULT
// ═══════════════════════════════════════════════════════════════════════════════
void test_accel_init_valid() {
  TEST_ASSERT_EQUAL(7500, acceleration_init_default_testable(7500));
  TEST_ASSERT_EQUAL(1000, acceleration_init_default_testable(1000));
  TEST_ASSERT_EQUAL(20000, acceleration_init_default_testable(20000));
  TEST_ASSERT_EQUAL(30000, acceleration_init_default_testable(30000));
}
void test_accel_init_below() {
  TEST_ASSERT_EQUAL(7500, acceleration_init_default_testable(0));
  TEST_ASSERT_EQUAL(7500, acceleration_init_default_testable(999));
}
void test_accel_init_above() {
  TEST_ASSERT_EQUAL(7500, acceleration_init_default_testable(30001));
  TEST_ASSERT_EQUAL(7500, acceleration_init_default_testable(UINT32_MAX));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 16: MICROSTEP INIT DEFAULT
// ═══════════════════════════════════════════════════════════════════════════════
void test_microstep_init_valid() {
  TEST_ASSERT_EQUAL(4, microstep_init_default_testable(4));
  TEST_ASSERT_EQUAL(8, microstep_init_default_testable(8));
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(16));
  TEST_ASSERT_EQUAL(32, microstep_init_default_testable(32));
}
void test_microstep_init_invalid() {
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(0));
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(1));
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(3));
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(5));
  TEST_ASSERT_EQUAL(16, microstep_init_default_testable(64));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 16b: MILLI-HZ START_SPEED FLOOR
// Mirrors motor_milli_hz_for_rpm_calibrated in motor.cpp
// ═══════════════════════════════════════════════════════════════════════════════
void test_milli_hz_zero_hz_clamps_to_floor() {
  uint32_t mhz = milli_hz_floor_testable(0.0f, 100u);
  TEST_ASSERT_EQUAL_UINT32(100u * 1000u, mhz);
}

void test_milli_hz_negative_hz_clamps_to_floor() {
  // Negative inputs must be treated as zero, then floored.
  uint32_t mhz = milli_hz_floor_testable(-5.0f, 100u);
  TEST_ASSERT_EQUAL_UINT32(100u * 1000u, mhz);
}

void test_milli_hz_nan_clamps_to_floor() {
  uint32_t mhz = milli_hz_floor_testable(NAN, 100u);
  TEST_ASSERT_EQUAL_UINT32(100u * 1000u, mhz);
}

void test_milli_hz_above_floor_passes_through() {
  // 5000 Hz = 5_000_000 mHz, well above 100*1000 floor.
  uint32_t mhz = milli_hz_floor_testable(5000.0f, 100u);
  TEST_ASSERT_EQUAL_UINT32(5000000u, mhz);
}

void test_milli_hz_just_at_floor() {
  // 100 Hz exactly = floor = 100_000 mHz.
  uint32_t mhz = milli_hz_floor_testable(100.0f, 100u);
  TEST_ASSERT_EQUAL_UINT32(100u * 1000u, mhz);
}

void test_milli_hz_just_below_floor_is_raised() {
  // 99 Hz = 99_000 mHz, below 100_000 floor → raised.
  uint32_t mhz = milli_hz_floor_testable(99.0f, 100u);
  TEST_ASSERT_EQUAL_UINT32(100u * 1000u, mhz);
}

void test_milli_hz_huge_value_saturates_to_u32_max() {
  // Very large Hz must not overflow — clamped to UINT32_MAX.
  uint32_t mhz = milli_hz_floor_testable(1e12f, 100u);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, mhz);
}

void test_milli_hz_different_floor_levels() {
  // Floor follows start_speed param.
  TEST_ASSERT_EQUAL_UINT32(50u * 1000u,
      milli_hz_floor_testable(10.0f, 50u));
  TEST_ASSERT_EQUAL_UINT32(200u * 1000u,
      milli_hz_floor_testable(10.0f, 200u));
}

void test_min_rpm_default_microstep_stays_above_start_floor() {
  float hz = rpmToStepHz_testable(MIN_RPM, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_16);
  TEST_ASSERT_TRUE(hz > (float)START_SPEED_HZ);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 17: PULSE TIME CLAMPS
// ═══════════════════════════════════════════════════════════════════════════════
void test_pulse_clamp_on_in_range() {
  TEST_ASSERT_EQUAL(500, pulse_clamp_on_ms_testable(500));
  TEST_ASSERT_EQUAL(100, pulse_clamp_on_ms_testable(100));
  TEST_ASSERT_EQUAL(10000, pulse_clamp_on_ms_testable(10000));
}
void test_pulse_clamp_on_below() {
  TEST_ASSERT_EQUAL(100, pulse_clamp_on_ms_testable(0));
  TEST_ASSERT_EQUAL(100, pulse_clamp_on_ms_testable(99));
}
void test_pulse_clamp_on_above() {
  TEST_ASSERT_EQUAL(10000, pulse_clamp_on_ms_testable(10001));
  TEST_ASSERT_EQUAL(10000, pulse_clamp_on_ms_testable(50000));
}
void test_pulse_clamp_off_in_range() {
  TEST_ASSERT_EQUAL(1000, pulse_clamp_off_ms_testable(1000));
  TEST_ASSERT_EQUAL(100, pulse_clamp_off_ms_testable(100));
  TEST_ASSERT_EQUAL(10000, pulse_clamp_off_ms_testable(10000));
}
void test_pulse_clamp_off_below() {
  TEST_ASSERT_EQUAL(100, pulse_clamp_off_ms_testable(0));
}
void test_pulse_clamp_off_above() {
  TEST_ASSERT_EQUAL(10000, pulse_clamp_off_ms_testable(10001));
  TEST_ASSERT_EQUAL(10000, pulse_clamp_off_ms_testable(50000));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 18: STEP ANGLE BOUNDS
// ═══════════════════════════════════════════════════════════════════════════════
void test_step_bounds_valid() {
  TEST_ASSERT_TRUE(step_angle_bounds_valid_testable(1.0f));
  TEST_ASSERT_TRUE(step_angle_bounds_valid_testable(90.0f));
  TEST_ASSERT_TRUE(step_angle_bounds_valid_testable(3600.0f));
}
void test_step_bounds_invalid() {
  TEST_ASSERT_FALSE(step_angle_bounds_valid_testable(0.0f));
  TEST_ASSERT_FALSE(step_angle_bounds_valid_testable(-1.0f));
  TEST_ASSERT_FALSE(step_angle_bounds_valid_testable(3601.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 19: SCREEN NEEDS REBUILD
// ═══════════════════════════════════════════════════════════════════════════════
void test_screen_rebuild_editor_screens() {
  TEST_ASSERT_TRUE(screen_needs_rebuild_testable(8));   // SCREEN_PROGRAM_EDIT
  TEST_ASSERT_TRUE(screen_needs_rebuild_testable(12));  // SCREEN_EDIT_PULSE
  TEST_ASSERT_TRUE(screen_needs_rebuild_testable(13));  // SCREEN_EDIT_STEP
  TEST_ASSERT_TRUE(screen_needs_rebuild_testable(14));  // SCREEN_EDIT_CONT
}
void test_screen_rebuild_non_editor() {
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(0));   // SCREEN_NONE
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(1));   // SCREEN_MAIN
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(3));   // SCREEN_PULSE
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(9));   // SCREEN_SETTINGS
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(11));  // SCREEN_BOOT
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(22));  // SCREEN_RUN_MODES
  TEST_ASSERT_FALSE(screen_needs_rebuild_testable(23));  // SCREEN_COUNT
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST RUNNER
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
  UNITY_BEGIN();

  // Section 1: State transition matrix — full 7x7 (49 + 5 parametric)
  RUN_TEST(test_trans_idle_to_idle);
  RUN_TEST(test_trans_idle_to_running);
  RUN_TEST(test_trans_idle_to_pulse);
  RUN_TEST(test_trans_idle_to_step);
  RUN_TEST(test_trans_idle_to_jog);
  RUN_TEST(test_trans_idle_to_stopping);
  RUN_TEST(test_trans_idle_to_estop);

  RUN_TEST(test_trans_running_to_idle);
  RUN_TEST(test_trans_running_to_running);
  RUN_TEST(test_trans_running_to_pulse);
  RUN_TEST(test_trans_running_to_step);
  RUN_TEST(test_trans_running_to_jog);
  RUN_TEST(test_trans_running_to_stopping);
  RUN_TEST(test_trans_running_to_estop);

  RUN_TEST(test_trans_pulse_to_idle);
  RUN_TEST(test_trans_pulse_to_running);
  RUN_TEST(test_trans_pulse_to_pulse);
  RUN_TEST(test_trans_pulse_to_step);
  RUN_TEST(test_trans_pulse_to_jog);
  RUN_TEST(test_trans_pulse_to_stopping);
  RUN_TEST(test_trans_pulse_to_estop);

  RUN_TEST(test_trans_step_to_idle);
  RUN_TEST(test_trans_step_to_running);
  RUN_TEST(test_trans_step_to_pulse);
  RUN_TEST(test_trans_step_to_step);
  RUN_TEST(test_trans_step_to_jog);
  RUN_TEST(test_trans_step_to_stopping);
  RUN_TEST(test_trans_step_to_estop);

  RUN_TEST(test_trans_jog_to_idle);
  RUN_TEST(test_trans_jog_to_running);
  RUN_TEST(test_trans_jog_to_pulse);
  RUN_TEST(test_trans_jog_to_step);
  RUN_TEST(test_trans_jog_to_jog);
  RUN_TEST(test_trans_jog_to_stopping);
  RUN_TEST(test_trans_jog_to_estop);

  RUN_TEST(test_trans_stopping_to_idle);
  RUN_TEST(test_trans_stopping_to_running);
  RUN_TEST(test_trans_stopping_to_pulse);
  RUN_TEST(test_trans_stopping_to_step);
  RUN_TEST(test_trans_stopping_to_jog);
  RUN_TEST(test_trans_stopping_to_stopping);
  RUN_TEST(test_trans_stopping_to_estop);

  RUN_TEST(test_trans_estop_to_idle);
  RUN_TEST(test_trans_estop_to_running);
  RUN_TEST(test_trans_estop_to_pulse);
  RUN_TEST(test_trans_estop_to_step);
  RUN_TEST(test_trans_estop_to_jog);
  RUN_TEST(test_trans_estop_to_stopping);
  RUN_TEST(test_trans_estop_to_estop);

  RUN_TEST(test_trans_any_to_estop_invariant);
  RUN_TEST(test_trans_estop_only_idle_or_estop);
  RUN_TEST(test_trans_invalid_enum_from);
  RUN_TEST(test_trans_invalid_enum_to_estop);
  RUN_TEST(test_trans_negative_enum);

  // Section 2: State names and request safety (18 tests)
  RUN_TEST(test_state_name_idle);
  RUN_TEST(test_state_name_running);
  RUN_TEST(test_state_name_pulse);
  RUN_TEST(test_state_name_step);
  RUN_TEST(test_state_name_jog);
  RUN_TEST(test_state_name_stopping);
  RUN_TEST(test_state_name_estop);
  RUN_TEST(test_state_name_unknown_99);
  RUN_TEST(test_state_name_unknown_neg);
  RUN_TEST(test_state_name_unknown_255);
  RUN_TEST(test_state_name_returns_nonnull);
  RUN_TEST(test_control_request_is_jog);
  RUN_TEST(test_control_cancel_jog_request);
  RUN_TEST(test_control_mailbox_latest_command_wins);
  RUN_TEST(test_control_start_command_waits_for_idle);
  RUN_TEST(test_continuous_auto_stop_request_from_preset_timer);
  RUN_TEST(test_soft_start_acceleration_profile);
  RUN_TEST(test_estop_clears_pending_continuous_start);
  RUN_TEST(test_estop_clears_all_pending_mode_parameters);
  RUN_TEST(test_estop_tick_rejects_late_pending_start);

  // Section 3: RPM to step Hz (16 tests)
  RUN_TEST(test_rpm_zero);
  RUN_TEST(test_rpm_min);
  RUN_TEST(test_rpm_max);
  RUN_TEST(test_rpm_half);
  RUN_TEST(test_rpm_linearity_2x);
  RUN_TEST(test_rpm_linearity_4x);
  RUN_TEST(test_rpm_linearity_10x);
  RUN_TEST(test_rpm_microstep_4_to_8);
  RUN_TEST(test_rpm_microstep_8_to_16);
  RUN_TEST(test_rpm_microstep_16_to_32);
  RUN_TEST(test_rpm_microstep_4_to_32);
  RUN_TEST(test_rpm_larger_workpiece);
  RUN_TEST(test_rpm_equal_diameters);
  RUN_TEST(test_rpm_negative);
  RUN_TEST(test_rpm_very_large);

  // Section 4: Reverse RPM (9 tests)
  RUN_TEST(test_reverse_rpm_zero);
  RUN_TEST(test_reverse_rpm_roundtrip);
  RUN_TEST(test_reverse_rpm_at_min);
  RUN_TEST(test_reverse_rpm_at_max);
  RUN_TEST(test_reverse_rpm_all_microsteps);
  RUN_TEST(test_reverse_rpm_with_cal_factor);
  RUN_TEST(test_calibrated_motor_command_matches_display_rpm);
  RUN_TEST(test_reverse_rpm_with_calibration);
  RUN_TEST(test_reverse_rpm_cal_unity_matches_no_cal);
  RUN_TEST(test_reverse_rpm_cal_below_1);

  // Section 5: Angle to steps (20 tests)
  RUN_TEST(test_angle_zero);
  RUN_TEST(test_angle_full_rotation_range);
  RUN_TEST(test_angle_quarter_rotation);
  RUN_TEST(test_angle_half_rotation);
  RUN_TEST(test_angle_one_degree);
  RUN_TEST(test_angle_small_01);
  RUN_TEST(test_angle_negative);
  RUN_TEST(test_angle_large_720);
  RUN_TEST(test_angle_microstep_4_to_8);
  RUN_TEST(test_angle_microstep_8_to_16);
  RUN_TEST(test_angle_microstep_16_to_32);
  RUN_TEST(test_angle_cal_unity);
  RUN_TEST(test_angle_cal_1_1);
  RUN_TEST(test_angle_cal_0_9);
  RUN_TEST(test_angle_cal_extreme_0_5);
  RUN_TEST(test_angle_cal_extreme_1_5);
  RUN_TEST(test_angle_cal_zero_gives_zero);
  RUN_TEST(test_angle_cal_roundtrip_with_apply);

  // Section 6: ADC to RPM (13 tests)
  RUN_TEST(test_adc_at_zero);
  RUN_TEST(test_adc_at_ref);
  RUN_TEST(test_adc_at_half_ref);
  RUN_TEST(test_adc_above_ref);
  RUN_TEST(test_adc_at_4095);
  RUN_TEST(test_adc_negative);
  RUN_TEST(test_adc_never_below_min_rpm);
  RUN_TEST(test_adc_monotonic);
  RUN_TEST(test_adc_snap_zone_reaches_max_rpm);
  RUN_TEST(test_adc_snap_running_threshold_covers_emi);
  RUN_TEST(test_adc_snap_zone_monotonic);
  RUN_TEST(test_adc_snap_off_linear_at_mid_high_adc);
  RUN_TEST(test_adc_user_rpm_cap_not_hardcoded_max);

  // Section 7: Calibration logic (13 tests)
  RUN_TEST(test_cal_apply_steps_unity);
  RUN_TEST(test_cal_apply_steps_above);
  RUN_TEST(test_cal_apply_steps_below);
  RUN_TEST(test_cal_apply_steps_zero);
  RUN_TEST(test_cal_apply_steps_negative);
  RUN_TEST(test_cal_apply_angle_unity);
  RUN_TEST(test_cal_apply_angle_above);
  RUN_TEST(test_cal_validate_in_range);
  RUN_TEST(test_cal_validate_out_of_range);
  RUN_TEST(test_cal_validate_reset);
  RUN_TEST(test_cal_clamp_in_range);
  RUN_TEST(test_cal_clamp_below);
  RUN_TEST(test_cal_clamp_above);

  // Section 8: Microstep logic (6 tests)
  RUN_TEST(test_microstep_spr_4);
  RUN_TEST(test_microstep_spr_8);
  RUN_TEST(test_microstep_spr_16);
  RUN_TEST(test_microstep_spr_32);
  RUN_TEST(test_microstep_valid_values);
  RUN_TEST(test_microstep_invalid_values);

  // Section 9: Acceleration clamping (3 tests)
  RUN_TEST(test_accel_in_range);
  RUN_TEST(test_accel_below_min);
  RUN_TEST(test_accel_above_max);

  // Section 10: Direction selection (8 tests)
  RUN_TEST(test_dir_cw_no_invert);
  RUN_TEST(test_dir_ccw_no_invert);
  RUN_TEST(test_dir_cw_with_invert);
  RUN_TEST(test_dir_ccw_with_invert);
  RUN_TEST(test_dir_double_invert);
  RUN_TEST(test_dir_program_override_wins_over_switch);
  RUN_TEST(test_dir_switch_used_without_program_override);
  RUN_TEST(test_dir_program_override_still_obeys_invert);

  // Section 11: Gear ratio verification (3 tests)
  RUN_TEST(test_gear_ratio_value);
  RUN_TEST(test_diameter_ratio_value);
  RUN_TEST(test_combined_ratio);

  // Section 12: Pot override logic (3 tests)
  RUN_TEST(test_pot_override_under_threshold);
  RUN_TEST(test_pot_override_at_threshold);
  RUN_TEST(test_pot_override_over_threshold);

  // Section 13: Cross-validation (3 tests)
  RUN_TEST(test_rpm_angle_formula_relationship);
  RUN_TEST(test_rpm_angle_all_microsteps_consistent);
  RUN_TEST(test_reverse_rpm_inverse_of_forward);

  // Section 14: Microstep string (5 tests)
  RUN_TEST(test_microstep_string_4);
  RUN_TEST(test_microstep_string_8);
  RUN_TEST(test_microstep_string_16);
  RUN_TEST(test_microstep_string_32);
  RUN_TEST(test_microstep_string_invalid);

  // Section 15: Acceleration init default (3 tests)
  RUN_TEST(test_accel_init_valid);
  RUN_TEST(test_accel_init_below);
  RUN_TEST(test_accel_init_above);

  // Section 16: Microstep init default (2 tests)
  RUN_TEST(test_microstep_init_valid);
  RUN_TEST(test_microstep_init_invalid);

  // Section 16b: Milli-Hz START_SPEED floor (8 tests)
  RUN_TEST(test_milli_hz_zero_hz_clamps_to_floor);
  RUN_TEST(test_milli_hz_negative_hz_clamps_to_floor);
  RUN_TEST(test_milli_hz_nan_clamps_to_floor);
  RUN_TEST(test_milli_hz_above_floor_passes_through);
  RUN_TEST(test_milli_hz_just_at_floor);
  RUN_TEST(test_milli_hz_just_below_floor_is_raised);
  RUN_TEST(test_milli_hz_huge_value_saturates_to_u32_max);
  RUN_TEST(test_milli_hz_different_floor_levels);
  RUN_TEST(test_min_rpm_default_microstep_stays_above_start_floor);

  // Section 17: Pulse time clamps (6 tests)
  RUN_TEST(test_pulse_clamp_on_in_range);
  RUN_TEST(test_pulse_clamp_on_below);
  RUN_TEST(test_pulse_clamp_on_above);
  RUN_TEST(test_pulse_clamp_off_in_range);
  RUN_TEST(test_pulse_clamp_off_below);
  RUN_TEST(test_pulse_clamp_off_above);

  // Section 18: Step angle bounds (2 tests)
  RUN_TEST(test_step_bounds_valid);
  RUN_TEST(test_step_bounds_invalid);

  // Section 19: Screen needs rebuild (2 tests)
  RUN_TEST(test_screen_rebuild_editor_screens);
  RUN_TEST(test_screen_rebuild_non_editor);

  return UNITY_END();
}
