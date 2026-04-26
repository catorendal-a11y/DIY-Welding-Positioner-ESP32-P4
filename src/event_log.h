// Event Log - Small RAM ring buffer for operator-visible runtime events
#pragma once

#include <Arduino.h>
#include <stddef.h>

#define EVENT_LOG_CAPACITY 16
#define EVENT_LOG_TEXT_LEN 56

struct EventLogEntry {
  uint32_t ms;
  char text[EVENT_LOG_TEXT_LEN];
};

void event_log_init();
void event_log_add(const char* text);
void event_log_addf(const char* fmt, ...);
size_t event_log_snapshot(EventLogEntry* out, size_t max_entries);
void event_log_clear();
