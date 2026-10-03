#include <unity.h>
#include <algorithm>
#include <thread>
#include "../../src/control/setup_policy.h"
#include "../../src/motor/motor.cpp"
#include "../../src/motor/calibration.cpp"
#include "../../src/motor/calibration_session.h"
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
uint32_t storage_request_settings_save() { return 1; }
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
long angleToStepsForDiameter(float angle,float) { return static_cast<long>(angle*100.0f); }
long angleToSteps(float angle) { return static_cast<long>(angle * 100.0f); }
uint32_t microstep_get_steps_per_rev() { return 3200; }

void setUp() {
  control_set_calibration_active(false);
  simTestMillis = 100;
  testStepper = FastAccelStepper{};
  testFault = FAULT_NONE;
  testDirection = DIR_CW;
  testRpm = 0.5f;
  if (!g_settings_mutex) g_settings_mutex = xSemaphoreCreateMutex();
  if (g_stepperMutex) g_stepperMutex->unavailable = false;
  calibration_discard_draft(); calibration_process_pending(); g_settings.calibration_factor = 1.0f;
  g_settings.max_rpm = MAX_RPM; g_settings.microstep = 16; g_settings.stepper_driver = STEPPER_DRIVER_DM542T; jog_set_speed(0.5f);
  g_settings.acceleration = 7500; g_settings.invert_direction = false;
  event_log_init();
  control_init();
  motionGate.takeStop();
  stopRequestedAt.store(0); stoppingAt.store(0);
  if (!g_stepperMutex) motor_init();
  motor_halt();
  motor_restore_configured_acceleration();
  motor_apply_settings();
  motor_refresh_hz_cache();
  step_reset_accumulator(); control_run_cycle();
}
void tearDown() {}

