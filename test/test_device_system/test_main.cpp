// Device System Tests - PSRAM, heap, chip info, FreeRTOS, atomics
// Runs on ESP32-P4 hardware via: pio test -e esp32p4-test -f test_device_system

#include <Arduino.h>
#include <unity.h>
#include <atomic>
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../../src/onchip_temp.h"

void setUp(void) {}
void tearDown(void) {}

// ────────────────────────────────────────────────────────────────────────────
// PSRAM
// ────────────────────────────────────────────────────────────────────────────

void test_psram_total_size(void) {
  size_t total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  TEST_ASSERT_GREATER_OR_EQUAL(8 * 1024 * 1024, total);
}

void test_psram_free_available(void) {
  size_t free_sz = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  TEST_ASSERT_GREATER_THAN(0, free_sz);
}

void test_psram_alloc_1mb(void) {
  size_t sz = 1024 * 1024;
  uint8_t* buf = (uint8_t*)heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
  TEST_ASSERT_NOT_NULL(buf);
  memset(buf, 0xA5, sz);
  TEST_ASSERT_EQUAL_UINT8(0xA5, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0xA5, buf[sz / 2]);
  TEST_ASSERT_EQUAL_UINT8(0xA5, buf[sz - 1]);
  heap_caps_free(buf);
}

void test_psram_aligned_alloc_64(void) {
  size_t sz = 128 * 1024;
  void* buf = heap_caps_aligned_calloc(64, 1, sz, MALLOC_CAP_SPIRAM);
  TEST_ASSERT_NOT_NULL(buf);
  TEST_ASSERT_EQUAL(0, (uintptr_t)buf % 64);
  heap_caps_free(buf);
}

void test_psram_largest_free_block(void) {
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  TEST_ASSERT_GREATER_OR_EQUAL(256 * 1024, largest);
}

// ────────────────────────────────────────────────────────────────────────────
// INTERNAL RAM
// ────────────────────────────────────────────────────────────────────────────

void test_internal_ram_free(void) {
  size_t free_sz = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  TEST_ASSERT_GREATER_THAN(0, free_sz);
}

void test_internal_ram_alloc(void) {
  uint8_t* buf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_INTERNAL);
  TEST_ASSERT_NOT_NULL(buf);
  buf[0] = 0xBB;
  buf[4095] = 0xCC;
  TEST_ASSERT_EQUAL_UINT8(0xBB, buf[0]);
  TEST_ASSERT_EQUAL_UINT8(0xCC, buf[4095]);
  heap_caps_free(buf);
}

// ────────────────────────────────────────────────────────────────────────────
// CHIP INFO
// ────────────────────────────────────────────────────────────────────────────

void test_chip_model_esp32p4(void) {
  esp_chip_info_t info;
  esp_chip_info(&info);
  TEST_ASSERT_EQUAL(CHIP_ESP32P4, info.model);
}

void test_flash_size(void) {
  uint32_t flashSize = ESP.getFlashChipSize();
  TEST_ASSERT_GREATER_OR_EQUAL((uint32_t)(16 * 1024 * 1024), flashSize);
}

void test_cpu_frequency(void) {
  uint32_t freq = ESP.getCpuFreqMHz();
  TEST_ASSERT_GREATER_OR_EQUAL(360, freq);
}

void test_sdk_version_not_null(void) {
  const char* ver = ESP.getSdkVersion();
  TEST_ASSERT_NOT_NULL(ver);
  TEST_ASSERT_GREATER_THAN(0, strlen(ver));
}

// ────────────────────────────────────────────────────────────────────────────
// TIMING & FREERTOS
// ────────────────────────────────────────────────────────────────────────────

void test_millis_advancing(void) {
  uint32_t t1 = millis();
  delay(15);
  uint32_t t2 = millis();
  TEST_ASSERT_GREATER_OR_EQUAL(10, t2 - t1);
}

void test_micros_advancing(void) {
  uint32_t t1 = micros();
  delayMicroseconds(500);
  uint32_t t2 = micros();
  TEST_ASSERT_GREATER_OR_EQUAL(400, t2 - t1);
}

void test_freertos_tick_nonzero(void) {
  delay(1);
  TickType_t ticks = xTaskGetTickCount();
  TEST_ASSERT_GREATER_THAN(0, ticks);
}

void test_freertos_mutex_create_take_give(void) {
  SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
  TEST_ASSERT_NOT_NULL(mtx);
  TEST_ASSERT_TRUE(xSemaphoreTake(mtx, pdMS_TO_TICKS(100)));
  TEST_ASSERT_TRUE(xSemaphoreGive(mtx));
  vSemaphoreDelete(mtx);
}

