// TIG Rotator Controller - Cross-core shared atomics (definitions)
// See app_state.h for documentation.

#include "app_state.h"
#include "config.h"
#include <Arduino.h>

std::atomic<bool> g_estopPending{false};
std::atomic<uint32_t> g_estopTriggerMs{0};
std::atomic<bool> g_uiResetPending{false};

std::atomic<bool> g_wakePending{false};

std::atomic<bool> g_dir_switch_cache{true};
std::atomic<bool> g_flashWriting{false};
std::atomic<bool> g_screenRedraw{false};

[[noreturn]] void fatal_halt(const char* reason) {
  // Fail closed; operator must repair the cause before restarting.
  digitalWrite(PIN_ENA, HIGH);
  LOG_E("FATAL: %s — motion disabled", reason ? reason : "(unknown)");
  Serial.flush();
  delay(100);
  // Stay disabled: a corrupt configuration must not cause a reboot/start loop.
  for (;;) {
    delay(1000);
  }
}
