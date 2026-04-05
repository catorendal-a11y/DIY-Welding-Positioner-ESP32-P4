// TIG Rotator Controller - Pure HAL Logic for Testing
// Extracted from lvgl_hal.cpp for native unit testing (no hardware dependencies)

#pragma once
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH COORDINATE TRANSFORM
// Portrait (px, py) → landscape (799 - py, px)
// Mirrors lvgl_touchpad_read_cb() in lvgl_hal.cpp
// ───────────────────────────────────────────────────────────────────────────────

inline int touch_portrait_to_landscape_x(int portrait_y) {
  return 799 - portrait_y;
}

inline int touch_portrait_to_landscape_y(int portrait_x) {
  return portrait_x;
}

// ───────────────────────────────────────────────────────────────────────────────
// FLUSH ROTATION — 90 CW landscape→portrait buffer index
// Mirrors the rotation loop in lvgl_flush_cb() in lvgl_hal.cpp:
//   rot_buf[(x2 - lx) * H + r] = src[r * W + c]
// Given landscape coordinates (lx, ly), area bounds (x1, x2, y1), and area H:
//   row in portrait buffer = x2 - lx
//   col in portrait buffer = ly - y1
//   linear index = row * H + col
// ───────────────────────────────────────────────────────────────────────────────

inline int flush_rotate_index(int lx, int ly, int x2, int y1, int area_H) {
  int row = x2 - lx;
  int col = ly - y1;
  return row * area_H + col;
}

// Portrait rectangle from landscape area bounds
// landscape area: x1..x2, y1..y2 -> portrait: px1=y1, py1=799-x2, px2=y2, py2=799-x1
inline void flush_portrait_rect(int x1, int y1, int x2, int y2,
                                int* px1, int* py1, int* px2, int* py2) {
  *px1 = y1;
  *py1 = 799 - x2;
  *px2 = y2;
  *py2 = 799 - x1;
}
