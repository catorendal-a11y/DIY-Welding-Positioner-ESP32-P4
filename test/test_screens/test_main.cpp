// TIG Rotator Controller - Screen Pure Logic Unit Tests
// Tests all extractable computation from screen files
// Run: pio test -e native

#include <unity.h>
#include <cstring>
#include <cmath>
#include <cstdio>

#include "../../src/ui/test_screen_logic.h"
#include "../../src/motor/test_speed_logic.h"

void setUp(void) {}
void tearDown(void) {}

// Hardware constants (mirror of config.h)
static const float GEAR_RATIO     = 60.0f * 72.0f / 40.0f;  // 108
static const float D_EMNE         = 0.300f;
static const float D_RULLE        = 0.080f;
static const float MIN_RPM        = 0.001f;
static const float MAX_RPM        = 3.0f;
static const uint32_t STEPS_1_8   = 1600;

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 1: PULSE COMPUTATION
// ═══════════════════════════════════════════════════════════════════════════════

void test_pulse_duty_equal() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, pulse_compute_duty(500, 500));
}

void test_pulse_duty_all_on() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, pulse_compute_duty(1000, 0));
}

void test_pulse_duty_all_off() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, pulse_compute_duty(0, 1000));
}

void test_pulse_duty_both_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, pulse_compute_duty(0, 0));
}

void test_pulse_duty_75_25() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 75.0f, pulse_compute_duty(3000, 1000));
}

void test_pulse_duty_short_cycle() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, pulse_compute_duty(100, 100));
}

void test_pulse_cycle_time_1s() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, pulse_compute_cycle_time(500, 500));
}

void test_pulse_cycle_time_10s() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, pulse_compute_cycle_time(5000, 5000));
}

void test_pulse_cycle_time_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pulse_compute_cycle_time(0, 0));
}

void test_pulse_frequency_1hz() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, pulse_compute_frequency(500, 500));
}

void test_pulse_frequency_5hz() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, pulse_compute_frequency(100, 100));
}

void test_pulse_frequency_zero() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pulse_compute_frequency(0, 0));
}

void test_pulse_total_time_finite() {
  float total = pulse_compute_total_time(500, 500, 10);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, total);
}

void test_pulse_total_time_infinite() {
  float total = pulse_compute_total_time(500, 500, 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, total);
}

void test_pulse_total_time_one_cycle() {
  float total = pulse_compute_total_time(1000, 2000, 1);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.0f, total);
}

void test_pulse_bar_time_at_min() {
  TEST_ASSERT_EQUAL(0, pulse_bar_percent_time(100, 100, 10000));
}

void test_pulse_bar_time_at_max() {
  TEST_ASSERT_EQUAL(100, pulse_bar_percent_time(10000, 100, 10000));
}

void test_pulse_bar_time_at_mid() {
  int pct = pulse_bar_percent_time(5050, 100, 10000);
  TEST_ASSERT_EQUAL(50, pct);
}

void test_pulse_bar_time_below_min() {
  TEST_ASSERT_EQUAL(0, pulse_bar_percent_time(0, 100, 10000));
}

void test_pulse_bar_time_above_max() {
  TEST_ASSERT_EQUAL(100, pulse_bar_percent_time(20000, 100, 10000));
}

void test_pulse_bar_rpm_at_min() {
  TEST_ASSERT_EQUAL(0, pulse_bar_percent_rpm(MIN_RPM, MIN_RPM, MAX_RPM));
}

void test_pulse_bar_rpm_at_max() {
  TEST_ASSERT_EQUAL(100, pulse_bar_percent_rpm(MAX_RPM, MIN_RPM, MAX_RPM));
}

