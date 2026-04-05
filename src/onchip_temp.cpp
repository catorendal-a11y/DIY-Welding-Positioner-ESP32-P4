// On-chip die temperature — single driver for UI surfaces

#include "onchip_temp.h"
#include <Arduino.h>
#include "driver/temperature_sensor.h"

static temperature_sensor_handle_t s_handle = nullptr;

void onchip_temp_init() {
  if (s_handle) return;
  temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if (temperature_sensor_install(&cfg, &s_handle) == ESP_OK) {
    temperature_sensor_enable(s_handle);
  }
}

bool onchip_temp_get_celsius(float* outC) {
  if (!s_handle || !outC) return false;
  return temperature_sensor_get_celsius(s_handle, outC) == ESP_OK;
}
