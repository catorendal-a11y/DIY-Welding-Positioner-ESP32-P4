#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define MALLOC_CAP_INTERNAL 0x01
#define MALLOC_CAP_SPIRAM   0x02
#define MALLOC_CAP_DMA      0x04

inline size_t heap_caps_get_total_size(uint32_t caps) {
  return (caps & MALLOC_CAP_SPIRAM) ? (32u * 1024u * 1024u) : (512u * 1024u);
}

inline size_t heap_caps_get_free_size(uint32_t caps) {
  return (caps & MALLOC_CAP_SPIRAM) ? (24u * 1024u * 1024u) : (320u * 1024u);
}

inline void* heap_caps_malloc(size_t size, uint32_t) {
  return malloc(size);
}

inline void* heap_caps_aligned_calloc(size_t alignment, size_t count, size_t size, uint32_t) {
  (void)alignment;
  size_t bytes = count * size;
  return calloc(1, bytes);
}

inline void heap_caps_free(void* ptr) {
  free(ptr);
}
