// TIG Rotator Controller - Display Driver
// ESP32-P4: EK79007 480×800 via MIPI-DSI + GT911 touch
// Uses ESP-IDF native MIPI-DSI and LCD panel APIs

#pragma once

#include "../config.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY HANDLES — accessible by LVGL HAL for flush/touch callbacks
// ───────────────────────────────────────────────────────────────────────────────
extern esp_lcd_panel_handle_t   display_panel;
extern esp_lcd_touch_handle_t   display_touch;

// ───────────────────────────────────────────────────────────────────────────────
// FUNCTION DECLARATIONS
// ───────────────────────────────────────────────────────────────────────────────
void display_init();
void display_set_brightness(uint8_t brightness);  // 0–255
