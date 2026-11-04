#pragma once
#include <cmath>
#include <cstdint>
inline bool settings_mechanics_valid(float roller_mm, float gear_ratio) {
  return std::isfinite(roller_mm) && std::isfinite(gear_ratio) && roller_mm >= 10.0f && roller_mm <= 1000.0f &&
         gear_ratio >= 1.0f && gear_ratio <= 1000.0f;
}
inline uint16_t settings_dim_seconds(int value) {
  // Version 1 stored 300 in a uint8_t. 44 was never a selectable value.
  if (value == 44) return 300;
  switch (value) {
    case 0:
    case 30:
    case 60:
    case 120:
    case 300:
      return uint16_t(value);
    default:
      return 60;
  }
}