void test_pulse_bar_rpm_at_mid() {
  float mid = (MIN_RPM + MAX_RPM) / 2.0f;
  int pct = pulse_bar_percent_rpm(mid, MIN_RPM, MAX_RPM);
  TEST_ASSERT_INT_WITHIN(2, 50, pct);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2: TIMER / COUNTDOWN
// ═══════════════════════════════════════════════════════════════════════════════

void test_countdown_color_full() {
  TEST_ASSERT_EQUAL(0, countdown_color_index(10, 10));
}

void test_countdown_color_two_thirds() {
  TEST_ASSERT_EQUAL(0, countdown_color_index(7, 10));
}

void test_countdown_color_at_66() {
  TEST_ASSERT_EQUAL(0, countdown_color_index(67, 100));
  TEST_ASSERT_EQUAL(1, countdown_color_index(66, 100));
}

void test_countdown_color_at_33() {
  TEST_ASSERT_EQUAL(1, countdown_color_index(34, 100));
  TEST_ASSERT_EQUAL(2, countdown_color_index(33, 100));
}

void test_countdown_color_zero() {
  TEST_ASSERT_EQUAL(2, countdown_color_index(0, 10));
}

void test_countdown_color_total_zero() {
  TEST_ASSERT_EQUAL(0, countdown_color_index(5, 0));
}

void test_countdown_arc_full() {
  TEST_ASSERT_EQUAL(100, countdown_arc_pct(10, 10));
}

void test_countdown_arc_half() {
  TEST_ASSERT_EQUAL(50, countdown_arc_pct(5, 10));
}

void test_countdown_arc_zero() {
  TEST_ASSERT_EQUAL(0, countdown_arc_pct(0, 10));
}

void test_countdown_arc_negative() {
  TEST_ASSERT_EQUAL(0, countdown_arc_pct(-5, 10));
}

void test_countdown_clamp_low() {
  TEST_ASSERT_EQUAL(1, countdown_clamp_seconds(0));
  TEST_ASSERT_EQUAL(1, countdown_clamp_seconds(-5));
}

void test_countdown_clamp_high() {
  TEST_ASSERT_EQUAL(10, countdown_clamp_seconds(11));
  TEST_ASSERT_EQUAL(10, countdown_clamp_seconds(100));
}

void test_countdown_clamp_valid() {
  TEST_ASSERT_EQUAL(1, countdown_clamp_seconds(1));
  TEST_ASSERT_EQUAL(5, countdown_clamp_seconds(5));
  TEST_ASSERT_EQUAL(10, countdown_clamp_seconds(10));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 3: STEP SCREEN LOGIC
// ═══════════════════════════════════════════════════════════════════════════════

void test_step_angle_valid_normal() {
  TEST_ASSERT_TRUE(step_angle_valid(90.0f));
  TEST_ASSERT_TRUE(step_angle_valid(360.0f));
  TEST_ASSERT_TRUE(step_angle_valid(0.1f));
}

void test_step_angle_valid_max() {
  TEST_ASSERT_TRUE(step_angle_valid(3600.0f));
}

void test_step_angle_invalid_zero() {
  TEST_ASSERT_FALSE(step_angle_valid(0.0f));
}

void test_step_angle_invalid_negative() {
  TEST_ASSERT_FALSE(step_angle_valid(-10.0f));
}

void test_step_angle_invalid_over_max() {
  TEST_ASSERT_FALSE(step_angle_valid(3601.0f));
}

void test_step_total_angle() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 900.0f, step_compute_total_angle(90.0f, 10));
}

void test_step_total_angle_single() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, step_compute_total_angle(45.0f, 1));
}

void test_step_total_steps() {
  long steps = step_compute_total_steps(90.0f, 1, GEAR_RATIO, D_RULLE, D_EMNE, STEPS_1_8, 1.0f);
  TEST_ASSERT_TRUE(steps > 0);
}

void test_step_total_steps_repeats() {
  long s1 = step_compute_total_steps(90.0f, 1, GEAR_RATIO, D_RULLE, D_EMNE, STEPS_1_8, 1.0f);
  long s5 = step_compute_total_steps(90.0f, 5, GEAR_RATIO, D_RULLE, D_EMNE, STEPS_1_8, 1.0f);
  TEST_ASSERT_EQUAL(s1 * 5, s5);
}

void test_step_total_steps_with_cal() {
  long s1 = step_compute_total_steps(90.0f, 1, GEAR_RATIO, D_RULLE, D_EMNE, STEPS_1_8, 1.0f);
  long s12 = step_compute_total_steps(90.0f, 1, GEAR_RATIO, D_RULLE, D_EMNE, STEPS_1_8, 1.2f);
  float ratio = (float)s12 / (float)s1;
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.2f, ratio);
}

void test_step_duration_basic() {
  float dur = step_compute_duration(360.0f, 1, 0.0f, 1.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 60.0f, dur);
}

