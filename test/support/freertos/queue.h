#pragma once
#include <deque>
#include <vector>
#include <cstring>
#include <freertos/FreeRTOS.h>
#define pdPASS pdTRUE
struct TestQueue { size_t limit, size; std::deque<std::vector<uint8_t>> entries; };
using QueueHandle_t = TestQueue*;
inline QueueHandle_t xQueueCreate(size_t count, size_t size) { return new TestQueue{count, size, {}}; }
inline void xQueueReset(QueueHandle_t q) { q->entries.clear(); }
inline int xQueueSend(QueueHandle_t q, const void* data, TickType_t) {
  if (q->entries.size() >= q->limit) return pdFALSE;
  const auto* bytes = static_cast<const uint8_t*>(data);
  q->entries.emplace_back(bytes, bytes + q->size); return pdPASS;
}
inline int xQueueReceive(QueueHandle_t q, void* data, TickType_t) {
  if (q->entries.empty()) return pdFALSE;
  std::memcpy(data, q->entries.front().data(), q->size); q->entries.pop_front(); return pdTRUE;
}
inline void vTaskDelayUntil(TickType_t* tick, TickType_t period) { *tick += period; }
