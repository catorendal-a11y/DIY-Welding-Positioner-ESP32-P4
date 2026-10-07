#include <unity.h>
#include <Arduino.h>
// Hardware-only ISR boundaries. The scheduled policy below is production code.
struct { struct { uint32_t val = 0; } out1_w1ts; } GPIO;
constexpr int FALLING = 2;
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int, void (*)(), int) {}
inline void delayMicroseconds(unsigned) {}
inline void vTaskDelayUntil(TickType_t* tick, TickType_t period) { *tick += period; }
#include "../../src/safety/safety.cpp"

SimSerial Serial;
std::atomic<bool> g_estopPending{false}, g_uiResetPending{false}, g_wakePending{false};
std::atomic<bool> g_inputHeartbeatValid{false}, g_restartRequired{false};
std::atomic<bool> g_storageFormatting{false};
std::atomic<uint32_t> g_inputHeartbeatMs{0}, g_estopTriggerMs{0};
static SystemState testState = STATE_IDLE;
static bool testControlStale = false, testCleanup = false, testPedalHealthy = true, testTouchHealthy = true;
static bool testMirrorArmed = true;
[[noreturn]] void fatal_halt(const char*) { std::abort(); }
void event_log_add(const char*) {}
bool motor_cleanup_pending() { return testCleanup; }
bool speed_pedal_input_healthy() { return testPedalHealthy; }
bool display_touch_operational() { return testTouchHealthy; }
void usb_mirror_set_armed(bool armed) { testMirrorArmed = armed; }
bool control_heartbeat_stale(uint32_t) { return testControlStale; }
void control_check_stop_deadline(uint32_t) {}
SystemState control_get_state() { return testState; }
bool control_transition_to(SystemState state) { testState = state; return true; }

void setUp() {
  simTestMillis = 100;
  simTestPins[PIN_ESTOP] = simTestPins[PIN_DRIVER_ALM] = HIGH;
  simTestPins[PIN_ENA] = LOW;
  testState = STATE_IDLE; testControlStale = testCleanup = false;
  testPedalHealthy = testTouchHealthy = testMirrorArmed = true;
  readyTasks.store(15u); estopLocked.store(false); s_faultReason.store(FAULT_NONE);
  s_driverAlarmLatched.store(false); s_almHighMs = 0;
  g_estopPending.store(false); g_estopTriggerMs.store(0); g_uiResetPending.store(false);
  g_restartRequired.store(false); g_storageFormatting.store(false);
  g_inputHeartbeatValid.store(true); g_inputHeartbeatMs.store(simTestMillis);
  safety_run_cycle(); // Re-arm edge supervisors on fresh task heartbeats.
}
void tearDown() {}