void test_step_duration_with_dwell() {
  float dur_no_dwell = step_compute_duration(90.0f, 4, 0.0f, 0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  float dur_with_dwell = step_compute_duration(90.0f, 4, 2.0f, 0.5f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 6.0f, dur_with_dwell - dur_no_dwell);
}

void test_step_duration_zero_rpm() {
  float dur = step_compute_duration(90.0f, 1, 0.0f, 0.0f, GEAR_RATIO, D_EMNE, D_RULLE, STEPS_1_8);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, dur);
}

void test_step_progress_arc_zero() {
  TEST_ASSERT_EQUAL(0, step_progress_arc(0.0f, 90.0f));
}

void test_step_progress_arc_half() {
  TEST_ASSERT_EQUAL(180, step_progress_arc(45.0f, 90.0f));
}

void test_step_progress_arc_full() {
  TEST_ASSERT_EQUAL(360, step_progress_arc(90.0f, 90.0f));
}

void test_step_progress_arc_over() {
  TEST_ASSERT_EQUAL(360, step_progress_arc(100.0f, 90.0f));
}

void test_step_arc_angle_clamped_normal() {
  TEST_ASSERT_EQUAL(90, step_arc_angle_clamped(90.0f));
}

void test_step_arc_angle_clamped_over() {
  TEST_ASSERT_EQUAL(360, step_arc_angle_clamped(720.0f));
}

void test_step_arc_angle_clamped_negative() {
  TEST_ASSERT_EQUAL(0, step_arc_angle_clamped(-10.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 4: SYSINFO SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void test_uptime_zero() {
  uint32_t h, m, s;
  uptime_format(0, &h, &m, &s);
  TEST_ASSERT_EQUAL(0, h);
  TEST_ASSERT_EQUAL(0, m);
  TEST_ASSERT_EQUAL(0, s);
}

void test_uptime_59s() {
  uint32_t h, m, s;
  uptime_format(59, &h, &m, &s);
  TEST_ASSERT_EQUAL(0, h);
  TEST_ASSERT_EQUAL(0, m);
  TEST_ASSERT_EQUAL(59, s);
}

void test_uptime_1h() {
  uint32_t h, m, s;
  uptime_format(3600, &h, &m, &s);
  TEST_ASSERT_EQUAL(1, h);
  TEST_ASSERT_EQUAL(0, m);
  TEST_ASSERT_EQUAL(0, s);
}

void test_uptime_mixed() {
  uint32_t h, m, s;
  uptime_format(3661, &h, &m, &s);
  TEST_ASSERT_EQUAL(1, h);
  TEST_ASSERT_EQUAL(1, m);
  TEST_ASSERT_EQUAL(1, s);
}

void test_uptime_large() {
  uint32_t h, m, s;
  uptime_format(86400, &h, &m, &s);
  TEST_ASSERT_EQUAL(24, h);
  TEST_ASSERT_EQUAL(0, m);
  TEST_ASSERT_EQUAL(0, s);
}

void test_heap_percent_half() {
  TEST_ASSERT_EQUAL(50, heap_percent(50000, 100000));
}

void test_heap_percent_full() {
  TEST_ASSERT_EQUAL(100, heap_percent(100000, 100000));
}

void test_heap_percent_zero() {
  TEST_ASSERT_EQUAL(0, heap_percent(0, 100000));
}

void test_heap_percent_div_zero() {
  TEST_ASSERT_EQUAL(0, heap_percent(1000, 0));
}

void test_core_load_idle() {
  TEST_ASSERT_EQUAL(0, core_load_percent(1000, 1000));
}

void test_core_load_full() {
  TEST_ASSERT_EQUAL(100, core_load_percent(0, 1000));
}

void test_core_load_half() {
  TEST_ASSERT_EQUAL(50, core_load_percent(500, 1000));
}

void test_core_load_div_zero() {
  TEST_ASSERT_EQUAL(0, core_load_percent(0, 0));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 5: WIFI RSSI
// ═══════════════════════════════════════════════════════════════════════════════

void test_rssi_strong() {
  TEST_ASSERT_EQUAL(4, rssi_to_bars(-30));
  TEST_ASSERT_EQUAL(4, rssi_to_bars(-49));
}

void test_rssi_good() {
  TEST_ASSERT_EQUAL(3, rssi_to_bars(-50));
  TEST_ASSERT_EQUAL(3, rssi_to_bars(-69));
}

void test_rssi_fair() {
  TEST_ASSERT_EQUAL(2, rssi_to_bars(-70));
  TEST_ASSERT_EQUAL(2, rssi_to_bars(-84));
}

void test_rssi_weak() {
  TEST_ASSERT_EQUAL(1, rssi_to_bars(-85));
  TEST_ASSERT_EQUAL(1, rssi_to_bars(-100));
}

void test_rssi_boundary_50() {
  TEST_ASSERT_EQUAL(3, rssi_to_bars(-50));
  TEST_ASSERT_EQUAL(4, rssi_to_bars(-49));
}

void test_rssi_boundary_70() {
  TEST_ASSERT_EQUAL(2, rssi_to_bars(-70));
  TEST_ASSERT_EQUAL(3, rssi_to_bars(-69));
}

void test_rssi_boundary_85() {
  TEST_ASSERT_EQUAL(1, rssi_to_bars(-85));
  TEST_ASSERT_EQUAL(2, rssi_to_bars(-84));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 6: CALIBRATION SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void test_cal_steps_per_deg_unity() {
  float spd = cal_steps_per_deg(STEPS_1_8, GEAR_RATIO, 1.0f);
  float expected = (float)STEPS_1_8 * GEAR_RATIO / 360.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, spd);
}

void test_cal_steps_per_deg_with_factor() {
  float spd1 = cal_steps_per_deg(STEPS_1_8, GEAR_RATIO, 1.0f);
  float spd12 = cal_steps_per_deg(STEPS_1_8, GEAR_RATIO, 1.2f);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, spd1 * 1.2f, spd12);
}

void test_cal_error_degrees_unity() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, cal_error_degrees(1.0f));
}

void test_cal_error_degrees_above() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 36.0f, cal_error_degrees(1.1f));
}

