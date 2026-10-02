#pragma once
#include <atomic>
#include <cstdint>

// One writer, multiple requesters. A request arriving during a write remains
// pending even when that write succeeds. A failure retains the request.
class SaveRequest {
 public:
  explicit SaveRequest(uint32_t interval) : base(interval), backoff(interval) {}
  void request() { dirty.store(true); }
  bool pending() const { return dirty.load() || writing.load(); }
  bool failed() const { return error.load(); }
  bool begin(uint32_t now) {
    if (now - last < backoff || !dirty.load()) return false;
    writing.store(true);
    if (!dirty.exchange(false)) {
      writing.store(false);
      return false;
    }
    last = now;
    return true;
  }
  void complete(bool success) {
    error.store(!success);
    if (!success) {
      dirty.store(true);
      backoff = backoff < 15000u ? backoff * 2u : 30000u;
    } else {
      backoff = base;
    }
    writing.store(false);
  }

 private:
  std::atomic<bool> dirty{false}, writing{false}, error{false};
  const uint32_t base;
  uint32_t backoff, last = 0;
};
