#pragma once
#include "esp_err.h"
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
struct esp_task_wdt_config_t { unsigned timeout_ms, idle_core_mask; bool trigger_panic; };
inline esp_err_t esp_task_wdt_reconfigure(const esp_task_wdt_config_t*) { return ESP_OK; }
inline esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t*) { return ESP_OK; }
inline esp_err_t esp_task_wdt_reset() { return ESP_OK; }
inline esp_err_t esp_task_wdt_add(void*) { return ESP_OK; }
