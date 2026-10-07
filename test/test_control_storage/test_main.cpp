#include <unity.h>
#include <new>
#include "../../src/motor/motor.cpp"
#include "../../src/motor/calibration.cpp"
#include "../../src/control/control.cpp"
#include "../../src/control/modes/continuous.cpp"
#include "../../src/control/modes/pulse.cpp"
#include "../../src/control/modes/step_mode.cpp"
#include "../../src/control/modes/jog.cpp"
#include "../../src/event_log.cpp"
#include "../../src/storage/storage.cpp"

SimSerial Serial;
SimEsp ESP;
std::atomic<bool> g_restartRequired{false}, g_storageFormatting{false};
std::atomic<bool> g_wakePending{false}, g_dir_switch_cache{true};
std::atomic<bool> g_flashWriting{false}, g_screenRedraw{false};
static FaultReason testFault = FAULT_NONE;
static Direction testDirection = DIR_CW;
static float testRpm = 0.5f;
[[noreturn]] void fatal_halt(const char*) { std::abort(); }
void safety_report_motor_fault(FaultReason reason) { testFault = reason; control_transition_to(STATE_ESTOP); }
bool safety_inhibit_motion() { return testFault != FAULT_NONE || g_restartRequired.load() || g_storageFormatting.load(); }
bool safety_check_ui_reset() { return false; }
void safety_register_watchdog() {}
void safety_feed_watchdog() {}
void safety_task_ready(uint32_t) {}
FaultReason safety_get_fault_reason() { return testFault; }
float speed_get_actual_rpm() { return motor_get_step_frequency_hz() / 1000.0f; }
SpeedInputSource speed_get_input_source() { return SPEED_SOURCE_UI; }
void speed_apply() {}
bool acceleration_has_pending_apply() { return false; }
void acceleration_clear_pending() {}
Direction speed_resolve_direction(Direction d) { return g_settings.invert_direction ? (d == DIR_CW ? DIR_CCW : DIR_CW) : d; }
void speed_sync_rpm_limits_from_settings() {}
Direction speed_get_direction() { return testDirection; }
void speed_set_program_direction_override(Direction dir) { testDirection = dir; }
void speed_clear_program_direction_override() {}
void speed_set_workpiece_diameter_mm(float) {}
void speed_slider_set(float rpm) { testRpm = rpm; }
float speed_get_target_rpm() { return testRpm; }
float speed_get_rpm_min_for_diameter(float) { return 0.020f; }
float speed_get_rpm_min() { return 0.020f; }
float speed_clamp_rpm(float r) { return std::isfinite(r) ? constrain(r, speed_get_rpm_min(), speed_get_rpm_max()) : 0; }
float speed_get_rpm_max() { return g_settings.max_rpm; }
float rpmToStepHzCalibrated(float rpm) { return rpm * 1000.0f; }
long angleToStepsForDiameter(float angle, float) { return static_cast<long>(angle * 100.0f); }
long angleToSteps(float angle) { return static_cast<long>(angle * 100.0f); }
uint32_t microstep_get_steps_per_rev() { return 3200; }

void setUp() {
  if (!g_settings_mutex) g_settings_mutex = xSemaphoreCreateMutex();
  if (!g_nvs_mutex) g_nvs_mutex = xSemaphoreCreateMutex();
  if (!g_presets_mutex) g_presets_mutex = xSemaphoreCreateMutex();
  if (g_stepperMutex) g_stepperMutex->unavailable = false;
  g_settings = default_settings();
  testStepper = FastAccelStepper{}; testFault = FAULT_NONE;
  g_restartRequired.store(false); g_storageFormatting.store(false); g_dir_switch_cache.store(true);
  testNvs.clear(); testLegacy.clear();
  testNvsShortRead = testNvsShortWrite = testNvsEraseFailure = false;
  testNvsBeforeClear = testNvsBeforePut = nullptr; g_prefs_open = true;
  settingsSave.~SaveRequest(); new (&settingsSave) SaveRequest{1000};
  presetsSave.~SaveRequest(); new (&presetsSave) SaveRequest{500};
  simTestMillis = 2000; event_log_init(); control_init(); motionGate.takeStop();
  stopRequestedAt.store(0); stoppingAt.store(0);
  if (!g_stepperMutex) motor_init();
  motor_halt(); motor_apply_settings(); control_run_cycle();
}
void tearDown() {}
static int stored_microstep() {
  JsonDocument stored;
  const auto& bytes = testNvs["cfg"];
  if (deserializeJson(stored, bytes.data(), bytes.size())) return -1;
  return stored["microstep"].as<int>();
}
void test_cancelled_proposal_cannot_reach_nvs_or_direction_cache() {
  const int original = g_settings.microstep;
  storage_request_settings_save(); // An earlier Display save is still pending.
  auto proposal = g_settings; proposal.microstep = 32; proposal.dir_switch_enabled = false;
  TEST_ASSERT_TRUE(control_apply_motor_settings(proposal));
  testStepper.beforeAcceleration = [] {
    testStepper.beforeAcceleration = nullptr;
    storage_flush(); // Core 1 snapshots while the hardware apply is in progress.
    safety_report_motor_fault(FAULT_MOTOR_COMMAND);
  };
  control_run_cycle();
  TEST_ASSERT_EQUAL(CONFIG_CANCELLED, control_config_status());
  TEST_ASSERT_EQUAL(original, g_settings.microstep);
  TEST_ASSERT_EQUAL(original, stored_microstep());
  TEST_ASSERT_TRUE(g_settings.dir_switch_enabled); TEST_ASSERT_TRUE(g_dir_switch_cache.load());
}
void test_successful_proposal_is_published_only_after_hardware_apply() {
  const int original = g_settings.microstep;
  storage_request_settings_save();
  auto proposal = g_settings; proposal.microstep = 32;
  TEST_ASSERT_TRUE(control_apply_motor_settings(proposal));
  testStepper.beforeAcceleration = [] {
    testStepper.beforeAcceleration = nullptr;
    storage_flush();
  };
  control_run_cycle();
  TEST_ASSERT_EQUAL(CONFIG_APPLIED, control_config_status());
  TEST_ASSERT_EQUAL(original, stored_microstep());
  TEST_ASSERT_EQUAL(32, g_settings.microstep);
  const auto ticket = control_config_save_ticket();
  TEST_ASSERT_EQUAL(STORAGE_PENDING, storage_settings_save_status(ticket));
  simTestMillis += 1000; storage_flush();
  TEST_ASSERT_EQUAL(32, stored_microstep());
  TEST_ASSERT_EQUAL(STORAGE_SAVED, storage_settings_save_status(ticket));
}
void test_old_flash_snapshot_cannot_overwrite_committed_direction_cache() {
  storage_request_settings_save();
  testNvsBeforePut = [] {
    testNvsBeforePut = nullptr;
    auto proposal = g_settings; proposal.dir_switch_enabled = false;
    control_apply_motor_settings(proposal); process_pending_requests();
  };
  storage_flush();
  TEST_ASSERT_EQUAL(CONFIG_APPLIED, control_config_status());
  TEST_ASSERT_FALSE(g_settings.dir_switch_enabled);
  TEST_ASSERT_FALSE(g_dir_switch_cache.load());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cancelled_proposal_cannot_reach_nvs_or_direction_cache);
  RUN_TEST(test_successful_proposal_is_published_only_after_hardware_apply);
  RUN_TEST(test_old_flash_snapshot_cannot_overwrite_committed_direction_cache);
  return UNITY_END();
}
