#include <unity.h>
#include <algorithm>
#include "../../src/motor/motor.cpp"
#include "../../src/control/control.cpp"
#include "../../src/control/modes/continuous.cpp"
#include "../../src/control/modes/pulse.cpp"
#include "../../src/control/modes/step_mode.cpp"
#include "../../src/control/modes/jog.cpp"
#include "../../src/event_log.cpp"
#include "../../src/ui/value_format.h"

SimSerial Serial;
SimEsp ESP;
std::atomic<bool> g_wakePending{false}, g_dir_switch_cache{false};
SystemSettings g_settings{};
SemaphoreHandle_t g_settings_mutex = nullptr;
static FaultReason testFault = FAULT_NONE;
static Direction testDirection = DIR_CW;
static float testRpm = 0.5f;
[[noreturn]] void fatal_halt(const char*) { std::abort(); }
void safety_report_motor_fault(FaultReason reason) { testFault = reason; control_transition_to(STATE_ESTOP); }
bool safety_inhibit_motion() { return testFault != FAULT_NONE; }
bool safety_check_ui_reset() { return false; }
void safety_register_watchdog() {}
void safety_feed_watchdog() {}
void safety_task_ready(uint32_t) {}
FaultReason safety_get_fault_reason() { return testFault; }
void storage_save_settings() {}
void speed_sync_rpm_limits_from_settings() {}
Direction speed_get_direction() { return testDirection; }
void speed_set_program_direction_override(Direction dir) { testDirection = dir; }
void speed_clear_program_direction_override() {}
void speed_set_workpiece_diameter_mm(float) {}
void speed_slider_set(float rpm) { testRpm = rpm; }
float speed_get_target_rpm() { return testRpm; }
float speed_get_rpm_max() { return MAX_RPM; }
float rpmToStepHzCalibrated(float rpm) { return rpm * 1000.0f; }
long angleToSteps(float angle) { return static_cast<long>(angle * 100.0f); }
uint32_t microstep_get_steps_per_rev() { return 3200; }

void setUp() {
  simTestMillis = 100;
  testStepper = FastAccelStepper{};
  testFault = FAULT_NONE;
  testDirection = DIR_CW;
  testRpm = 0.5f;
  if (!g_settings_mutex) g_settings_mutex = xSemaphoreCreateMutex();
  if (g_stepperMutex) g_stepperMutex->unavailable = false;
  g_settings.acceleration = 7500;
  event_log_init();
  control_init();
  motionGate.takeStop();
  stopRequestedAt.store(0); stoppingAt.store(0);
  if (!g_stepperMutex) motor_init();
  motor_halt();
  motor_restore_configured_acceleration();
  motor_apply_settings();
  motor_refresh_hz_cache();
  step_reset_accumulator();
}
void tearDown() {}

