#pragma once

#include <cstddef>
#include <cstdlib>

inline size_t esp_get_free_heap_size() {
  return 320u * 1024u;
}

[[noreturn]] inline void esp_restart() {
  std::exit(0);
}
