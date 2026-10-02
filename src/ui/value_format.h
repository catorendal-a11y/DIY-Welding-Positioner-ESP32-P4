#pragma once
#include <cmath>
#include <cstdio>
#include <cstddef>

inline float ui_rpm_increment(float rpm) { return rpm < 0.1f ? 0.001f : 0.01f; }

inline void ui_format_rpm(char* output, size_t size, float rpm) {
  if (!std::isfinite(rpm) || rpm < 0.0f) {
    std::snprintf(output, size, "--");
    return;
  }
  std::snprintf(output, size, rpm < 0.1f ? "%.3f" : "%.2f", static_cast<double>(rpm));
}
