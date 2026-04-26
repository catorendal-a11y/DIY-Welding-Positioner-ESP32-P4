// Event Log - Small RAM ring buffer for operator-visible runtime events
#include "event_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static EventLogEntry s_entries[EVENT_LOG_CAPACITY] = {};
static size_t s_next = 0;
static size_t s_count = 0;
static SemaphoreHandle_t s_mutex = nullptr;
static std::atomic<uint32_t> s_version{0};

static bool event_log_lock() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
  return s_mutex != nullptr && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void event_log_unlock() {
  if (s_mutex) {
    xSemaphoreGive(s_mutex);
  }
}

void event_log_init() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
  event_log_clear();
  event_log_add("BOOT");
}

void event_log_add(const char* text) {
  if (text == nullptr) return;
  if (!event_log_lock()) return;

  EventLogEntry& entry = s_entries[s_next];
  entry.ms = millis();
  snprintf(entry.text, sizeof(entry.text), "%s", text);

  s_next = (s_next + 1) % EVENT_LOG_CAPACITY;
  if (s_count < EVENT_LOG_CAPACITY) {
    s_count++;
  }
  s_version.fetch_add(1, std::memory_order_release);

  event_log_unlock();
}

void event_log_addf(const char* fmt, ...) {
  if (fmt == nullptr) return;

  char buf[EVENT_LOG_TEXT_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  event_log_add(buf);
}

size_t event_log_snapshot(EventLogEntry* out, size_t max_entries) {
  if (out == nullptr || max_entries == 0) return 0;
  if (!event_log_lock()) return 0;

  size_t n = s_count < max_entries ? s_count : max_entries;
  for (size_t i = 0; i < n; i++) {
    size_t idx = (s_next + EVENT_LOG_CAPACITY - 1 - i) % EVENT_LOG_CAPACITY;
    out[i] = s_entries[idx];
  }

  event_log_unlock();
  return n;
}

uint32_t event_log_version() {
  return s_version.load(std::memory_order_acquire);
}

void event_log_clear() {
  if (!event_log_lock()) return;

  memset(s_entries, 0, sizeof(s_entries));
  s_next = 0;
  s_count = 0;
  s_version.fetch_add(1, std::memory_order_release);

  event_log_unlock();
}
