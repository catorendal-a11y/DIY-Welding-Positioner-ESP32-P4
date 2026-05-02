// PC simulator FreeRTOS compatibility stubs

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <thread>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef void* TaskHandle_t;

#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

struct SimSemaphore {
  std::recursive_mutex mutex;
};

typedef SimSemaphore* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
  return new SimSemaphore();
}

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  return new SimSemaphore();
}

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t) {
  if (handle) handle->mutex.lock();
  return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
  if (handle) handle->mutex.unlock();
  return pdTRUE;
}

inline void vTaskDelay(TickType_t ticks) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

inline TickType_t xTaskGetTickCount() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return (TickType_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline int xPortGetCoreID() {
  return 1;
}

inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) {
  return 8192;
}

typedef struct {
  const char* pcTaskName;
  uint32_t ulRunTimeCounter;
  BaseType_t xCoreID;
} TaskStatus_t;

inline UBaseType_t uxTaskGetNumberOfTasks() {
  return 2;
}

inline UBaseType_t uxTaskGetSystemState(TaskStatus_t* array, UBaseType_t count, uint32_t* totalRunTime) {
  static uint32_t runtime = 1000;
  runtime += 200;
  if (totalRunTime) *totalRunTime = runtime;
  if (array && count >= 2) {
    array[0] = {"IDLE0", runtime / 3, 0};
    array[1] = {"IDLE1", runtime / 2, 1};
    return 2;
  }
  return 0;
}

inline void* pvPortMalloc(size_t size) {
  return malloc(size);
}

inline void vPortFree(void* ptr) {
  free(ptr);
}
