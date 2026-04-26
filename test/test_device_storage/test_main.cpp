// Device Storage Tests - NVS settings persistence, preset CRUD
// Runs on ESP32-P4 hardware via: pio test -e esp32p4-test -f test_device_storage
//
// These tests exercise storage_init(), storage_load/save_settings(),
// and storage_load/save_presets() against NVS on flash.
//
// IMPORTANT: Tests modify g_settings and g_presets, then restore defaults.

#include <Arduino.h>
#include <unity.h>
#include <cstring>
#include <nvs.h>
#include "../../src/storage/storage.h"
#include "../../src/config.h"

static SystemSettings s_defaultSettings;
static bool s_storageInited = false;

static void ensure_storage_init() {
  if (!s_storageInited) {
    storage_init();
    s_storageInited = true;
  }
}

void setUp(void) {
  ensure_storage_init();
}

void tearDown(void) {}

// Raw NVS writes (separate handle from Preferences) for corrupt/clamp tests
static void test_nvs_put_cfg_blob(const char* json, size_t len) {
  nvs_handle_t h;
  TEST_ASSERT_EQUAL(ESP_OK, nvs_open("wrot", NVS_READWRITE, &h));
  TEST_ASSERT_EQUAL(ESP_OK, nvs_set_blob(h, "cfg", json, len));
  TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(h));
  nvs_close(h);
}

static void test_nvs_put_prs_blob(const char* json, size_t len) {
  nvs_handle_t h;
  TEST_ASSERT_EQUAL(ESP_OK, nvs_open("wrot", NVS_READWRITE, &h));
  TEST_ASSERT_EQUAL(ESP_OK, nvs_set_blob(h, "prs", json, len));
  TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(h));
  nvs_close(h);
}

// ────────────────────────────────────────────────────────────────────────────
// NVS USAGE
// ────────────────────────────────────────────────────────────────────────────

void test_nvs_storage_reports_partition(void) {
  size_t used = 0, total = 0;
  storage_get_usage(&used, &total);
  TEST_ASSERT_GREATER_THAN(0, total);
}

void test_nvs_total_matches_partition_table(void) {
  size_t used = 0, total = 0;
  storage_get_usage(&used, &total);
  TEST_ASSERT_EQUAL(0x6000, total);
}

void test_nvs_used_not_greater_than_total(void) {
  size_t used = 0, total = 0;
  storage_get_usage(&used, &total);
  TEST_ASSERT_LESS_OR_EQUAL(total, used);
}

// ────────────────────────────────────────────────────────────────────────────
// SETTINGS DEFAULTS
// ────────────────────────────────────────────────────────────────────────────

void test_settings_default_acceleration(void) {
  TEST_ASSERT_GREATER_OR_EQUAL(1000, g_settings.acceleration);
  TEST_ASSERT_LESS_OR_EQUAL(30000, g_settings.acceleration);
}

void test_settings_default_microstep(void) {
  TEST_ASSERT_TRUE(
    g_settings.microstep == 4 || g_settings.microstep == 8 ||
    g_settings.microstep == 16 || g_settings.microstep == 32
  );
}

void test_settings_default_calibration(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 1.0f, g_settings.calibration_factor);
}

void test_settings_default_brightness_range(void) {
  TEST_ASSERT_GREATER_OR_EQUAL(10, g_settings.brightness);
  TEST_ASSERT_LESS_OR_EQUAL(255, g_settings.brightness);
}

void test_settings_default_countdown_range(void) {
  TEST_ASSERT_GREATER_OR_EQUAL(1, g_settings.countdown_seconds);
  TEST_ASSERT_LESS_OR_EQUAL(10, g_settings.countdown_seconds);
}

void test_settings_default_accent_color_range(void) {
  TEST_ASSERT_LESS_OR_EQUAL(7, g_settings.accent_color);
}

void test_settings_stepper_driver_valid(void) {
  TEST_ASSERT_TRUE(
    g_settings.stepper_driver == STEPPER_DRIVER_STANDARD ||
    g_settings.stepper_driver == STEPPER_DRIVER_DM542T
  );
}

// ────────────────────────────────────────────────────────────────────────────
// SETTINGS SAVE/LOAD ROUNDTRIP
// ────────────────────────────────────────────────────────────────────────────

