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
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
void lvgl_touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

// External handles from display.cpp
extern esp_lcd_panel_handle_t   display_panel;
extern esp_lcd_touch_handle_t   display_touch;
extern void*                    display_framebuffer;  // DPI framebuffer pointer
