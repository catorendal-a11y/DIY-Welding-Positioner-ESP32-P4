#pragma once

#include "esp_err.h"

typedef void* esp_lcd_panel_handle_t;

typedef struct {
  int dummy;
} esp_lcd_dpi_panel_event_data_t;

inline esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t, int, int, int, int, const void*) {
  return ESP_OK;
}