void test_settings_save_load_roundtrip(void) {
  SystemSettings backup;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  backup = g_settings;
  xSemaphoreGive(g_settings_mutex);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.acceleration = 7777;
  g_settings.microstep = 16;
  g_settings.calibration_factor = 1.25f;
  g_settings.brightness = 200;
  g_settings.countdown_seconds = 7;
  g_settings.invert_direction = true;
  g_settings.accent_color = 3;
  g_settings.dim_timeout = 120;
  g_settings.dir_switch_enabled = false;
  g_settings.stepper_driver = STEPPER_DRIVER_STANDARD;
  g_settings.max_rpm = 2.5f;
  xSemaphoreGive(g_settings_mutex);

  storage_save_settings();
  delay(1200);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.acceleration = 9999;
  g_settings.microstep = 4;
  g_settings.calibration_factor = 0.5f;
  g_settings.brightness = 10;
  xSemaphoreGive(g_settings_mutex);

  bool ok = storage_load_settings();
  TEST_ASSERT_TRUE(ok);

  TEST_ASSERT_EQUAL(7777, g_settings.acceleration);
  TEST_ASSERT_EQUAL(16, g_settings.microstep);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.25f, g_settings.calibration_factor);
  TEST_ASSERT_EQUAL(200, g_settings.brightness);
  TEST_ASSERT_EQUAL(7, g_settings.countdown_seconds);
  TEST_ASSERT_TRUE(g_settings.invert_direction);
  TEST_ASSERT_EQUAL(3, g_settings.accent_color);
  TEST_ASSERT_EQUAL(120, g_settings.dim_timeout);
  TEST_ASSERT_FALSE(g_settings.dir_switch_enabled);
  TEST_ASSERT_EQUAL(STEPPER_DRIVER_STANDARD, g_settings.stepper_driver);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, g_settings.max_rpm);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings = backup;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
  delay(1200);
  storage_flush();
}

// ────────────────────────────────────────────────────────────────────────────
// SETTINGS CONSTRAINTS (verify clamping on load)
// ────────────────────────────────────────────────────────────────────────────

void test_settings_acceleration_clamped_on_load(void) {
  const char* raw = "{\"acceleration\":99999,\"microstep\":8,\"brightness\":255}";
  test_nvs_put_cfg_blob(raw, strlen(raw));

  storage_load_settings();
  TEST_ASSERT_EQUAL(30000, g_settings.acceleration);
}

void test_settings_brightness_clamped_on_load(void) {
  const char* raw = "{\"brightness\":0}";
  test_nvs_put_cfg_blob(raw, strlen(raw));

  storage_load_settings();
  TEST_ASSERT_GREATER_OR_EQUAL(10, g_settings.brightness);
}

void test_settings_max_rpm_clamped_on_load(void) {
  const char* raw = "{\"max_rpm\":10.0}";
  test_nvs_put_cfg_blob(raw, strlen(raw));
  storage_load_settings();
  TEST_ASSERT_FLOAT_WITHIN(0.001f, MAX_RPM, g_settings.max_rpm);

  const char* raw2 = "{\"max_rpm\":0.0001}";
  test_nvs_put_cfg_blob(raw2, strlen(raw2));
  storage_load_settings();
  TEST_ASSERT_FLOAT_WITHIN(0.0002f, MIN_RPM, g_settings.max_rpm);
}

void test_settings_countdown_clamped_on_load(void) {
  const char* raw = "{\"countdown_seconds\":50}";
  test_nvs_put_cfg_blob(raw, strlen(raw));

  storage_load_settings();
  TEST_ASSERT_LESS_OR_EQUAL(10, g_settings.countdown_seconds);
}

void test_settings_calibration_clamped_on_load(void) {
  const char* raw = "{\"calibration_factor\":5.0}";
  test_nvs_put_cfg_blob(raw, strlen(raw));

  storage_load_settings();
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5f, g_settings.calibration_factor);
}

void test_settings_stepper_driver_clamped_on_load(void) {
  const char* raw = "{\"stepper_driver\":9}";
  test_nvs_put_cfg_blob(raw, strlen(raw));

  storage_load_settings();
  TEST_ASSERT_EQUAL(STEPPER_DRIVER_DM542T, g_settings.stepper_driver);
}

// ────────────────────────────────────────────────────────────────────────────
// PRESET CRUD
// ────────────────────────────────────────────────────────────────────────────

void test_presets_initially_loads(void) {
  bool ok = storage_load_presets();
  TEST_ASSERT_TRUE(ok);
}