void test_cal_error_degrees_below() {
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -36.0f, cal_error_degrees(0.9f));
}

void test_cal_tolerance_pass() {
  TEST_ASSERT_TRUE(cal_is_within_tolerance(1.001f, 0.5f));
}

void test_cal_tolerance_fail() {
  TEST_ASSERT_FALSE(cal_is_within_tolerance(1.1f, 0.5f));
}

void test_cal_tolerance_exact() {
  // Slightly inside tolerance — float precision makes exact boundary unreliable
  float factor = 1.0f + 0.4f / 360.0f;
  TEST_ASSERT_TRUE(cal_is_within_tolerance(factor, 0.5f));
}

void test_cal_bar_pct_zero_error() {
  TEST_ASSERT_EQUAL(0, cal_bar_pct(1.0f, 0.5f));
}

void test_cal_bar_pct_full_error() {
  float factor_for_0_5_deg = 1.0f + 0.5f / 360.0f;
  int pct = cal_bar_pct(factor_for_0_5_deg, 0.5f);
  TEST_ASSERT_INT_WITHIN(2, 100, pct);
}

void test_cal_bar_pct_over() {
  TEST_ASSERT_EQUAL(100, cal_bar_pct(1.5f, 0.5f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 7: DISPLAY BRIGHTNESS
// ═══════════════════════════════════════════════════════════════════════════════

void test_brightness_raw_to_pct_full() {
  TEST_ASSERT_EQUAL(100, brightness_raw_to_pct(255));
}

void test_brightness_raw_to_pct_half() {
  int pct = brightness_raw_to_pct(128);
  TEST_ASSERT_INT_WITHIN(1, 50, pct);
}

void test_brightness_raw_to_pct_low_floors_to_20() {
  TEST_ASSERT_EQUAL(20, brightness_raw_to_pct(10));
  TEST_ASSERT_EQUAL(20, brightness_raw_to_pct(0));
}

void test_brightness_pct_to_raw_100() {
  TEST_ASSERT_EQUAL(255, brightness_pct_to_raw(100));
}

void test_brightness_pct_to_raw_50() {
  TEST_ASSERT_INT_WITHIN(1, 127, brightness_pct_to_raw(50));
}

void test_brightness_pct_to_raw_0() {
  TEST_ASSERT_EQUAL(0, brightness_pct_to_raw(0));
}

void test_brightness_roundtrip() {
  uint8_t raw = brightness_pct_to_raw(75);
  int pct = brightness_raw_to_pct(raw);
  TEST_ASSERT_INT_WITHIN(2, 75, pct);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 8: BOOT PROGRESS
// ═══════════════════════════════════════════════════════════════════════════════

void test_boot_clamp_normal() {
  TEST_ASSERT_EQUAL(50, boot_clamp_progress(50));
}

void test_boot_clamp_below() {
  TEST_ASSERT_EQUAL(0, boot_clamp_progress(-10));
}

void test_boot_clamp_above() {
  TEST_ASSERT_EQUAL(100, boot_clamp_progress(150));
}

void test_boot_clamp_boundaries() {
  TEST_ASSERT_EQUAL(0, boot_clamp_progress(0));
  TEST_ASSERT_EQUAL(100, boot_clamp_progress(100));
}

void test_boot_increment_normal() {
  TEST_ASSERT_EQUAL(60, boot_increment(50, 10));
}

void test_boot_increment_overflow() {
  TEST_ASSERT_EQUAL(100, boot_increment(95, 20));
}

void test_boot_increment_underflow() {
  TEST_ASSERT_EQUAL(0, boot_increment(5, -10));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 9: MAIN SCREEN
// ═══════════════════════════════════════════════════════════════════════════════

void test_state_color_idle() {
  TEST_ASSERT_EQUAL(0, get_state_color_index(0));
}

void test_state_color_running() {
  TEST_ASSERT_EQUAL(1, get_state_color_index(1));
}

void test_state_color_estop() {
  TEST_ASSERT_EQUAL(2, get_state_color_index(6));
}

void test_state_color_pulse() {
  TEST_ASSERT_EQUAL(0, get_state_color_index(2));
}

void test_state_color_step() {
  TEST_ASSERT_EQUAL(0, get_state_color_index(3));
}

void test_state_color_jog() {
  TEST_ASSERT_EQUAL(0, get_state_color_index(4));
}

void test_state_color_stopping() {
  TEST_ASSERT_EQUAL(0, get_state_color_index(5));
}

void test_rpm_arc_value_zero() {
  TEST_ASSERT_EQUAL(0, rpm_arc_value(0.0f));
}

void test_rpm_arc_value_half() {
  TEST_ASSERT_EQUAL(50, rpm_arc_value(0.5f));
}

void test_rpm_arc_value_max() {
  TEST_ASSERT_EQUAL(100, rpm_arc_value(1.0f));
}

void test_rpm_adjust_up() {
  float r = rpm_adjust(0.5f, 0.1f, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, r);
}

void test_rpm_adjust_down() {
  float r = rpm_adjust(0.5f, -0.1f, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.4f, r);
}

void test_rpm_adjust_clamp_min() {
  float r = rpm_adjust(MIN_RPM, -0.1f, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MIN_RPM, r);
}

void test_rpm_adjust_clamp_max() {
  float r = rpm_adjust(MAX_RPM, 0.1f, MIN_RPM, MAX_RPM);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, r);
}

void test_is_running_state() {
  TEST_ASSERT_FALSE(is_running_state(0));  // IDLE
  TEST_ASSERT_TRUE(is_running_state(1));   // RUNNING
  TEST_ASSERT_TRUE(is_running_state(2));   // PULSE
  TEST_ASSERT_TRUE(is_running_state(3));   // STEP
  TEST_ASSERT_TRUE(is_running_state(4));   // JOG
  TEST_ASSERT_FALSE(is_running_state(5));  // STOPPING
  TEST_ASSERT_FALSE(is_running_state(6));  // ESTOP
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 10: PROGRAMS SCREEN — format_preset_details
// ═══════════════════════════════════════════════════════════════════════════════

void test_format_details_continuous() {
  char buf[128];
  format_preset_details(buf, sizeof(buf), 0, 0.5f, 0, 0, 0, 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Continuous"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "0.50"));
}

void test_format_details_pulse_finite() {
  char buf[128];
  format_preset_details(buf, sizeof(buf), 1, 0.3f, 500, 1000, 0, 10);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Pulse"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "10 cyc"));
}

void test_format_details_pulse_infinite() {
  char buf[128];
  format_preset_details(buf, sizeof(buf), 1, 0.3f, 500, 1000, 0, 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "INF"));
}

void test_format_details_step() {
  char buf[128];
  format_preset_details(buf, sizeof(buf), 2, 0.5f, 0, 0, 90.0f, 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Step"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "90.0"));
}

void test_format_details_unknown() {
  char buf[128];
  format_preset_details(buf, sizeof(buf), 99, 0.5f, 0, 0, 0, 0);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Unknown"));
}

void test_format_details_truncation() {
  char buf[5];
  format_preset_details(buf, sizeof(buf), 0, 0.5f, 0, 0, 0, 0);
  TEST_ASSERT_EQUAL(0, buf[4]);
  TEST_ASSERT_TRUE(strlen(buf) <= 4);
}

void test_pulse_bar_rpm_below_min() {
  TEST_ASSERT_EQUAL(0, pulse_bar_percent_rpm(-0.1f, MIN_RPM, MAX_RPM));
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST RUNNER
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
  UNITY_BEGIN();

  // Section 1: Pulse computation (22 tests)
  RUN_TEST(test_pulse_duty_equal);
  RUN_TEST(test_pulse_duty_all_on);
  RUN_TEST(test_pulse_duty_all_off);
  RUN_TEST(test_pulse_duty_both_zero);
  RUN_TEST(test_pulse_duty_75_25);
  RUN_TEST(test_pulse_duty_short_cycle);
  RUN_TEST(test_pulse_cycle_time_1s);
  RUN_TEST(test_pulse_cycle_time_10s);
  RUN_TEST(test_pulse_cycle_time_zero);
  RUN_TEST(test_pulse_frequency_1hz);
  RUN_TEST(test_pulse_frequency_5hz);
  RUN_TEST(test_pulse_frequency_zero);
  RUN_TEST(test_pulse_total_time_finite);
  RUN_TEST(test_pulse_total_time_infinite);
  RUN_TEST(test_pulse_total_time_one_cycle);
  RUN_TEST(test_pulse_bar_time_at_min);
  RUN_TEST(test_pulse_bar_time_at_max);
  RUN_TEST(test_pulse_bar_time_at_mid);
  RUN_TEST(test_pulse_bar_time_below_min);
  RUN_TEST(test_pulse_bar_time_above_max);
  RUN_TEST(test_pulse_bar_rpm_at_min);
  RUN_TEST(test_pulse_bar_rpm_at_max);
  RUN_TEST(test_pulse_bar_rpm_at_mid);

  // Section 2: Timer/countdown (13 tests)
  RUN_TEST(test_countdown_color_full);
  RUN_TEST(test_countdown_color_two_thirds);
  RUN_TEST(test_countdown_color_at_66);
  RUN_TEST(test_countdown_color_at_33);
  RUN_TEST(test_countdown_color_zero);
  RUN_TEST(test_countdown_color_total_zero);
  RUN_TEST(test_countdown_arc_full);
  RUN_TEST(test_countdown_arc_half);
  RUN_TEST(test_countdown_arc_zero);
  RUN_TEST(test_countdown_arc_negative);
  RUN_TEST(test_countdown_clamp_low);
  RUN_TEST(test_countdown_clamp_high);
  RUN_TEST(test_countdown_clamp_valid);

  // Section 3: Step screen (18 tests)
  RUN_TEST(test_step_angle_valid_normal);
  RUN_TEST(test_step_angle_valid_max);
  RUN_TEST(test_step_angle_invalid_zero);
  RUN_TEST(test_step_angle_invalid_negative);
  RUN_TEST(test_step_angle_invalid_over_max);
  RUN_TEST(test_step_total_angle);
  RUN_TEST(test_step_total_angle_single);
  RUN_TEST(test_step_total_steps);
  RUN_TEST(test_step_total_steps_repeats);
  RUN_TEST(test_step_total_steps_with_cal);
  RUN_TEST(test_step_duration_basic);
  RUN_TEST(test_step_duration_with_dwell);
  RUN_TEST(test_step_duration_zero_rpm);
  RUN_TEST(test_step_progress_arc_zero);
  RUN_TEST(test_step_progress_arc_half);
  RUN_TEST(test_step_progress_arc_full);
  RUN_TEST(test_step_progress_arc_over);
  RUN_TEST(test_step_arc_angle_clamped_normal);
  RUN_TEST(test_step_arc_angle_clamped_over);
  RUN_TEST(test_step_arc_angle_clamped_negative);

  // Section 4: Sysinfo (9 tests)
  RUN_TEST(test_uptime_zero);
  RUN_TEST(test_uptime_59s);
  RUN_TEST(test_uptime_1h);
  RUN_TEST(test_uptime_mixed);
  RUN_TEST(test_uptime_large);
  RUN_TEST(test_heap_percent_half);
  RUN_TEST(test_heap_percent_full);
  RUN_TEST(test_heap_percent_zero);
  RUN_TEST(test_heap_percent_div_zero);
  RUN_TEST(test_core_load_idle);
  RUN_TEST(test_core_load_full);
  RUN_TEST(test_core_load_half);
  RUN_TEST(test_core_load_div_zero);

  // Section 5: WiFi RSSI (7 tests)
  RUN_TEST(test_rssi_strong);
  RUN_TEST(test_rssi_good);
  RUN_TEST(test_rssi_fair);
  RUN_TEST(test_rssi_weak);
  RUN_TEST(test_rssi_boundary_50);
  RUN_TEST(test_rssi_boundary_70);
  RUN_TEST(test_rssi_boundary_85);

  // Section 6: Calibration (11 tests)
  RUN_TEST(test_cal_steps_per_deg_unity);
  RUN_TEST(test_cal_steps_per_deg_with_factor);
  RUN_TEST(test_cal_error_degrees_unity);
  RUN_TEST(test_cal_error_degrees_above);
  RUN_TEST(test_cal_error_degrees_below);
  RUN_TEST(test_cal_tolerance_pass);
  RUN_TEST(test_cal_tolerance_fail);
  RUN_TEST(test_cal_tolerance_exact);
  RUN_TEST(test_cal_bar_pct_zero_error);
  RUN_TEST(test_cal_bar_pct_full_error);
  RUN_TEST(test_cal_bar_pct_over);

  // Section 7: Display brightness (7 tests)
  RUN_TEST(test_brightness_raw_to_pct_full);
  RUN_TEST(test_brightness_raw_to_pct_half);
  RUN_TEST(test_brightness_raw_to_pct_low_floors_to_20);
  RUN_TEST(test_brightness_pct_to_raw_100);
  RUN_TEST(test_brightness_pct_to_raw_50);
  RUN_TEST(test_brightness_pct_to_raw_0);
  RUN_TEST(test_brightness_roundtrip);

  // Section 8: Boot progress (7 tests)
  RUN_TEST(test_boot_clamp_normal);
  RUN_TEST(test_boot_clamp_below);
  RUN_TEST(test_boot_clamp_above);
  RUN_TEST(test_boot_clamp_boundaries);
  RUN_TEST(test_boot_increment_normal);
  RUN_TEST(test_boot_increment_overflow);
  RUN_TEST(test_boot_increment_underflow);

  // Section 9: Main screen (14 tests)
  RUN_TEST(test_state_color_idle);
  RUN_TEST(test_state_color_running);
  RUN_TEST(test_state_color_estop);
  RUN_TEST(test_state_color_pulse);
  RUN_TEST(test_state_color_step);
  RUN_TEST(test_state_color_jog);
  RUN_TEST(test_state_color_stopping);
  RUN_TEST(test_rpm_arc_value_zero);
  RUN_TEST(test_rpm_arc_value_half);
  RUN_TEST(test_rpm_arc_value_max);
  RUN_TEST(test_rpm_adjust_up);
  RUN_TEST(test_rpm_adjust_down);
  RUN_TEST(test_rpm_adjust_clamp_min);
  RUN_TEST(test_rpm_adjust_clamp_max);
  RUN_TEST(test_is_running_state);

  // Section 10: Programs format (6 tests)
  RUN_TEST(test_format_details_continuous);
  RUN_TEST(test_format_details_pulse_finite);
  RUN_TEST(test_format_details_pulse_infinite);
  RUN_TEST(test_format_details_step);
  RUN_TEST(test_format_details_unknown);
  RUN_TEST(test_format_details_truncation);

  // Section 11: Edge cases (1 test)
  RUN_TEST(test_pulse_bar_rpm_below_min);

  return UNITY_END();
}
