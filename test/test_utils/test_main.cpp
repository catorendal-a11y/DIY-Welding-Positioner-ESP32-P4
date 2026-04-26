// TIG Rotator Controller - Utility and Module Logic Unit Tests
// Tests storage validation, HAL transforms, sanitization
// Run: pio test -e native

#include <unity.h>
#include <cstring>
#include <cstdint>

#include "../../src/storage/test_storage_logic.h"
#include "../../src/ui/test_hal_logic.h"

void setUp(void) {}
void tearDown(void) {}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 1: SANITIZE ASCII
// ═══════════════════════════════════════════════════════════════════════════════

void test_sanitize_ascii_clean() {
  char buf[] = "Hello World";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("Hello World", buf);
}

void test_sanitize_ascii_control_chars() {
  char buf[] = "A\x01\x02\x03Z";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A???Z", buf);
}

void test_sanitize_ascii_high_bytes() {
  char buf[] = "A\x80\xFF Z";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A?? Z", buf);
}

void test_sanitize_ascii_newlines() {
  char buf[] = "Line\nTwo\rThree";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("Line?Two?Three", buf);
}

void test_sanitize_ascii_tab() {
  char buf[] = "A\tB";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A?B", buf);
}

void test_sanitize_ascii_space_ok() {
  char buf[] = "A B";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A B", buf);
}

void test_sanitize_ascii_tilde_ok() {
  char buf[] = "~test~";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("~test~", buf);
}

void test_sanitize_ascii_del_replaced() {
  char buf[4] = {'A', 0x7F, 'B', 0};
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("A?B", buf);
}

void test_sanitize_ascii_empty() {
  char buf[] = "";
  sanitize_ascii_testable(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_sanitize_ascii_len_limit() {
  char buf[] = "ABCDE\x01\x02";
  sanitize_ascii_testable(buf, 4);
  TEST_ASSERT_EQUAL('A', buf[0]);
  TEST_ASSERT_EQUAL('B', buf[1]);
  TEST_ASSERT_EQUAL('C', buf[2]);
  TEST_ASSERT_EQUAL('D', buf[3]);
  TEST_ASSERT_EQUAL('E', buf[4]);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 2: SETTINGS CONSTRAIN
// ═══════════════════════════════════════════════════════════════════════════════

void test_settings_accel_in_range() {
  TEST_ASSERT_EQUAL(5000, settings_constrain_acceleration(5000));
  TEST_ASSERT_EQUAL(1000, settings_constrain_acceleration(1000));
  TEST_ASSERT_EQUAL(20000, settings_constrain_acceleration(20000));
  TEST_ASSERT_EQUAL(30000, settings_constrain_acceleration(30000));
}

void test_settings_accel_below() {
  TEST_ASSERT_EQUAL(1000, settings_constrain_acceleration(0));
  TEST_ASSERT_EQUAL(1000, settings_constrain_acceleration(999));
}

void test_settings_accel_above() {
  TEST_ASSERT_EQUAL(30000, settings_constrain_acceleration(30001));
  TEST_ASSERT_EQUAL(30000, settings_constrain_acceleration(99999));
}

void test_settings_brightness_in_range() {
  TEST_ASSERT_EQUAL(150, settings_constrain_brightness(150));
  TEST_ASSERT_EQUAL(10, settings_constrain_brightness(10));
  TEST_ASSERT_EQUAL(255, settings_constrain_brightness(255));
}

void test_settings_brightness_below() {
  TEST_ASSERT_EQUAL(10, settings_constrain_brightness(0));
  TEST_ASSERT_EQUAL(10, settings_constrain_brightness(9));
}

void test_settings_countdown_in_range() {
  TEST_ASSERT_EQUAL(1, settings_constrain_countdown(1));
  TEST_ASSERT_EQUAL(5, settings_constrain_countdown(5));
  TEST_ASSERT_EQUAL(10, settings_constrain_countdown(10));
}

void test_settings_countdown_below() {
  TEST_ASSERT_EQUAL(1, settings_constrain_countdown(0));
}

void test_settings_countdown_above() {
  TEST_ASSERT_EQUAL(10, settings_constrain_countdown(11));
  TEST_ASSERT_EQUAL(10, settings_constrain_countdown(255));
}

void test_settings_accent_in_range() {
  TEST_ASSERT_EQUAL(0, settings_constrain_accent(0));
  TEST_ASSERT_EQUAL(3, settings_constrain_accent(3));
  TEST_ASSERT_EQUAL(7, settings_constrain_accent(7));
}

void test_settings_accent_above() {
  TEST_ASSERT_EQUAL(7, settings_constrain_accent(8));
  TEST_ASSERT_EQUAL(7, settings_constrain_accent(255));
}

void test_settings_calibration_in_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, settings_constrain_calibration(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, settings_constrain_calibration(0.5f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, settings_constrain_calibration(1.5f));
}

void test_settings_calibration_below() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, settings_constrain_calibration(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, settings_constrain_calibration(-1.0f));
}

void test_settings_calibration_above() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, settings_constrain_calibration(2.0f));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 3: PRESET CONSTRAIN
// ═══════════════════════════════════════════════════════════════════════════════

void test_preset_rpm_in_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, preset_constrain_rpm(0.5f, 0.01f, 3.0f));
}