void test_rejected_commands_never_run_or_complete() {
  for (int8_t error : {-1, -2, -3}) {
    testStepper.commandResult = error;
    TEST_ASSERT_TRUE(control_start_step(90)); process_pending_requests();
    TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
    TEST_ASSERT_EQUAL(FAULT_MOTOR_COMMAND, testFault);
    simTestMillis += 100; step_update();
    TEST_ASSERT_EQUAL(0, step_get_count());
    TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
    setUp();
    testStepper.commandResult = error;
    TEST_ASSERT_FALSE(motor_run_cw());
    TEST_ASSERT_FALSE(motor_run_ccw());
    TEST_ASSERT_EQUAL(0, testStepper.starts);
    setUp();
  }
}
void test_speed_and_acceleration_errors_latch_fault() {
  testStepper.speedResult = -1;
  motor_set_target_milli_hz(50000);
  TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  setUp(); testStepper.accelerationResult = -1;
  motor_apply_settings(); TEST_ASSERT_EQUAL(FAULT_MOTOR_COMMAND, testFault);
}
void test_stop_mutex_failure_inhibits_and_blocks_cleanup() {
  g_stepperMutex->unavailable = true;
  motor_stop();
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT, testFault);
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  TEST_ASSERT_TRUE(motor_cleanup_pending());
  TEST_ASSERT_TRUE(motor_is_running());
  TEST_ASSERT_FALSE(motor_halt());
  g_stepperMutex->unavailable = false;
  TEST_ASSERT_TRUE(motor_halt()); TEST_ASSERT_FALSE(motor_cleanup_pending());
}
void test_stop_cancels_queued_start_in_real_dispatcher() {
  TEST_ASSERT_TRUE(control_start_continuous());
  control_stop(); process_pending_requests(); process_pending_requests();
  TEST_ASSERT_EQUAL(0, testStepper.starts);
  TEST_ASSERT_EQUAL(STATE_IDLE, control_get_state());
}
void test_stop_request_and_deceleration_deadlines() {
  control_stop(); simTestMillis += 49; control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_NONE, testFault);
  ++simTestMillis; control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT, testFault);
  setUp(); control_start_continuous(); process_pending_requests();
  motor_refresh_hz_cache(); control_stop(); process_pending_requests();
  TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
  simTestMillis += motor_stop_timeout_ms(); control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
}
void test_pulse_counts_completed_on_and_final_pause() {
  control_start_pulse(100, 200, 1); process_pending_requests();
  TEST_ASSERT_EQUAL(STATE_PULSE, control_get_state());
  simTestMillis += 100; pulse_update();
  TEST_ASSERT_EQUAL(1, pulse_get_cycle_count()); TEST_ASSERT_FALSE(pulse_is_on_phase());
  simTestMillis += 500; pulse_update(); // Still decelerating: OFF must not start yet.
  TEST_ASSERT_EQUAL(STATE_PULSE, control_get_state());
  testStepper.running = false; pulse_update();
  simTestMillis += 199; pulse_update(); TEST_ASSERT_EQUAL(STATE_PULSE, control_get_state());
  ++simTestMillis; pulse_update(); TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
  TEST_ASSERT_EQUAL(1, testStepper.starts); TEST_ASSERT_FALSE(pulse_is_on_phase());
}
void test_stop_deadline_survives_clock_wrap() {
  simTestMillis = UINT32_MAX;
  control_stop(); control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_NONE, testFault);
  simTestMillis = 47; control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_NONE, testFault);
  simTestMillis = 48; control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT, testFault);
}
void test_stop_budget_tracks_soft_start_acceleration() {
  motor_set_target_milli_hz(1500000); motor_run_cw(); motor_refresh_hz_cache();
  motor_apply_soft_start_acceleration();
  TEST_ASSERT_EQUAL_UINT32(2800, motor_stop_timeout_ms());
  motor_restore_configured_acceleration();
  TEST_ASSERT_EQUAL_UINT32(2200, motor_stop_timeout_ms());
}
void test_pulse_two_cycles_and_rejected_restart() {
  pulse_start(100, 100, 2);
  simTestMillis += 100; pulse_update(); testStepper.running = false; pulse_update();
  simTestMillis += 100; pulse_update(); TEST_ASSERT_EQUAL(2, testStepper.starts);
  simTestMillis += 100; pulse_update(); testStepper.running = false; pulse_update();
  simTestMillis += 100; pulse_update(); TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
  TEST_ASSERT_EQUAL(2, pulse_get_cycle_count());
  setUp(); pulse_start(100, 100, 0);
  simTestMillis += 100; pulse_update(); testStepper.running = false; pulse_update();
  testStepper.commandResult = -1; simTestMillis += 100; pulse_update();
  TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
}
void test_step_progress_crosses_counter_boundary() {
  testStepper.position = INT32_MAX - 49;
  step_execute(1.0f);
  testStepper.position = INT32_MIN;
  step_update(); TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, step_get_accumulated());
  testStepper.running = false; simTestMillis += 50; step_update();
  TEST_ASSERT_EQUAL(1, step_get_count());
  setUp(); testDirection = DIR_CCW; testStepper.position = INT32_MIN + 49;
  step_execute(1.0f); testStepper.position = INT32_MAX;
  step_update(); TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, step_get_accumulated());
}
void test_continuous_auto_stop_and_jog_speed_use_production_modes() {
  continuous_start(true, 100); simTestMillis += 99; continuous_update();
  TEST_ASSERT_EQUAL(STATE_RUNNING, control_get_state());
  ++simTestMillis; continuous_update(); TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
  setUp(); jog_set_speed(0.25f); jog_start(DIR_CCW); jog_update();
  TEST_ASSERT_EQUAL(STATE_JOG, control_get_state()); TEST_ASSERT_FALSE(motor_direction_is_cw());
  jog_stop(); TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
}
void test_event_snapshot_contention_retries_and_counts_drops() {
  event_log_add("FAULT TEST");
  EventLogEntry entries[2]; size_t count = 99; uint32_t version = 99;
  s_mutex->unavailable = true;
  const uint32_t drops = event_log_dropped(); event_log_add("DROPPED");
  TEST_ASSERT_EQUAL(drops + 1, event_log_dropped());
  TEST_ASSERT_FALSE(event_log_try_snapshot(entries, 2, &count, &version));
  TEST_ASSERT_EQUAL(99, version);
  s_mutex->unavailable = false;
  TEST_ASSERT_TRUE(event_log_try_snapshot(entries, 2, &count, &version));
  TEST_ASSERT_EQUAL_STRING("FAULT TEST", entries[0].text);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_rejected_commands_never_run_or_complete);
  RUN_TEST(test_speed_and_acceleration_errors_latch_fault);
  RUN_TEST(test_stop_mutex_failure_inhibits_and_blocks_cleanup);
  RUN_TEST(test_stop_cancels_queued_start_in_real_dispatcher);
  RUN_TEST(test_stop_request_and_deceleration_deadlines);
  RUN_TEST(test_stop_deadline_survives_clock_wrap);
  RUN_TEST(test_stop_budget_tracks_soft_start_acceleration);
  RUN_TEST(test_pulse_counts_completed_on_and_final_pause);
  RUN_TEST(test_pulse_two_cycles_and_rejected_restart);
  RUN_TEST(test_step_progress_crosses_counter_boundary);
  RUN_TEST(test_continuous_auto_stop_and_jog_speed_use_production_modes);
  RUN_TEST(test_event_snapshot_contention_retries_and_counts_drops);
  return UNITY_END();
}
