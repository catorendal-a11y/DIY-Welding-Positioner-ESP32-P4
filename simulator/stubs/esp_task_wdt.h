#pragma once

#include "esp_err.h"

// Simulator tasks are never TWDT-subscribed; report like an unregistered task.
inline esp_err_t esp_task_wdt_delete(void*) { return ESP_ERR_NOT_FOUND; }
