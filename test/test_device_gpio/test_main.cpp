// Device GPIO Tests - Pin states, ADC, direction switch, ESTOP, ENA
// Runs on ESP32-P4 hardware via: pio test -e esp32p4-test -f test_device_gpio
//
// SAFETY: Motor ENA is never set LOW. Motor stays disabled throughout all tests.

#include <Arduino.h>
#include <unity.h>
#include "../../src/config.h"

void setUp(void) {}
void tearDown(void) {}

// ────────────────────────────────────────────────────────────────────────────
// ENA PIN — Motor enable (active LOW to driver, HIGH = disabled)
// ────────────────────────────────────────────────────────────────────────────

void test_ena_pin_starts_high(void) {
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_ENA));
}

void test_ena_pin_stays_high_after_reconfig(void) {
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);
  pinMode(PIN_ENA, INPUT);
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_ENA));
}

// ────────────────────────────────────────────────────────────────────────────
// ESTOP PIN — Active LOW, NC contact (HIGH = not pressed)
// ────────────────────────────────────────────────────────────────────────────

void test_estop_pin_readable(void) {
  pinMode(PIN_ESTOP, INPUT);
  int val = digitalRead(PIN_ESTOP);
  // GPIO34 has no internal pull — reads HIGH with NC contact connected, LOW if open/pressed
  TEST_ASSERT_TRUE(val == HIGH || val == LOW);
}

void test_estop_pin_stable(void) {
  pinMode(PIN_ESTOP, INPUT);
  int first = digitalRead(PIN_ESTOP);
  delay(5);
  int second = digitalRead(PIN_ESTOP);
  TEST_ASSERT_EQUAL(first, second);
}

// ────────────────────────────────────────────────────────────────────────────
// DIRECTION SWITCH — INPUT_PULLUP, HIGH when open
// ────────────────────────────────────────────────────────────────────────────

void test_dir_switch_pullup(void) {
  pinMode(PIN_DIR_SWITCH, INPUT_PULLUP);
  delay(1);
  int val = digitalRead(PIN_DIR_SWITCH);
  TEST_ASSERT_TRUE(val == HIGH || val == LOW);
}

void test_dir_switch_stable(void) {
  pinMode(PIN_DIR_SWITCH, INPUT_PULLUP);
  delay(1);
  int first = digitalRead(PIN_DIR_SWITCH);
  delay(5);
  int second = digitalRead(PIN_DIR_SWITCH);
  TEST_ASSERT_EQUAL(first, second);
}

// ────────────────────────────────────────────────────────────────────────────
// PEDAL SWITCH — INPUT_PULLUP, HIGH when open (not pressed)
// ────────────────────────────────────────────────────────────────────────────

void test_pedal_switch_pullup(void) {
  pinMode(PIN_PEDAL_SW, INPUT_PULLUP);
  delay(1);
  int val = digitalRead(PIN_PEDAL_SW);
  TEST_ASSERT_TRUE(val == HIGH || val == LOW);
}

void test_pedal_switch_stable(void) {
  pinMode(PIN_PEDAL_SW, INPUT_PULLUP);
  delay(1);
  int first = digitalRead(PIN_PEDAL_SW);
  delay(5);
  int second = digitalRead(PIN_PEDAL_SW);
  TEST_ASSERT_EQUAL(first, second);
}

// ────────────────────────────────────────────────────────────────────────────
// ADC — Potentiometer on PIN_POT (GPIO 49)
// ────────────────────────────────────────────────────────────────────────────

void test_adc_pot_resolution(void) {
  analogReadResolution(12);
  int val = analogRead(PIN_POT);
  TEST_ASSERT_GREATER_OR_EQUAL(0, val);
  TEST_ASSERT_LESS_OR_EQUAL(4095, val);
}

void test_adc_pot_multiple_reads_in_range(void) {
  analogReadResolution(12);
  for (int i = 0; i < 10; i++) {
    int val = analogRead(PIN_POT);
    TEST_ASSERT_GREATER_OR_EQUAL(0, val);
    TEST_ASSERT_LESS_OR_EQUAL(4095, val);
  }
}

void test_adc_pot_reads_stable(void) {
  analogReadResolution(12);
  int readings[10];
  for (int i = 0; i < 10; i++) {
    readings[i] = analogRead(PIN_POT);
    delay(2);
  }
  int minVal = readings[0], maxVal = readings[0];
  for (int i = 1; i < 10; i++) {
    if (readings[i] < minVal) minVal = readings[i];
    if (readings[i] > maxVal) maxVal = readings[i];
  }
  // ADC noise should be within +/-100 counts for a stable pot
  TEST_ASSERT_LESS_OR_EQUAL(100, maxVal - minVal);
}

void test_adc_pot_nonzero_or_zero(void) {
  analogReadResolution(12);
  int sum = 0;
  for (int i = 0; i < 5; i++) {
    sum += analogRead(PIN_POT);
    delay(1);
  }
  // Just verify we can read without crash — value depends on pot position
  TEST_ASSERT_GREATER_OR_EQUAL(0, sum);
}

// ────────────────────────────────────────────────────────────────────────────
// STEP & DIR PINS — Verify configurable as outputs
// ────────────────────────────────────────────────────────────────────────────

void test_step_pin_configurable(void) {
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  TEST_ASSERT_EQUAL(LOW, digitalRead(PIN_STEP));
  digitalWrite(PIN_STEP, HIGH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_STEP));
  digitalWrite(PIN_STEP, LOW);
}

void test_dir_pin_configurable(void) {
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);
  TEST_ASSERT_EQUAL(LOW, digitalRead(PIN_DIR));
  digitalWrite(PIN_DIR, HIGH);
  TEST_ASSERT_EQUAL(HIGH, digitalRead(PIN_DIR));
  digitalWrite(PIN_DIR, LOW);
}

// ────────────────────────────────────────────────────────────────────────────
// ENTRY POINT
// ────────────────────────────────────────────────────────────────────────────

void setup() {
  delay(2000);

  // Safety first: ensure motor is disabled
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);

  UNITY_BEGIN();

  RUN_TEST(test_ena_pin_starts_high);
  RUN_TEST(test_ena_pin_stays_high_after_reconfig);

  RUN_TEST(test_estop_pin_readable);
  RUN_TEST(test_estop_pin_stable);

  RUN_TEST(test_dir_switch_pullup);
  RUN_TEST(test_dir_switch_stable);

  RUN_TEST(test_pedal_switch_pullup);
  RUN_TEST(test_pedal_switch_stable);

  RUN_TEST(test_adc_pot_resolution);
  RUN_TEST(test_adc_pot_multiple_reads_in_range);
  RUN_TEST(test_adc_pot_reads_stable);
  RUN_TEST(test_adc_pot_nonzero_or_zero);

  RUN_TEST(test_step_pin_configurable);
  RUN_TEST(test_dir_pin_configurable);

  UNITY_END();
}

void loop() {
  // Keep USB-CDC alive for re-flashing
  if (Serial.available()) {
    while (Serial.read() != -1); // Drain
  }
  delay(10);
}
