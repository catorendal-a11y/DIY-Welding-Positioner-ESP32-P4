// PC simulator Arduino compatibility stubs

#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "freertos/FreeRTOS.h"

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define IRAM_ATTR

class SimSerial {
public:
  void begin(unsigned long) {}
  void flush() { fflush(stdout); }
  void print(const char* s) { fputs(s, stdout); }
  void println(const char* s) { fputs(s, stdout); fputc('\n', stdout); }
  void printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
};

extern SimSerial Serial;

class SimEsp {
public:
  [[noreturn]] void restart() {
    std::exit(0);
  }
};

extern SimEsp ESP;

inline uint32_t millis() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline uint32_t micros() {
  static const auto start = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
}

inline void delay(uint32_t ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int digitalRead(int pin) {
  // Safe defaults for simulator diagnostics: NC E-stop clear, driver alarm clear, ENA disabled.
  (void)pin;
  return HIGH;
}
inline int analogRead(int) { return 2048; }

template <typename T>
inline T constrain(T value, T low, T high) {
  return std::max(low, std::min(value, high));
}

inline size_t strlcpy(char* dst, const char* src, size_t dstSize) {
  size_t srcLen = src ? strlen(src) : 0;
  if (dstSize == 0) return srcLen;
  size_t copyLen = std::min(srcLen, dstSize - 1);
  if (copyLen > 0 && src) memcpy(dst, src, copyLen);
  dst[copyLen] = '\0';
  return srcLen;
}
