// TIG Rotator Controller - LVGL Hardware Abstraction Layer
// ESP32-P4: LVGL 8.x with PSRAM buffers, MIPI-DSI DPI panel flush

#pragma once
#include <lvgl.h>
#include "../config.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define LVGL_BUFFER_HEIGHT  80   // Buffer height in pixels (2 buffers × 80 × 800 × 2 = 250KB)

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_hal_init();
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void lvgl_touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

// External handles from display.cpp
extern esp_lcd_panel_handle_t   display_panel;
extern esp_lcd_touch_handle_t   display_touch;
