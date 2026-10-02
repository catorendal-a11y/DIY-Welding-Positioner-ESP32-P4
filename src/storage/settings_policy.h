#pragma once
#include <cstdint>
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