void test_snapshot_and_dispatch_use_one_control_cycle() {
  ControlSnapshot view;
  control_run_cycle(); TEST_ASSERT_TRUE(control_read_snapshot(view));
  TEST_ASSERT_EQUAL(STATE_IDLE, view.state);
  const auto sequence = view.sequence;
  control_start_continuous(); control_run_cycle();
  TEST_ASSERT_TRUE(control_read_snapshot(view));
  TEST_ASSERT_TRUE(view.sequence > sequence); TEST_ASSERT_EQUAL(STATE_RUNNING, view.state);
  TEST_ASSERT_TRUE(view.motor_running); TEST_ASSERT_EQUAL(simTestMillis, view.timestamp_ms);
  control_stop(); control_run_cycle(); testStepper.running = false; control_run_cycle();
  TEST_ASSERT_TRUE(control_read_snapshot(view)); TEST_ASSERT_EQUAL(STATE_IDLE, view.state);
}
void test_stale_control_blocks_start_but_stop_is_unconditional() {
  simTestMillis += 101;
  TEST_ASSERT_FALSE(control_start_continuous());
  TEST_ASSERT_FALSE(control_start_step(90));
  TEST_ASSERT_TRUE(control_stop());
  control_run_cycle();
  TEST_ASSERT_EQUAL(STATE_IDLE, control_get_state());
  TEST_ASSERT_EQUAL(0, testStepper.starts);
  TEST_ASSERT_TRUE(control_start_continuous()); control_run_cycle();
  TEST_ASSERT_EQUAL(STATE_RUNNING, control_get_state());
}
void test_config_apply_publishes_receipt_and_driver_kind() {
  auto settings = g_settings;
  settings.stepper_driver = STEPPER_DRIVER_DM542T;
  settings.microstep = 8; settings.acceleration = 6000; settings.max_rpm = 1;
  TEST_ASSERT_TRUE(control_apply_motor_settings(settings));
  control_run_cycle();
  TEST_ASSERT_EQUAL(CONFIG_APPLIED, control_config_status());
  TEST_ASSERT_EQUAL(1, control_config_save_ticket());
  TEST_ASSERT_EQUAL(STEPPER_DRIVER_DM542T, g_settings.stepper_driver);
  TEST_ASSERT_EQUAL(8, g_settings.microstep);
}
void test_jog_inversion_and_lease_expiry() {
  g_settings.invert_direction = true;
  control_start_jog_cw(); control_run_cycle();
  TEST_ASSERT_FALSE(motor_direction_is_cw());
  simTestMillis += 151; control_run_cycle();
  TEST_ASSERT_EQUAL(STATE_STOPPING, control_get_state());
  g_settings.invert_direction = false;
}
void test_snapshot_staleness_handles_wrap_and_boundary() {
  TEST_ASSERT_TRUE(control_timestamp_fresh(100, 0, true));
  TEST_ASSERT_FALSE(control_timestamp_fresh(101, 0, true));
  TEST_ASSERT_FALSE(control_timestamp_fresh(0, 0, false));
  TEST_ASSERT_TRUE(control_timestamp_fresh(20, UINT32_MAX - 10, true));
  TEST_ASSERT_TRUE(control_timestamp_fresh(100,101,true));
  TEST_ASSERT_FALSE(control_timestamp_fresh(100,106,true));
}
void test_snapshot_mailbox_never_tears_concurrent_reads() {
  struct Pair { uint32_t value, inverse; };
  SnapshotMailbox<Pair> box;
  Pair v{}; TEST_ASSERT_FALSE(box.read(v));
  std::atomic<bool> done{false}; std::atomic<bool> torn{false};
  std::thread writer([&] { for (uint32_t i=0; i<50000; ++i) box.publish({i, ~i}); done.store(true); });
  do { if (box.read(v) && v.inverse != ~v.value) torn.store(true); } while (!done.load());
  writer.join(); TEST_ASSERT_FALSE(torn.load());
}
void test_setup_requires_observed_sequence_and_confirmed_stages() {
  SetupProgress s;
  TEST_ASSERT_FALSE(s.advance()); s.motor_saved = true; TEST_ASSERT_TRUE(s.advance());
  TEST_ASSERT_FALSE(s.advance()); s.direction_confirmed = true; TEST_ASSERT_TRUE(s.advance());
  s.calibration_saved = true; TEST_ASSERT_TRUE(s.advance());
  s.start_requested = true; s.stop_requested = true;
  s.observe(false, false, true, false); TEST_ASSERT_FALSE(s.advance());
  s.observe(true, true, false, false); s.observe(false, true, false, false);
  TEST_ASSERT_FALSE(s.reset_seen); s.observe(false, false, true, false);
  TEST_ASSERT_TRUE(s.reset_seen); TEST_ASSERT_FALSE(s.advance());
  s.observe(false, false, false, true); s.observe(false, false, true, false);
  TEST_ASSERT_TRUE(s.advance()); TEST_ASSERT_EQUAL((int)SetupStage::Saving, (int)s.stage);
}
void test_setup_migration_preserves_existing_installations() {
  TEST_ASSERT_TRUE(setup_migration_completed(false, false));
  TEST_ASSERT_FALSE(setup_migration_completed(true, false));
  TEST_ASSERT_TRUE(setup_migration_completed(true, true));
}
void test_rejected_commands_never_run_or_complete() {
  for (auto error : {MoveResultCode::ErrorNoDirectionPin,
                     MoveResultCode::ErrorSpeedIsUndefined,
                     MoveResultCode::ErrorAccelerationIsUndefined}) {
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
  testStepper.commandResult = MoveResultCode::ErrorSpeedIsUndefined; simTestMillis += 100; pulse_update();
  TEST_ASSERT_EQUAL(STATE_ESTOP, control_get_state());
}
void test_force_stop_drain_keeps_cleanup_pending_and_ena_disabled() {
  TEST_ASSERT_TRUE(motor_run_cw());
  testStepper.drainForceStop = true;
  TEST_ASSERT_FALSE(motor_halt());
  TEST_ASSERT_TRUE(motor_cleanup_pending());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  TEST_ASSERT_TRUE(testStepper.running);
  TEST_ASSERT_FALSE(motor_halt());
  testStepper.running = false; // Hardware completion, independently of forceStop().
  TEST_ASSERT_TRUE(motor_halt());
  TEST_ASSERT_FALSE(motor_cleanup_pending());
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
}
void test_direction_timing_never_changes_during_queued_motion() {
  g_settings.stepper_driver = STEPPER_DRIVER_STANDARD; motor_apply_settings();
  const auto before = testStepper.directionWrites;
  TEST_ASSERT_TRUE(motor_run_cw()); testStepper.drainForceStop = true;
  g_settings.stepper_driver = STEPPER_DRIVER_DM542T; motor_apply_settings();
  TEST_ASSERT_EQUAL(before, testStepper.directionWrites);
  TEST_ASSERT_EQUAL(FAULT_MOTOR_COMMAND, testFault);
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
  TEST_ASSERT_TRUE(motor_cleanup_pending());
}
void test_rmt_driver_is_selected_explicitly() {
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(FasDriver::RMT), static_cast<uint8_t>(engine.selectedDriver));
  MotorDriverInfo info;
  TEST_ASSERT_TRUE(motor_read_driver_info(info));
  TEST_ASSERT_EQUAL_STRING("RMT", info.name);
  TEST_ASSERT_EQUAL_UINT32(1500, info.direction_before_us);
  TEST_ASSERT_EQUAL_UINT32(200, info.direction_after_us);
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

void test_calibration_draft_is_not_persisted_before_save() {
  calibration_set_factor(1.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 1.1f, calibration_get_factor());
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 1.0f, g_settings.calibration_factor);
  calibration_discard_draft(); control_run_cycle();
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 1.0f, calibration_get_factor());
  calibration_set_factor(1.2f); calibration_save();
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 1.2f, g_settings.calibration_factor);
}
void test_calibration_requires_two_completed_moves() {
  CalibrationSession s; s.reset(1.0f,300);
  TEST_ASSERT_FALSE(s.measurement(360));
  TEST_ASSERT_TRUE(s.begin(100,1000,4,true,16));
  s.observe(120,true,false,4,true,16); TEST_ASSERT_TRUE(s.moving());
  s.observe(200,true,false,5,true,16); TEST_ASSERT_EQUAL(CalibrationSession::Measure,s.stage);
  TEST_ASSERT_TRUE(s.measurement(345)); TEST_ASSERT_TRUE(s.apply());
  TEST_ASSERT_FLOAT_WITHIN(0.00001f,360.0f/345,s.factor); TEST_ASSERT_FALSE(s.passed());
  TEST_ASSERT_TRUE(s.begin(300,1000,5,true,16));
  s.observe(500,true,false,6,true,16); TEST_ASSERT_TRUE(s.measurement(360.25f)); TEST_ASSERT_TRUE(s.passed());
}
void test_calibration_abort_and_fault_never_allow_save() {
  CalibrationSession s; s.begin(100,1000,0,true,16); s.abort("STOP");
  TEST_ASSERT_FALSE(s.measurement(360)); TEST_ASSERT_FALSE(s.passed());
  s.begin(200,1000,0,true,16); s.observe(300,true,true,1,true,16);
  TEST_ASSERT_EQUAL(CalibrationSession::Prepare,s.stage); TEST_ASSERT_FALSE(s.measurement(360));
}
void test_calibration_rejected_move_timeout_and_clock_wrap() {
  CalibrationSession s; s.begin(0xfffffff0u,1000,3,true,16);
  s.observe(50,true,false,3,true,16); TEST_ASSERT_TRUE(s.moving());
  s.observe(600,true,false,3,true,16); TEST_ASSERT_FALSE(s.moving()); TEST_ASSERT_FALSE(s.measurement(360));
  s.begin(1000,1000,3,true,16); s.observe(2001,false,false,3,true,16); TEST_ASSERT_FALSE(s.moving());
}
void test_calibration_context_change_rejects_move() {
  CalibrationSession s; s.begin(100,1000,0,true,16); s.observe(200,true,false,1,false,16);
  TEST_ASSERT_FALSE(s.measurement(360));
  s.begin(300,1000,0,true,16); s.observe(400,true,false,1,true,32); TEST_ASSERT_FALSE(s.measurement(360));
}
void test_calibration_rejects_silent_factor_clamp_and_invalid_numbers() {
  CalibrationSession s; s.begin(100,1000,0,true,16); s.observe(200,true,false,1,true,16);
  TEST_ASSERT_FALSE(s.measurement(NAN)); TEST_ASSERT_TRUE(s.measurement(180));
  TEST_ASSERT_FALSE(s.apply()); TEST_ASSERT_FLOAT_WITHIN(0.00001f,1,s.factor);
  TEST_ASSERT_TRUE(s.measurement(360)); TEST_ASSERT_TRUE(s.apply());
  calibration_set_factor(NAN); TEST_ASSERT_FLOAT_WITHIN(0.00001f,1,calibration_get_factor());
}
void test_calibration_parser_accepts_comma_rejects_trailing_text() {
  float v; TEST_ASSERT_TRUE(calibration_parse_angle("359,75",v)); TEST_ASSERT_FLOAT_WITHIN(0.001f,359.75f,v);
  TEST_ASSERT_FALSE(calibration_parse_angle("360junk",v)); TEST_ASSERT_FALSE(calibration_parse_angle("360.1.2",v));
  TEST_ASSERT_FALSE(calibration_parse_angle("nan",v)); TEST_ASSERT_FALSE(calibration_parse_angle("",v));
}
void test_calibration_blocks_unrelated_motion_and_pedal_mode() {
  control_set_calibration_active(true); control_run_cycle();
  TEST_ASSERT_TRUE(control_setup_active()); TEST_ASSERT_FALSE(control_start_continuous());
  TEST_ASSERT_FALSE(control_start_pulse(500,500,1));
  TEST_ASSERT_TRUE(control_start_step(360)); control_stop(); control_run_cycle();
  control_set_calibration_active(false); TEST_ASSERT_FALSE(control_setup_active());
}

