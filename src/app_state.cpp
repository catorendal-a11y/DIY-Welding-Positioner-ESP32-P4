// TIG Rotator Controller - Cross-core shared atomics (definitions)
// See app_state.h for documentation.

#include "app_state.h"
#include "config.h"
#include <Arduino.h>

std::atomic<bool>     g_estopPending{false};
std::atomic<uint32_t> g_estopTriggerMs{0};
std::atomic<bool>     g_uiResetPending{false};

std::atomic<bool>     g_wakePending{false};

std::atomic<bool>     g_dir_switch_cache{true};
std::atomic<bool>     g_flashWriting{false};
std::atomic<bool>     g_screenRedraw{false};

std::atomic<bool>     motorConfigApplyPending{false};

[[noreturn]] void fatal_halt(const char* reason) {
  // LOG_E is compiled into release builds (see config.h). Emit once, drain,
  // then reboot. ESTOP hardware path is independent of this (ISR already ran
  // if applicable), so motor is safe to stay disabled through the reboot.
  LOG_E("FATAL: %s — rebooting", reason ? reason : "(unknown)");
  Serial.flush();
  delay(100);
  ESP.restart();
  // ESP.restart never returns, but compiler doesn't know that.
  for (;;) { delay(1000); }
}