void test_preset_rpm_below() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.01f, preset_constrain_rpm(0.0f, 0.01f, 3.0f));
}

void test_preset_rpm_above() {
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, preset_constrain_rpm(5.0f, 0.01f, 3.0f));
}

void test_preset_pulse_on_in_range() {
  TEST_ASSERT_EQUAL(500, preset_constrain_pulse_on(500));
}

void test_preset_pulse_on_below() {
  TEST_ASSERT_EQUAL(100, preset_constrain_pulse_on(0));
  TEST_ASSERT_EQUAL(100, preset_constrain_pulse_on(99));
}

void test_preset_pulse_on_above() {
  TEST_ASSERT_EQUAL(10000, preset_constrain_pulse_on(70000));
}

void test_preset_pulse_off_in_range() {
  TEST_ASSERT_EQUAL(1000, preset_constrain_pulse_off(1000));
}

void test_preset_pulse_off_below() {
  TEST_ASSERT_EQUAL(100, preset_constrain_pulse_off(0));
}

void test_preset_pulse_off_above() {
  TEST_ASSERT_EQUAL(10000, preset_constrain_pulse_off(100000));
}

void test_preset_step_angle_in_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, preset_constrain_step_angle(90.0f));
}

void test_preset_step_angle_below() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, preset_constrain_step_angle(0.0f));
}

void test_preset_step_angle_above() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 360.0f, preset_constrain_step_angle(500.0f));
}

void test_preset_workpiece_diameter_in_range() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 300.0f, preset_constrain_workpiece_diameter_mm(300.0f));
}

void test_preset_workpiece_diameter_default_for_invalid() {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, preset_constrain_workpiece_diameter_mm(0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, preset_constrain_workpiece_diameter_mm(30000.0f));
}

void test_preset_timer_ms_in_range() {
  TEST_ASSERT_EQUAL(60000, preset_constrain_timer_ms(60000));
}

void test_preset_timer_ms_below() {
  TEST_ASSERT_EQUAL(1, preset_constrain_timer_ms(0));
}

