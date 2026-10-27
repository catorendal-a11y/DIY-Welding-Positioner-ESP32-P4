#pragma once
#include <cstdint>

// Landscape (x, y) -> portrait (y, width - 1 - x), i.e. 90 degrees CCW.
inline void rotate_rgb565_to_portrait(const uint16_t* source, uint16_t* destination,
                                     int width, int height) {
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      destination[(width - 1 - column) * height + row] = source[row * width + column];
    }
  }
}
