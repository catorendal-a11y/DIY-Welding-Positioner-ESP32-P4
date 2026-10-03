#pragma once
#include <atomic>
#include <cstdint>

// Try-only mailbox: no torn reads and no waiting in either task.
template <typename T> class SnapshotMailbox {
 public:
  bool publish(const T& value) {
    if (busy.test_and_set(std::memory_order_acquire)) return false;
    data = value;
    available = true;
    busy.clear(std::memory_order_release);
    return true;
  }
  bool read(T& value) {
    if (busy.test_and_set(std::memory_order_acquire)) return false;
    const bool ok = available;
    if (ok) value = data;
    busy.clear(std::memory_order_release);
    return ok;
  }
 private:
  std::atomic_flag busy = ATOMIC_FLAG_INIT;
  bool available = false;
  T data{};
};

inline bool control_timestamp_fresh(uint32_t now, uint32_t timestamp, bool valid) {
  return valid && uint32_t(now - timestamp) <= 100u;
}