void test_presets_add_and_retrieve(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  size_t before = g_presets.size();
  Preset p = {};
  p.id = 200;
  strlcpy(p.name, "Test Preset", sizeof(p.name));
  p.mode = STATE_RUNNING;
  p.rpm = 0.5f;
  p.direction = 0;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);

  storage_load_presets();

  Preset out = {};
  bool found = storage_get_preset(200, &out);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_STRING("Test Preset", out.name);
  TEST_ASSERT_EQUAL(STATE_RUNNING, out.mode);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, out.rpm);

  storage_delete_preset(200);
  delay(600);
  storage_flush();
  (void)before;
}

void test_presets_delete(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  Preset p = {};
  p.id = 201;
  strlcpy(p.name, "DeleteMe", sizeof(p.name));
  p.mode = STATE_PULSE;
  p.rpm = 0.3f;
  p.pulse_on_ms = 500;
  p.pulse_off_ms = 500;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  bool deleted = storage_delete_preset(201);
  TEST_ASSERT_TRUE(deleted);
  delay(600);
  storage_flush();
  delay(100);

  Preset out = {};
  bool found = storage_get_preset(201, &out);
  TEST_ASSERT_FALSE(found);
}

void test_presets_max_limit(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  for (int i = 0; i < MAX_PRESETS; i++) {
    Preset p = {};
    p.id = (uint8_t)(100 + i);
    snprintf(p.name, sizeof(p.name), "P%d", i);
    p.mode = STATE_RUNNING;
    p.rpm = 0.1f + i * 0.05f;
    g_presets.push_back(p);
  }
  size_t count = g_presets.size();
  xSemaphoreGive(g_presets_mutex);

  TEST_ASSERT_EQUAL(MAX_PRESETS, count);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);

  storage_load_presets();

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  count = g_presets.size();
  xSemaphoreGive(g_presets_mutex);

  TEST_ASSERT_EQUAL(MAX_PRESETS, count);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);
  storage_save_presets();
  delay(600);
  storage_flush();
}

void test_preset_pulse_mode_roundtrip(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  Preset p = {};
  p.id = 202;
  strlcpy(p.name, "PulseTest", sizeof(p.name));
  p.mode = STATE_PULSE;
  p.rpm = 0.2f;
  p.pulse_on_ms = 1500;
  p.pulse_off_ms = 750;
  p.pulse_cycles = 10;
  p.direction = 1;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);
  storage_load_presets();

  Preset out = {};
  bool found = storage_get_preset(202, &out);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL(STATE_PULSE, out.mode);
  TEST_ASSERT_EQUAL(1500, out.pulse_on_ms);
  TEST_ASSERT_EQUAL(750, out.pulse_off_ms);
  TEST_ASSERT_EQUAL(10, out.pulse_cycles);
  TEST_ASSERT_EQUAL(1, out.direction);

  storage_delete_preset(202);
  delay(600);
  storage_flush();
}

void test_preset_step_mode_roundtrip(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  Preset p = {};
  p.id = 203;
  strlcpy(p.name, "StepTest", sizeof(p.name));
  p.mode = STATE_STEP;
  p.rpm = 0.4f;
  p.step_angle = 45.0f;
  p.workpiece_diameter_mm = 355.0f;
  p.step_repeats = 5;
  p.step_dwell_sec = 2.5f;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);
  storage_load_presets();

  Preset out = {};
  bool found = storage_get_preset(203, &out);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL(STATE_STEP, out.mode);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 45.0f, out.step_angle);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 355.0f, out.workpiece_diameter_mm);
  TEST_ASSERT_EQUAL(5, out.step_repeats);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.5f, out.step_dwell_sec);

  storage_delete_preset(203);
  delay(600);
  storage_flush();
}

void test_preset_name_sanitized(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  Preset p = {};
  p.id = 204;
  p.name[0] = 'A';
  p.name[1] = (char)0x80;
  p.name[2] = 'B';
  p.name[3] = '\0';
  p.mode = STATE_RUNNING;
  p.rpm = 0.3f;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);
  storage_load_presets();

  Preset out = {};
  bool found = storage_get_preset(204, &out);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL('A', out.name[0]);
  TEST_ASSERT_EQUAL('?', out.name[1]);
  TEST_ASSERT_EQUAL('B', out.name[2]);

  storage_delete_preset(204);
  delay(600);
  storage_flush();
}

