#pragma once
#include "esp_err.h"
struct nvs_stats_t { size_t used_entries,total_entries; };
inline esp_err_t nvs_get_stats(const char*,nvs_stats_t* s) { s->used_entries=1; s->total_entries=100; return ESP_OK; }
