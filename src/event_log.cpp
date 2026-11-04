// Event Log - Small RAM ring buffer for operator-visible runtime events
#include "event_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// Kept across panic/watchdog resets (not power loss) so a crash leaves its history.
struct EventLogStore {
  uint32_t magic;
  size_t next, count;
  EventLogEntry entries[EVENT_LOG_CAPACITY];
};
static constexpr uint32_t kEventLogMagic = 0x45564C47u;
#if defined(ARDUINO_ARCH_ESP32)
#include "esp_attr.h"
#include "esp_system.h"
static __NOINIT_ATTR EventLogStore s_log;
static bool event_log_survived_reset() { return esp_reset_reason() != ESP_RST_POWERON; }
#else
static EventLogStore s_log{};
static bool event_log_survived_reset() { return false; }
#endif
static SemaphoreHandle_t s_mutex = nullptr;
static std::atomic<uint32_t> s_version{0};
static std::atomic<uint32_t> s_dropped{0};
uint32_t event_log_dropped() { return s_dropped.load(); }

static bool event_log_lock() { return s_mutex != nullptr && xSemaphoreTake(s_mutex, 0) == pdTRUE; }

static void event_log_unlock() {
  if (s_mutex) {
    xSemaphoreGive(s_mutex);
  }
}

void event_log_init() {
  if (s_mutex == nullptr) {
    s_mutex = xSemaphoreCreateMutex();
  }
  if (event_log_survived_reset() && s_log.magic == kEventLogMagic && s_log.next < EVENT_LOG_CAPACITY &&
      s_log.count <= EVENT_LOG_CAPACITY) {
    for (auto& entry : s_log.entries) entry.text[EVENT_LOG_TEXT_LEN - 1] = '\0';
    event_log_add("BOOT (LOG RESTORED)");
    return;
  }
  s_log.magic = kEventLogMagic;
  event_log_clear();
  event_log_add("BOOT");
}

void event_log_add(const char* text) {
  if (text == nullptr) return;
  if (!event_log_lock()) { s_dropped.fetch_add(1); s_version.fetch_add(1); return; }

  EventLogEntry& entry = s_log.entries[s_log.next];
  entry.ms = millis();
  snprintf(entry.text, sizeof(entry.text), "%s", text);

  s_log.next = (s_log.next + 1) % EVENT_LOG_CAPACITY;
  if (s_log.count < EVENT_LOG_CAPACITY) {
    s_log.count++;
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
  size_t count = 0;
  uint32_t version = 0;
  event_log_try_snapshot(out, max_entries, &count, &version);
  return count;
}
bool event_log_try_snapshot(EventLogEntry* out, size_t max_entries, size_t* count, uint32_t* version) {
  if (!out || !max_entries || !count || !version || !event_log_lock()) return false;

  size_t n = s_log.count < max_entries ? s_log.count : max_entries;
  for (size_t i = 0; i < n; i++) {
    size_t idx = (s_log.next + EVENT_LOG_CAPACITY - 1 - i) % EVENT_LOG_CAPACITY;
    out[i] = s_log.entries[idx];
  }

  *count = n;
  *version = s_version.load();
  event_log_unlock();
  return true;
}

uint32_t event_log_version() { return s_version.load(std::memory_order_acquire); }

void event_log_clear() {
  if (!event_log_lock()) return;

  memset(s_log.entries, 0, sizeof(s_log.entries));
  s_log.next = 0;
  s_log.count = 0;
  s_version.fetch_add(1, std::memory_order_release);

  event_log_unlock();
}