void test_preset_timer_ms_above() {
  TEST_ASSERT_EQUAL(3600000, preset_constrain_timer_ms(5000000));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 4: RECONNECT BACKOFF
// ═══════════════════════════════════════════════════════════════════════════════

void test_reconnect_backoff_initial() {
  TEST_ASSERT_EQUAL(60000, reconnect_backoff_interval(30000));
}

void test_reconnect_backoff_double() {
  TEST_ASSERT_EQUAL(120000, reconnect_backoff_interval(60000));
}

void test_reconnect_backoff_cap() {
  TEST_ASSERT_EQUAL(300000, reconnect_backoff_interval(300000));
}

void test_reconnect_backoff_near_cap() {
  // 200000 < 300000, so it doubles to 400000 (cap applies on NEXT iteration)
  TEST_ASSERT_EQUAL(400000, reconnect_backoff_interval(200000));
}

void test_reconnect_backoff_reset() {
  TEST_ASSERT_EQUAL(30000, reconnect_backoff_reset());
}

void test_reconnect_backoff_sequence() {
  uint32_t interval = 30000;
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(60000, interval);
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(120000, interval);
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(240000, interval);
  // 240000 < 300000, so doubles to 480000
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(480000, interval);
  // 480000 >= 300000, caps at 300000
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(300000, interval);
  interval = reconnect_backoff_interval(interval);
  TEST_ASSERT_EQUAL(300000, interval);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 5: TOUCH COORDINATE TRANSFORM
// ═══════════════════════════════════════════════════════════════════════════════

void test_touch_x_origin() {
  TEST_ASSERT_EQUAL(799, touch_portrait_to_landscape_x(0));
}

void test_touch_x_max() {
  TEST_ASSERT_EQUAL(0, touch_portrait_to_landscape_x(799));
}

void test_touch_x_center() {
  TEST_ASSERT_EQUAL(399, touch_portrait_to_landscape_x(400));
}

void test_touch_y_origin() {
  TEST_ASSERT_EQUAL(0, touch_portrait_to_landscape_y(0));
}

void test_touch_y_max() {
  TEST_ASSERT_EQUAL(479, touch_portrait_to_landscape_y(479));
}

void test_touch_y_identity() {
  TEST_ASSERT_EQUAL(200, touch_portrait_to_landscape_y(200));
}

void test_touch_roundtrip() {
  int px = 200, py = 300;
  int lx = touch_portrait_to_landscape_x(py);
  int ly = touch_portrait_to_landscape_y(px);
  TEST_ASSERT_EQUAL(499, lx);
  TEST_ASSERT_EQUAL(200, ly);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SECTION 6: FLUSH ROTATION INDEX
// ═══════════════════════════════════════════════════════════════════════════════

void test_flush_rotate_index_origin() {
  int idx = flush_rotate_index(0, 0, 9, 0, 10);
  TEST_ASSERT_EQUAL(90, idx);
}

void test_flush_rotate_index_corner() {
  int idx = flush_rotate_index(9, 9, 9, 0, 10);
  TEST_ASSERT_EQUAL(9, idx);
}

void test_flush_rotate_portrait_rect_full() {
  int px1, py1, px2, py2;
  flush_portrait_rect(0, 0, 799, 479, &px1, &py1, &px2, &py2);
  TEST_ASSERT_EQUAL(0, px1);
  TEST_ASSERT_EQUAL(0, py1);
  TEST_ASSERT_EQUAL(479, px2);
  TEST_ASSERT_EQUAL(799, py2);
}

void test_flush_rotate_portrait_rect_partial() {
  int px1, py1, px2, py2;
  flush_portrait_rect(100, 50, 200, 100, &px1, &py1, &px2, &py2);
  TEST_ASSERT_EQUAL(50, px1);
  TEST_ASSERT_EQUAL(599, py1);
  TEST_ASSERT_EQUAL(100, px2);
  TEST_ASSERT_EQUAL(699, py2);
}

void test_flush_rotate_portrait_rect_single_pixel() {
  int px1, py1, px2, py2;
  flush_portrait_rect(400, 240, 400, 240, &px1, &py1, &px2, &py2);
  TEST_ASSERT_EQUAL(240, px1);
  TEST_ASSERT_EQUAL(399, py1);
  TEST_ASSERT_EQUAL(240, px2);
  TEST_ASSERT_EQUAL(399, py2);
}

// ═══════════════════════════════════════════════════════════════════════════════
// TEST RUNNER
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
  UNITY_BEGIN();

  // Section 1: Sanitize ASCII (10 tests)
  RUN_TEST(test_sanitize_ascii_clean);
  RUN_TEST(test_sanitize_ascii_control_chars);
  RUN_TEST(test_sanitize_ascii_high_bytes);
  RUN_TEST(test_sanitize_ascii_newlines);
  RUN_TEST(test_sanitize_ascii_tab);
  RUN_TEST(test_sanitize_ascii_space_ok);
  RUN_TEST(test_sanitize_ascii_tilde_ok);
  RUN_TEST(test_sanitize_ascii_del_replaced);
  RUN_TEST(test_sanitize_ascii_empty);
  RUN_TEST(test_sanitize_ascii_len_limit);

  // Section 2: Settings constrain (12 tests)
  RUN_TEST(test_settings_accel_in_range);
  RUN_TEST(test_settings_accel_below);
  RUN_TEST(test_settings_accel_above);
  RUN_TEST(test_settings_brightness_in_range);
  RUN_TEST(test_settings_brightness_below);
  RUN_TEST(test_settings_countdown_in_range);
  RUN_TEST(test_settings_countdown_below);
  RUN_TEST(test_settings_countdown_above);
  RUN_TEST(test_settings_accent_in_range);
  RUN_TEST(test_settings_accent_above);
  RUN_TEST(test_settings_calibration_in_range);
  RUN_TEST(test_settings_calibration_below);
  RUN_TEST(test_settings_calibration_above);

  // Section 3: Preset constrain
  RUN_TEST(test_preset_rpm_in_range);
  RUN_TEST(test_preset_rpm_below);
  RUN_TEST(test_preset_rpm_above);
  RUN_TEST(test_preset_pulse_on_in_range);
  RUN_TEST(test_preset_pulse_on_below);
  RUN_TEST(test_preset_pulse_on_above);
  RUN_TEST(test_preset_pulse_off_in_range);
  RUN_TEST(test_preset_pulse_off_below);
  RUN_TEST(test_preset_pulse_off_above);
  RUN_TEST(test_preset_step_angle_in_range);
  RUN_TEST(test_preset_step_angle_below);
  RUN_TEST(test_preset_step_angle_above);
  RUN_TEST(test_preset_workpiece_diameter_in_range);
  RUN_TEST(test_preset_workpiece_diameter_default_for_invalid);
  RUN_TEST(test_preset_timer_ms_in_range);
  RUN_TEST(test_preset_timer_ms_below);
  RUN_TEST(test_preset_timer_ms_above);

  // Section 4: Reconnect backoff (6 tests)
  RUN_TEST(test_reconnect_backoff_initial);
  RUN_TEST(test_reconnect_backoff_double);
  RUN_TEST(test_reconnect_backoff_cap);
  RUN_TEST(test_reconnect_backoff_near_cap);
  RUN_TEST(test_reconnect_backoff_reset);
  RUN_TEST(test_reconnect_backoff_sequence);

  // Section 5: Touch coordinate transform (7 tests)
  RUN_TEST(test_touch_x_origin);
  RUN_TEST(test_touch_x_max);
  RUN_TEST(test_touch_x_center);
  RUN_TEST(test_touch_y_origin);
  RUN_TEST(test_touch_y_max);
  RUN_TEST(test_touch_y_identity);
  RUN_TEST(test_touch_roundtrip);

  // Section 6: Flush rotation (5 tests)
  RUN_TEST(test_flush_rotate_index_origin);
  RUN_TEST(test_flush_rotate_index_corner);
  RUN_TEST(test_flush_rotate_portrait_rect_full);
  RUN_TEST(test_flush_rotate_portrait_rect_partial);
  RUN_TEST(test_flush_rotate_portrait_rect_single_pixel);

  return UNITY_END();
}
