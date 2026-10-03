#pragma once
#include "lvgl.h"

// UI task only, under the LVGL lock. Each input device owns its release latch.
struct ScreenSaverInput {
  bool pressed = false;
  bool suppressUntilRelease = false;
};
void screen_saver_filter_input(lv_indev_data_t* data, ScreenSaverInput& input);
void screen_saver_request_preview();
void screen_saver_destroy();
bool screen_saver_visible();
bool screen_saver_available();
// Explicit time permits timeout/rollover checks without waiting minutes.
void screen_saver_update(uint32_t now);
void dim_reset_activity();
void dim_update();
