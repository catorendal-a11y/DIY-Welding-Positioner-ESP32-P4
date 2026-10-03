#pragma once
#include <cstddef>
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_PARTITION_SUBTYPE_DATA_NVS 2
struct esp_partition_t { size_t size; };
inline const esp_partition_t* esp_partition_find_first(int,int,const char*) { static esp_partition_t p{32768}; return &p; }
