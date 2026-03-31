// TIG Rotator Controller - LVGL Hardware Abstraction Layer
// ESP32-P4: LVGL 8.x RGB565 with PSRAM buffers, MIPI-DSI DPI panel flush
// Matches JC4880P433C BSP configuration

#pragma once
#include <lvgl.h>
#include "../config.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define LVGL_BUF_LINES  80   // 80 lines = 480×80×2 = 76800 bytes - fits in internal DMA-RAM

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_hal_init();
void lvgl_alloc_buffers();  // Allocate PSRAM buffers (NOT DMA - draw_bitmap handles DMA)
void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map);
void lvgl_touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data);
void dim_reset_activity();
void dim_update();
// External handles from display.cpp
extern esp_lcd_panel_handle_t   display_panel;
extern esp_lcd_touch_handle_t   display_touch;
extern void*                    display_framebuffer;  // DPI framebuffer pointer
