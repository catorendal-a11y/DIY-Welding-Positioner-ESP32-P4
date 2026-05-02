#pragma once

#include "esp_err.h"
#include <cstdint>

typedef void* esp_lcd_touch_handle_t;

inline esp_err_t esp_lcd_touch_read_data(esp_lcd_touch_handle_t) {
  return ESP_OK;
}

inline bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t, uint16_t*, uint16_t*, uint16_t*, uint8_t* count, uint8_t) {
  if (count) *count = 0;
  return false;
}
