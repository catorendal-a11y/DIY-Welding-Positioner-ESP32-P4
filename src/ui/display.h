// TIG Rotator Controller - Display Driver
// ESP32-P4: ST7701S 480×800 via MIPI-DSI + GT911 touch
// GUITION JC4880P433 4.3" Touch Display Dev Board
// Uses ESP-IDF native MIPI-DSI and LCD panel APIs

#pragma once

#include <stdint.h>
#include "../config.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY HANDLES — accessible by LVGL HAL for flush/touch callbacks
// ───────────────────────────────────────────────────────────────────────────────
extern esp_lcd_panel_handle_t   display_panel;
extern esp_lcd_touch_handle_t   display_touch;
extern void*                    display_framebuffer;  // DPI framebuffer pointer

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void display_init();
i2c_master_bus_handle_t display_touch_i2c_bus_handle();
void display_set_brightness(uint8_t brightness);  // 0–255

// Fill physical panel with black (used at boot). Do not call during LVGL runtime — raw draw_bitmap races MIPI flush and can break touch.
void display_fill_black();

// Fill both DPI framebuffers with black to prevent blue flash during flash writes.
// Safe to call from storageTask (no LVGL interaction).
void display_fill_black_sync();

// Vsync callback for LVGL flush ready (MIPI DSI Video Mode requirement)
extern "C" bool display_lvgl_vsync_callback(
    esp_lcd_panel_handle_t panel,
    esp_lcd_dpi_panel_event_data_t *edata,
    void *user_ctx);

void display_register_lvgl_vsync(void *user_ctx);