void test_jog_reclamps_after_config_lowered_cap() {
  jog_set_speed(1.0f);
  auto settings = g_settings; settings.max_rpm = 0.1f;
  TEST_ASSERT_TRUE(control_apply_motor_settings(settings)); control_run_cycle();
  TEST_ASSERT_TRUE(control_start_jog_cw()); control_run_cycle();
  TEST_ASSERT_FLOAT_WITHIN(0.00001f, 0.1f, jog_get_speed());
  TEST_ASSERT_EQUAL_UINT32(100000, testStepper.milliHz);
}
void test_internal_pulse_stop_has_independent_deadline() {
  control_start_pulse(100,100,1); control_run_cycle();
  simTestMillis += 100; pulse_update();
  simTestMillis += motor_stop_timeout_ms(); control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT, testFault);
  TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]);
}


void test_deferred_start_never_survives_fault_reset_or_stop() {
  for (int fault=0; fault<2; ++fault) {
    const uint32_t ticket = control_motion_generation();
    TEST_ASSERT_TRUE(control_deferred_start_valid(ticket));
    if (fault) safety_report_motor_fault(FAULT_MOTOR_COMMAND); else control_stop();
    control_run_cycle(); testFault = FAULT_NONE;
    if (control_get_state() == STATE_ESTOP) control_transition_to(STATE_IDLE);
    control_run_cycle();
    TEST_ASSERT_FALSE(control_start_deferred_continuous(ticket));
    TEST_ASSERT_EQUAL(0, testStepper.starts);
  }
  TEST_ASSERT_TRUE(control_start_deferred_continuous(control_motion_generation()));
  control_run_cycle(); TEST_ASSERT_EQUAL(STATE_RUNNING, control_get_state());
}
void test_finite_move_deadline_and_completed_move_cancellation() {
  step_execute(90); TEST_ASSERT_EQUAL(STATE_STEP,control_get_state());
  simTestMillis += motionDeadlineBudget.load(); control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_MOTOR_TIMEOUT,testFault);
  setUp(); step_execute(90); testStepper.running = false;
  simTestMillis += 100; step_update(); control_run_cycle();
  TEST_ASSERT_EQUAL(STATE_IDLE,control_get_state());
  simTestMillis += 10000; control_check_stop_deadline(simTestMillis);
  TEST_ASSERT_EQUAL(FAULT_NONE,testFault);
}
void test_motion_checked_range_and_invalid_commands() {
  int32_t out = 123; TEST_ASSERT_TRUE(motion_checked_steps(INT32_MAX,out));
  TEST_ASSERT_EQUAL_INT32(INT32_MAX,out);
  for (double value : {double(INT32_MAX)+1, -1.0, 0.0, double(NAN), double(INFINITY)}) {
    TEST_ASSERT_FALSE(motion_checked_steps(value,out)); TEST_ASSERT_EQUAL(0,out);
  }
  TEST_ASSERT_FALSE(control_start_step(NAN)); TEST_ASSERT_FALSE(control_start_step(INFINITY));
  TEST_ASSERT_FALSE(control_start_step_sequence(90,1,NAN));
  int32_t start; TEST_ASSERT_FALSE(motor_move_steps(-2147483647L-1,0.5f,&start));
  TEST_ASSERT_FALSE(motor_move_steps(100,NAN,&start));
  TEST_ASSERT_EQUAL(0,testStepper.moves);
}
void test_below_floor_rate_is_rejected_without_silent_speedup() {
  TEST_ASSERT_EQUAL(0,motor_milli_hz_for_rpm_calibrated(0.001f));
  TEST_ASSERT_EQUAL(0,motor_milli_hz_for_rpm_calibrated(NAN));
  testRpm = 0.001f; TEST_ASSERT_FALSE(control_start_continuous());
  TEST_ASSERT_FALSE(control_start_pulse(100,100,1)); TEST_ASSERT_EQUAL(0,testStepper.starts);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_deferred_start_never_survives_fault_reset_or_stop);
  RUN_TEST(test_finite_move_deadline_and_completed_move_cancellation);
  RUN_TEST(test_motion_checked_range_and_invalid_commands);
  RUN_TEST(test_below_floor_rate_is_rejected_without_silent_speedup);
  RUN_TEST(test_jog_reclamps_after_config_lowered_cap);
  RUN_TEST(test_internal_pulse_stop_has_independent_deadline);
  RUN_TEST(test_calibration_blocks_unrelated_motion_and_pedal_mode);
  RUN_TEST(test_calibration_draft_is_not_persisted_before_save);
  RUN_TEST(test_calibration_requires_two_completed_moves);
  RUN_TEST(test_calibration_abort_and_fault_never_allow_save);
  RUN_TEST(test_calibration_rejected_move_timeout_and_clock_wrap);
  RUN_TEST(test_calibration_context_change_rejects_move);
  RUN_TEST(test_calibration_rejects_silent_factor_clamp_and_invalid_numbers);
  RUN_TEST(test_calibration_parser_accepts_comma_rejects_trailing_text);

  RUN_TEST(test_force_stop_drain_keeps_cleanup_pending_and_ena_disabled);
  RUN_TEST(test_direction_timing_never_changes_during_queued_motion);
  RUN_TEST(test_rmt_driver_is_selected_explicitly);
  RUN_TEST(test_snapshot_and_dispatch_use_one_control_cycle);
  RUN_TEST(test_stale_control_blocks_start_but_stop_is_unconditional);
  RUN_TEST(test_config_apply_publishes_receipt_and_driver_kind);
  RUN_TEST(test_jog_inversion_and_lease_expiry);
  RUN_TEST(test_snapshot_staleness_handles_wrap_and_boundary);
  RUN_TEST(test_snapshot_mailbox_never_tears_concurrent_reads);
  RUN_TEST(test_setup_requires_observed_sequence_and_confirmed_stages);
  RUN_TEST(test_setup_migration_preserves_existing_installations);
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
