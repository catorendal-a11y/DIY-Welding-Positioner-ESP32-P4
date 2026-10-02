#pragma once
#include <atomic>
#include <cstdint>

// One writer, multiple requesters. A request arriving during a write remains
// pending even when that write succeeds. A failure retains the request.
class SaveRequest {
 public:
  explicit SaveRequest(uint32_t interval) : base(interval), backoff(interval) {}
  uint32_t request() { return requested.fetch_add(1) + 1u; }
  bool pending() const { return requested.load() != committed.load() || writing.load(); }
  bool saved(uint32_t ticket) const {
    return static_cast<int32_t>(committed.load() - ticket) >= 0;
  }
  bool failed() const { return error.load(); }
  bool begin(uint32_t now) {
    if (now - last < backoff || requested.load() == committed.load()) return false;
    writing.store(true);
    inFlight = requested.load();
    last = now;
    return true;
  }
  void complete(bool success) {
    error.store(!success);
    if (!success) {
      backoff = backoff < 15000u ? backoff * 2u : 30000u;
    } else {
      committed.store(inFlight);
      backoff = base;
    }
    writing.store(false);
  }

 private:
  std::atomic<bool> writing{false}, error{false};
  std::atomic<uint32_t> requested{0}, committed{0};
  uint32_t inFlight = 0;
  const uint32_t base;
  uint32_t backoff, last = 0;
};
