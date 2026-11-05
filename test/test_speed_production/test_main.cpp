#include <unity.h>
#include <Arduino.h>
inline void analogReadResolution(int) {}
inline void analogSetPinAttenuation(int,int) {}
#define ADC_11db 0
#include "driver/i2c_master.h"
typedef void* i2c_master_dev_handle_t;
struct i2c_device_config_t { int dev_addr_length; int device_address; int scl_speed_hz; };
#define I2C_ADDR_BIT_LEN_7 0
inline int i2c_master_transmit(void*, const unsigned char*, size_t, int) { return -1; }
inline int i2c_master_transmit_receive(void*, const unsigned char*, size_t, unsigned char*, size_t, int) { return -1; }
inline int i2c_master_probe(void*, int, int) { return -1; }
inline int i2c_master_bus_add_device(void*, const i2c_device_config_t*, void**) { return -1; }
inline int i2c_master_bus_rm_device(void*) { return 0; }
#include "../../src/motor/speed.cpp"
#include "../../src/motor/calibration.cpp"
#include "../../src/motor/microstep.cpp"
#include "../../src/motor/motor.cpp"
SimSerial Serial; SimEsp ESP;
[[noreturn]] void fatal_halt(const char*) { std::abort(); }
SystemSettings g_settings{};
SemaphoreHandle_t g_settings_mutex = nullptr;
std::atomic<bool> g_wakePending{false}, g_dir_switch_cache{false};
void storage_save_settings() {}
uint32_t storage_request_settings_save() { return 1; }
void safety_report_motor_fault(FaultReason) {}
void safety_report_input_fault() {}
bool safety_inhibit_motion() { return false; }
bool control_motion_blocked() { return false; }
SystemState control_get_state() { return STATE_IDLE; }
i2c_master_bus_handle_t display_touch_i2c_bus_handle() { return nullptr; }
void setUp() {
  if (!g_settings_mutex) g_settings_mutex = xSemaphoreCreateMutex();
  g_settings.microstep = 16; g_settings.calibration_factor = 1; g_settings.max_rpm = MAX_RPM;
  g_settings.invert_direction = false; g_dir_switch_cache = false;
  calibration_discard_draft(); calibration_process_pending();
  speed_set_workpiece_diameter_mm(300); speed_sync_rpm_limits_from_settings();
  sliderPriorityOverride = false; buttonsActive = false; programDirectionOverrideActive = false;
  programRestorePending = false; potBroken = false;
  pedalEnabled = false; adcFiltered = 3000; pedalFiltered = 1000;
  simTestMillis = 100; adsSampleValid = true; adsSampleMs = 100;
  ads1115Connected = true;
}
void tearDown() {}
void test_real_geometry_rejects_overflow_and_nonfinite() {
  g_settings.microstep=32; g_settings.calibration_factor=1.5f;
  TEST_ASSERT_EQUAL(0,angleToStepsForDiameter(3600,20000));
  TEST_ASSERT_TRUE(angleToStepsForDiameter(360,20000)>0);
  TEST_ASSERT_EQUAL(0,angleToStepsForDiameter(NAN,300));
  TEST_ASSERT_EQUAL(0,angleToStepsForDiameter(INFINITY,300));
  TEST_ASSERT_EQUAL(0,calibration_apply_steps(INT32_MAX));
}
void test_floor_is_visible_and_never_silently_raises_driver_rate() {
  g_settings.microstep=4;
  TEST_ASSERT_FLOAT_WITHIN(0.000001f,0.004f,speed_get_rpm_min());
  TEST_ASSERT_EQUAL(0,motor_milli_hz_for_rpm_calibrated(0.001f));
  speed_slider_set(0.001f);
  TEST_ASSERT_FLOAT_WITHIN(0.000001f,0.004f,speed_get_target_rpm());
  TEST_ASSERT_EQUAL_UINT32(21600,motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm()));
}
void test_geometry_with_no_feasible_range_cannot_command_motor() {
  g_settings.microstep=4; speed_set_workpiece_diameter_mm(1); g_settings.max_rpm=0.1f;
  speed_sync_rpm_limits_from_settings(); speed_slider_set(0.1f);
  TEST_ASSERT_EQUAL_FLOAT(0,speed_get_target_rpm());
  TEST_ASSERT_EQUAL(0,motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm()));
}
void test_pedal_takes_over_only_when_it_reaches_the_ui_speed() {
  pedalEnabled=true; speed_slider_set(0.8f); speed_apply();
  TEST_ASSERT_FLOAT_WITHIN(0.00001f,0.8f,speed_get_target_rpm());
  TEST_ASSERT_EQUAL(SPEED_SOURCE_UI,speed_get_input_source());
  pedalFiltered=1250; speed_apply();  // Moved, but further away: the speed must not jump.
  TEST_ASSERT_EQUAL(SPEED_SOURCE_UI,speed_get_input_source());
  TEST_ASSERT_FLOAT_WITHIN(0.00001f,0.8f,speed_get_target_rpm());
  pedalFiltered=2600; speed_apply();  // Crossed the UI speed.
  TEST_ASSERT_EQUAL(SPEED_SOURCE_PEDAL,speed_get_input_source());
  TEST_ASSERT_TRUE(fabs(speed_get_target_rpm()-0.8f)>0.1f);
}
void test_source_change_rebases_instead_of_simulating_movement() {
  speed_slider_set(0.8f); pedalEnabled=true; speed_apply();
  TEST_ASSERT_EQUAL(SPEED_SOURCE_UI,speed_get_input_source());
  TEST_ASSERT_FLOAT_WITHIN(0.00001f,0.8f,speed_get_target_rpm());
  pedalFiltered=pedalFiltered.load()+201; speed_apply(); TEST_ASSERT_EQUAL(SPEED_SOURCE_UI,speed_get_input_source());
  pedalFiltered=2700; speed_apply(); TEST_ASSERT_EQUAL(SPEED_SOURCE_PEDAL,speed_get_input_source());
}
void test_requested_and_effective_directions_are_distinct() {
  g_settings.invert_direction=true; speed_set_direction(DIR_CW);
  TEST_ASSERT_EQUAL(DIR_CW,speed_get_requested_direction()); TEST_ASSERT_EQUAL(DIR_CCW,speed_get_direction());
  speed_set_program_direction_override(speed_get_requested_direction()); TEST_ASSERT_EQUAL(DIR_CCW,speed_get_direction());
}
void test_pedal_enabled_without_ads_stays_healthy_and_uses_panel_pot() {
  pedalEnabled = true; ads1115Connected = false; buttonsActive = false; adcFiltered = 3000;
  TEST_ASSERT_TRUE(speed_pedal_input_healthy());
  speed_apply();
  TEST_ASSERT_EQUAL(SPEED_SOURCE_POT, speed_get_input_source());
}
void test_pedal_with_detected_ads_still_blocks_on_stale_samples() {
  pedalEnabled = true; ads1115Connected = true; buttonsActive = false;
  adsSampleValid = false;
  TEST_ASSERT_FALSE(speed_pedal_input_healthy());
  adsSampleValid = true; adsSampleMs = simTestMillis - 500;
  TEST_ASSERT_FALSE(speed_pedal_input_healthy());
  adsSampleMs = simTestMillis;
  TEST_ASSERT_TRUE(speed_pedal_input_healthy());
}
static void direction_filter_initial_cw() {
  g_dir_switch_cache.store(true); stableDirSwitch.store(true);
  dirSwitchCandidate = HIGH; dirSwitchCandidateMs = 0;
  simTestPins[PIN_DIR_SWITCH] = HIGH;
  speed_update_adc();
}
void test_pot_supply_loss_holds_speed_then_blocks_motion() {
  PotBreakDetector pot;
  pot.update(2000);
  pot.update(0); TEST_ASSERT_TRUE(pot.holding()); TEST_ASSERT_FALSE(pot.broken());
  pot.update(0); TEST_ASSERT_FALSE(pot.broken());
  pot.update(0); TEST_ASSERT_TRUE(pot.broken());
  pot.update(500); TEST_ASSERT_FALSE(pot.holding());
  potBroken = true;
  TEST_ASSERT_FALSE(speed_pedal_input_healthy());
  potBroken = false;
}
void test_pot_glitch_is_ignored_and_clears() {
  PotBreakDetector pot;
  pot.update(2000); pot.update(0); pot.update(2000);
  TEST_ASSERT_FALSE(pot.holding());
  pot.update(0); pot.update(0);
  TEST_ASSERT_FALSE(pot.broken());
}
void test_pot_turned_to_zero_by_hand_is_not_a_fault() {
  PotBreakDetector pot;
  for (int adc = 2000; adc >= 0; adc -= 330) pot.update(adc);
  for (int i = 0; i < 5; ++i) pot.update(0);
  TEST_ASSERT_FALSE(pot.holding());
}
void test_ads_small_negative_code_at_full_pedal_is_valid() {
  TEST_ASSERT_TRUE(ads_code_in_range(-3));
  TEST_ASSERT_TRUE(ads_code_in_range(0));
  TEST_ASSERT_FALSE(ads_code_in_range(-200));
  TEST_ASSERT_FALSE(ads_code_in_range(28001));
}
void test_rpm_ceiling_respects_motor_limit_for_large_diameter() {
  speed_set_workpiece_diameter_mm(300);
  TEST_ASSERT_EQUAL_FLOAT(MAX_RPM, speed_get_rpm_max());
  speed_set_workpiece_diameter_mm(1000);  // 3 RPM would need 4050 motor RPM; the limit allows 1.11.
  const float cap = speed_get_rpm_max();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.11f, cap);
  TEST_ASSERT_TRUE(rpmToStepHzCalibrated(cap) <= MOTOR_RPM_LIMIT / 60.0f * 3200.0f);
  TEST_ASSERT_TRUE(motor_milli_hz_for_rpm_calibrated(cap) > 0);
  TEST_ASSERT_EQUAL(0, motor_milli_hz_for_rpm_calibrated(cap + 0.1f));
  speed_set_workpiece_diameter_mm(300);
}
void test_mechanics_come_from_settings() {
  const float hz = rpmToStepHz(1.0f);
  g_settings.roller_mm = 160.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, hz / 2.0f, rpmToStepHz(1.0f));
  g_settings.roller_mm = D_RULLE * 1000.0f;
}
void test_program_values_do_not_leak_into_manual_mode() {
  speed_set_workpiece_diameter_mm(300); speed_slider_set(1.0f);
  speed_set_program_direction_override(DIR_CCW);
  speed_set_workpiece_diameter_mm(500); speed_slider_set(2.0f);
  speed_clear_program_direction_override();
  TEST_ASSERT_EQUAL_FLOAT(300.0f, speed_get_workpiece_diameter_mm());
  TEST_ASSERT_EQUAL_FLOAT(1.0f, sliderRPM.load());
  speed_clear_program_direction_override();  // Idempotent: IDLE and fault paths both call it.
  TEST_ASSERT_EQUAL_FLOAT(300.0f, speed_get_workpiece_diameter_mm());
}
void test_direction_change_at_zero_timestamp_is_accepted() {
  simTestMillis = UINT32_MAX - 20u; direction_filter_initial_cw();
  simTestMillis = 0; simTestPins[PIN_DIR_SWITCH] = LOW; speed_update_adc();
  simTestMillis = 20; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CW, speed_get_requested_direction());
  simTestMillis = 40; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CCW, speed_get_requested_direction());
}
void test_direction_change_waits_across_clock_wrap() {
  direction_filter_initial_cw();
  simTestMillis = UINT32_MAX - 10u; simTestPins[PIN_DIR_SWITCH] = LOW; speed_update_adc();
  simTestMillis = 10; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CW, speed_get_requested_direction());
  simTestMillis = 20; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CCW, speed_get_requested_direction());
}
void test_direction_bounce_restarts_stability_window() {
  direction_filter_initial_cw();
  simTestPins[PIN_DIR_SWITCH] = LOW; speed_update_adc();
  simTestMillis += 10; simTestPins[PIN_DIR_SWITCH] = HIGH; speed_update_adc();
  simTestMillis += 10; simTestPins[PIN_DIR_SWITCH] = LOW; speed_update_adc();
  simTestMillis += 20; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CW, speed_get_requested_direction());
  simTestMillis += 20; speed_update_adc(); TEST_ASSERT_EQUAL(DIR_CCW, speed_get_requested_direction());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_real_geometry_rejects_overflow_and_nonfinite);
  RUN_TEST(test_floor_is_visible_and_never_silently_raises_driver_rate);
  RUN_TEST(test_geometry_with_no_feasible_range_cannot_command_motor);
  RUN_TEST(test_pedal_takes_over_only_when_it_reaches_the_ui_speed);
  RUN_TEST(test_source_change_rebases_instead_of_simulating_movement);
  RUN_TEST(test_requested_and_effective_directions_are_distinct);
  RUN_TEST(test_pedal_enabled_without_ads_stays_healthy_and_uses_panel_pot);
  RUN_TEST(test_pedal_with_detected_ads_still_blocks_on_stale_samples);
  RUN_TEST(test_pot_supply_loss_holds_speed_then_blocks_motion);
  RUN_TEST(test_pot_glitch_is_ignored_and_clears);
  RUN_TEST(test_pot_turned_to_zero_by_hand_is_not_a_fault);
  RUN_TEST(test_ads_small_negative_code_at_full_pedal_is_valid);
  RUN_TEST(test_rpm_ceiling_respects_motor_limit_for_large_diameter);
  RUN_TEST(test_mechanics_come_from_settings);
  RUN_TEST(test_program_values_do_not_leak_into_manual_mode);
  RUN_TEST(test_direction_change_at_zero_timestamp_is_accepted);
  RUN_TEST(test_direction_change_waits_across_clock_wrap);
  RUN_TEST(test_direction_bounce_restarts_stability_window);
  return UNITY_END();
}
