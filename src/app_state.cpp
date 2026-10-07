// TIG Rotator Controller - Cross-core shared atomics (definitions)
// See app_state.h for documentation.

#include "app_state.h"
#include "config.h"
#include <Arduino.h>
#include "esp_err.h"
#include "esp_task_wdt.h"

std::atomic<bool> g_estopPending{false};
std::atomic<uint32_t> g_estopTriggerMs{0};
std::atomic<bool> g_uiResetPending{false};
std::atomic<uint32_t> g_inputHeartbeatMs{0};
std::atomic<bool> g_inputHeartbeatValid{false};
std::atomic<bool> g_restartRequired{false};

std::atomic<bool> g_wakePending{false};

std::atomic<bool> g_dir_switch_cache{true};
std::atomic<bool> g_flashWriting{false};
std::atomic<bool> g_screenRedraw{false};

[[noreturn]] void fatal_halt(const char* reason) {
  // Fail closed; operator must repair the cause before restarting.
  g_restartRequired.store(true, std::memory_order_release);
  digitalWrite(PIN_ENA, HIGH);
  LOG_E("FATAL: %s — motion disabled", reason ? reason : "(unknown)");
  Serial.flush();
  // A watchdog-subscribed caller stops feeding the TWDT in this loop and would
  // panic-reboot, defeating the no-reboot-loop intent; unsubscribe when the
  // calling task is registered. Unregistered tasks report ESP_ERR_NOT_FOUND.
  esp_err_t wdt = esp_task_wdt_delete(nullptr);
  if (wdt != ESP_OK && wdt != ESP_ERR_NOT_FOUND) {
    LOG_E("fatal_halt: TWDT unsubscribe failed: %s", esp_err_to_name(wdt));
  }
  // Stay disabled: a corrupt configuration must not cause a reboot/start loop.
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