void test_preset_rpm_clamped(void) {
  const char* raw = "[{\"id\":205,\"name\":\"Clamp\",\"mode\":1,\"rpm\":99.0}]";
  test_nvs_put_prs_blob(raw, strlen(raw));

  storage_load_presets();

  Preset out = {};
  bool found = storage_get_preset(205, &out);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, MAX_RPM, out.rpm);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);
  storage_save_presets();
  delay(600);
  storage_flush();
}

void test_preset_get_null_ptr_returns_false(void) {
  bool found = storage_get_preset(0, nullptr);
  TEST_ASSERT_FALSE(found);
}

// ────────────────────────────────────────────────────────────────────────────
// STORAGE FORMAT & RECOVERY
// ────────────────────────────────────────────────────────────────────────────

void test_storage_format_clears_data(void) {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  Preset p = {};
  p.id = 207;
  strlcpy(p.name, "WillBeErased", sizeof(p.name));
  p.mode = STATE_RUNNING;
  p.rpm = 0.5f;
  g_presets.push_back(p);
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  delay(600);
  storage_flush();
  delay(100);

  storage_format();

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  size_t count = g_presets.size();
  xSemaphoreGive(g_presets_mutex);

  TEST_ASSERT_EQUAL(0, count);
}

void test_storage_load_after_format(void) {
  storage_format();
  bool ok = storage_load_presets();
  TEST_ASSERT_TRUE(ok);

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  size_t count = g_presets.size();
  xSemaphoreGive(g_presets_mutex);
  TEST_ASSERT_EQUAL(0, count);

  ok = storage_load_settings();
  TEST_ASSERT_TRUE(ok);
}

// ────────────────────────────────────────────────────────────────────────────
// MUTEX INTEGRITY
// ────────────────────────────────────────────────────────────────────────────

void test_settings_mutex_exists(void) {
  TEST_ASSERT_NOT_NULL(g_settings_mutex);
}

void test_presets_mutex_exists(void) {
  TEST_ASSERT_NOT_NULL(g_presets_mutex);
}

void test_nvs_mutex_exists(void) {
  TEST_ASSERT_NOT_NULL(g_nvs_mutex);
}

// ────────────────────────────────────────────────────────────────────────────
// ENTRY POINT
// ────────────────────────────────────────────────────────────────────────────

void setup() {
  delay(2000);
  ensure_storage_init();
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  s_defaultSettings = g_settings;
  xSemaphoreGive(g_settings_mutex);

  UNITY_BEGIN();

  RUN_TEST(test_nvs_storage_reports_partition);
  RUN_TEST(test_nvs_total_matches_partition_table);
  RUN_TEST(test_nvs_used_not_greater_than_total);

  RUN_TEST(test_settings_default_acceleration);
  RUN_TEST(test_settings_default_microstep);
  RUN_TEST(test_settings_default_calibration);
  RUN_TEST(test_settings_default_brightness_range);
  RUN_TEST(test_settings_default_countdown_range);
  RUN_TEST(test_settings_default_accent_color_range);
  RUN_TEST(test_settings_stepper_driver_valid);

  RUN_TEST(test_settings_save_load_roundtrip);

  RUN_TEST(test_settings_acceleration_clamped_on_load);
  RUN_TEST(test_settings_brightness_clamped_on_load);
  RUN_TEST(test_settings_max_rpm_clamped_on_load);
  RUN_TEST(test_settings_countdown_clamped_on_load);
  RUN_TEST(test_settings_calibration_clamped_on_load);
  RUN_TEST(test_settings_stepper_driver_clamped_on_load);

  RUN_TEST(test_presets_initially_loads);
  RUN_TEST(test_presets_add_and_retrieve);
  RUN_TEST(test_presets_delete);
  RUN_TEST(test_presets_max_limit);
  RUN_TEST(test_preset_pulse_mode_roundtrip);
  RUN_TEST(test_preset_step_mode_roundtrip);
  RUN_TEST(test_preset_name_sanitized);
  RUN_TEST(test_preset_rpm_clamped);
  RUN_TEST(test_preset_get_null_ptr_returns_false);

  RUN_TEST(test_storage_format_clears_data);
  RUN_TEST(test_storage_load_after_format);

  RUN_TEST(test_settings_mutex_exists);
  RUN_TEST(test_presets_mutex_exists);
  RUN_TEST(test_nvs_mutex_exists);

  UNITY_END();

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings = s_defaultSettings;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
}

void loop() {
  delay(100);
}