void test_first_alarm_sample_blocks_new_motion() {
  simTestPins[PIN_DRIVER_ALM] = LOW;
  TEST_ASSERT_TRUE(safety_inhibit_motion());
}
void test_short_alarm_does_not_leave_controller_running_with_driver_disabled() {
  testState = STATE_RUNNING;
  simTestPins[PIN_DRIVER_ALM] = LOW; safety_poll_driver_alarm();
  simTestPins[PIN_DRIVER_ALM] = HIGH; safety_poll_driver_alarm();
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  TEST_ASSERT_EQUAL(STATE_ESTOP, testState);
  TEST_ASSERT_EQUAL(FAULT_DRIVER_ALARM, safety_get_fault_reason());
}
void test_alarm_reset_requires_healthy_raw_input() {
  simTestPins[PIN_DRIVER_ALM] = LOW;
  TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
}
void test_new_estop_edge_cannot_be_cleared_by_ui_reset() {
  safety_report_motor_fault(FAULT_MOTOR_TIMEOUT);
  estopISR(); // Edge released before reset samples the raw level again.
  g_uiResetPending.store(true);
  TEST_ASSERT_FALSE(safety_check_ui_reset());
  TEST_ASSERT_TRUE(g_estopPending.load());
  TEST_ASSERT_TRUE(safety_is_estop_locked());
}
void test_restart_and_hmi_failure_block_reset() {
  g_restartRequired.store(true); TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
  g_restartRequired.store(false); testTouchHealthy = false;
  TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
}
void test_formatting_blocks_motion_and_reset_without_clearing_fatal_inhibit() {
  g_storageFormatting.store(true);
  TEST_ASSERT_TRUE(safety_inhibit_motion()); TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
  g_restartRequired.store(true); g_storageFormatting.store(false);
  TEST_ASSERT_TRUE(safety_inhibit_motion()); TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
}
void test_first_fault_is_retained_and_mirror_is_disarmed() {
  safety_report_motor_fault(FAULT_MOTOR_TIMEOUT);
  safety_report_input_fault();
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT, safety_get_fault_reason());
  TEST_ASSERT_FALSE(testMirrorArmed); TEST_ASSERT_TRUE(safety_inhibit_motion());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
}
void test_fault_reset_waits_for_cleanup_and_healthy_heartbeats() {
  testCleanup = true; TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
  testCleanup = false; testControlStale = true; TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
  testControlStale = false; simTestMillis += 101; TEST_ASSERT_FALSE(safety_can_reset_from_overlay());
  g_inputHeartbeatMs.store(simTestMillis); TEST_ASSERT_TRUE(safety_can_reset_from_overlay());
}
void test_reset_only_clears_fault_and_never_enables_driver() {
  safety_report_motor_fault(FAULT_MOTOR_COMMAND);
  g_uiResetPending.store(true); TEST_ASSERT_TRUE(safety_check_ui_reset());
  TEST_ASSERT_EQUAL(FAULT_NONE, safety_get_fault_reason());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]); TEST_ASSERT_EQUAL(STATE_ESTOP, testState);
}
void test_deadman_supervisors_latch_only_after_tasks_start() {
  readyTasks.store(1u); g_inputHeartbeatValid.store(false);
  simTestMillis += 1000; safety_run_cycle();
  TEST_ASSERT_EQUAL(FAULT_NONE, safety_get_fault_reason());
  readyTasks.store(15u); g_inputHeartbeatValid.store(true);
  safety_run_cycle();
  TEST_ASSERT_EQUAL(FAULT_INPUT_STALE, safety_get_fault_reason());
  TEST_ASSERT_EQUAL(STATE_ESTOP, testState);
  setUp(); testControlStale = true; safety_run_cycle();
  TEST_ASSERT_EQUAL(FAULT_CONTROL_STALE, safety_get_fault_reason());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
}
void test_sustained_estop_latches_without_interrupt() {
  simTestPins[PIN_ESTOP] = LOW; safety_run_cycle();
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  TEST_ASSERT_TRUE(g_estopPending.load());
  TEST_ASSERT_TRUE(safety_inhibit_motion());
  simTestMillis += 5; safety_run_cycle();
  TEST_ASSERT_EQUAL(FAULT_ESTOP_PRESSED, safety_get_fault_reason());
  TEST_ASSERT_EQUAL(STATE_ESTOP, testState); TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
}
void test_estop_glitch_latches_and_requires_explicit_reset() {
  estopISR(); safety_run_cycle();
  simTestMillis += 5; safety_run_cycle();
  TEST_ASSERT_EQUAL(FAULT_ESTOP_GLITCH, safety_get_fault_reason());
  TEST_ASSERT_FALSE(g_estopPending.load());
  TEST_ASSERT_TRUE(safety_is_estop_locked());
}
void test_alarm_release_does_not_reset_fault_automatically() {
  simTestPins[PIN_DRIVER_ALM] = LOW; safety_run_cycle();
  simTestPins[PIN_DRIVER_ALM] = HIGH;
  for (int i = 0; i < 50; ++i) { ++simTestMillis; safety_run_cycle(); }
  TEST_ASSERT_FALSE(safety_is_driver_alarm_latched());
  TEST_ASSERT_TRUE(safety_is_estop_locked());
  TEST_ASSERT_EQUAL(FAULT_DRIVER_ALARM, safety_get_fault_reason());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_alarm_sample_blocks_new_motion);
  RUN_TEST(test_short_alarm_does_not_leave_controller_running_with_driver_disabled);
  RUN_TEST(test_alarm_reset_requires_healthy_raw_input);
  RUN_TEST(test_new_estop_edge_cannot_be_cleared_by_ui_reset);
  RUN_TEST(test_restart_and_hmi_failure_block_reset);
  RUN_TEST(test_formatting_blocks_motion_and_reset_without_clearing_fatal_inhibit);
  RUN_TEST(test_first_fault_is_retained_and_mirror_is_disarmed);
  RUN_TEST(test_fault_reset_waits_for_cleanup_and_healthy_heartbeats);
  RUN_TEST(test_reset_only_clears_fault_and_never_enables_driver);
  RUN_TEST(test_deadman_supervisors_latch_only_after_tasks_start);
  RUN_TEST(test_sustained_estop_latches_without_interrupt);
  RUN_TEST(test_estop_glitch_latches_and_requires_explicit_reset);
  RUN_TEST(test_alarm_release_does_not_reset_fault_automatically);
  return UNITY_END();
}