void test_freertos_recursive_mutex(void) {
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();
  TEST_ASSERT_NOT_NULL(mtx);
  TEST_ASSERT_TRUE(xSemaphoreTakeRecursive(mtx, pdMS_TO_TICKS(100)));
  TEST_ASSERT_TRUE(xSemaphoreTakeRecursive(mtx, pdMS_TO_TICKS(100)));
  TEST_ASSERT_TRUE(xSemaphoreGiveRecursive(mtx));
  TEST_ASSERT_TRUE(xSemaphoreGiveRecursive(mtx));
  vSemaphoreDelete(mtx);
}

void test_freertos_binary_semaphore(void) {
  SemaphoreHandle_t sem = xSemaphoreCreateBinary();
  TEST_ASSERT_NOT_NULL(sem);
  xSemaphoreGive(sem);
  TEST_ASSERT_TRUE(xSemaphoreTake(sem, pdMS_TO_TICKS(100)));
  TEST_ASSERT_FALSE(xSemaphoreTake(sem, pdMS_TO_TICKS(10)));
  vSemaphoreDelete(sem);
}

void test_stack_watermark_ok(void) {
  UBaseType_t mark = uxTaskGetStackHighWaterMark(NULL);
  TEST_ASSERT_GREATER_THAN(256, mark);
}

// ────────────────────────────────────────────────────────────────────────────
// ATOMIC OPERATIONS (RISC-V SMP correctness)
// ────────────────────────────────────────────────────────────────────────────

void test_atomic_int_store_load(void) {
  std::atomic<int> val{0};
  val.store(42, std::memory_order_release);
  TEST_ASSERT_EQUAL(42, val.load(std::memory_order_acquire));
}

void test_atomic_bool_store_load(void) {
  std::atomic<bool> flag{false};
  flag.store(true, std::memory_order_release);
  TEST_ASSERT_TRUE(flag.load(std::memory_order_acquire));
  flag.store(false, std::memory_order_release);
  TEST_ASSERT_FALSE(flag.load(std::memory_order_acquire));
}

void test_atomic_float_store_load(void) {
  std::atomic<float> rpm{0.0f};
  rpm.store(0.5f, std::memory_order_release);
  float loaded = rpm.load(std::memory_order_acquire);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, loaded);
}

void test_atomic_exchange(void) {
  std::atomic<int> val{10};
  int old = val.exchange(20, std::memory_order_acq_rel);
  TEST_ASSERT_EQUAL(10, old);
  TEST_ASSERT_EQUAL(20, val.load(std::memory_order_acquire));
}

void test_atomic_compare_exchange(void) {
  std::atomic<int> val{100};
  int expected = 100;
  bool ok = val.compare_exchange_strong(expected, 200, std::memory_order_acq_rel);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL(200, val.load(std::memory_order_acquire));

  expected = 999;
  ok = val.compare_exchange_strong(expected, 300, std::memory_order_acq_rel);
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL(200, expected);
}

// ────────────────────────────────────────────────────────────────────────────
// ON-CHIP TEMPERATURE
// ────────────────────────────────────────────────────────────────────────────

void test_onchip_temp_init_and_read(void) {
  onchip_temp_init();
  float tempC = 0.0f;
  bool ok = onchip_temp_get_celsius(&tempC);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_FLOAT_WITHIN(95.0f, 37.5f, tempC);
}

void test_onchip_temp_null_ptr(void) {
  onchip_temp_init();
  bool ok = onchip_temp_get_celsius(nullptr);
  TEST_ASSERT_FALSE(ok);
}

// ────────────────────────────────────────────────────────────────────────────
// ENTRY POINT
// ────────────────────────────────────────────────────────────────────────────

void setup() {
  delay(2000);
  UNITY_BEGIN();

  RUN_TEST(test_psram_total_size);
  RUN_TEST(test_psram_free_available);
  RUN_TEST(test_psram_alloc_1mb);
  RUN_TEST(test_psram_aligned_alloc_64);
  RUN_TEST(test_psram_largest_free_block);

  RUN_TEST(test_internal_ram_free);
  RUN_TEST(test_internal_ram_alloc);

  RUN_TEST(test_chip_model_esp32p4);
  RUN_TEST(test_flash_size);
  RUN_TEST(test_cpu_frequency);
  RUN_TEST(test_sdk_version_not_null);

  RUN_TEST(test_millis_advancing);
  RUN_TEST(test_micros_advancing);
  RUN_TEST(test_freertos_tick_nonzero);
  RUN_TEST(test_freertos_mutex_create_take_give);
  RUN_TEST(test_freertos_recursive_mutex);
  RUN_TEST(test_freertos_binary_semaphore);
  RUN_TEST(test_stack_watermark_ok);

  RUN_TEST(test_atomic_int_store_load);
  RUN_TEST(test_atomic_bool_store_load);
  RUN_TEST(test_atomic_float_store_load);
  RUN_TEST(test_atomic_exchange);
  RUN_TEST(test_atomic_compare_exchange);

  RUN_TEST(test_onchip_temp_init_and_read);
  RUN_TEST(test_onchip_temp_null_ptr);

  UNITY_END();
}

void loop() {
  delay(100);
}
