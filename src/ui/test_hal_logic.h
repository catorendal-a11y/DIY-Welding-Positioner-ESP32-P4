// TIG Rotator Controller - Pure HAL Logic
// Called by lvgl_hal.cpp and tested natively (no hardware dependencies).

#pragma once
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH COORDINATE TRANSFORM
// Portrait (px, py) → landscape (799 - py, px)
// Used by lvgl_touchpad_read_cb() in lvgl_hal.cpp
// ───────────────────────────────────────────────────────────────────────────────

inline int touch_portrait_to_landscape_x(int portrait_y) {
  return 799 - portrait_y;
}

inline int touch_portrait_to_landscape_y(int portrait_x) {
  return portrait_x;
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH READ FAILURE HOLD — used by lvgl_touchpad_read_cb() in lvgl_hal.cpp
// A failed GT911 read (HF/I2C noise) is not a lift: reporting RELEASED would make
// LVGL fire CLICKED and re-hit-test the next good read. A held press is kept only
// briefly after the last good read, so a lost release cannot keep a jog running.
// ───────────────────────────────────────────────────────────────────────────────

static constexpr uint32_t TOUCH_FAIL_HOLD_MS = 150u;

inline bool touch_hold_press_on_failed_read(bool lastPressed, uint32_t nowMs, uint32_t lastGoodMs) {
  return lastPressed && (uint32_t)(nowMs - lastGoodMs) <= TOUCH_FAIL_HOLD_MS;
}

// ───────────────────────────────────────────────────────────────────────────────
// FLUSH RECTANGLE — used by lvgl_flush_cb(); the pixel rotation itself is
// rotate_rgb565_to_portrait() in rgb565_rotation.h.
// Portrait rectangle from landscape area bounds
// landscape area: x1..x2, y1..y2 -> portrait: px1=y1, py1=799-x2, px2=y2, py2=799-x1
inline void flush_portrait_rect(int x1, int y1, int x2, int y2,
                                int* px1, int* py1, int* px2, int* py2) {
  *px1 = y1;
  *py1 = 799 - x2;
  *px2 = y2;
  *py2 = 799 - x1;
}
