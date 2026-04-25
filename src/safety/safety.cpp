// TIG Rotator Controller - Safety System Implementation
// ESTOP ISR with < 1ms response, hardware watchdog

#include "safety.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../control/control.h"
#include <esp_task_wdt.h>

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS
// Cross-core atomics live in src/app_state.cpp (see app_state.h).
// ───────────────────────────────────────────────────────────────────────────────
static std::atomic<bool> estopLocked{false};
static FastAccelStepper* estopStepper = nullptr;
static std::atomic<bool> estopResetPending{false};
static std::atomic<uint8_t> s_faultReason{FAULT_NONE};

// DM542T ALM: open-drain active LOW on fault; INPUT_PULLUP on PIN_DRIVER_ALM.
static std::atomic<bool> s_driverAlarmLatched{false};
static uint16_t s_almLowMs = 0;
static uint16_t s_almHighMs = 0;

static void safety_set_fault_reason(FaultReason reason) {
  s_faultReason.store((uint8_t)reason, std::memory_order_release);
}

#if DEBUG_BUILD
static uint32_t g_estopISRCount  = 0;
static uint32_t g_estopConfirmed = 0;
#endif

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY INITIALIZATION — Cache stepper pointer for ISR
// ───────────────────────────────────────────────────────────────────────────────
void safety_cache_stepper() {
  if (g_stepperMutex) {
    xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  }
  estopStepper = motor_get_stepper();
  if (g_stepperMutex) {
    xSemaphoreGive(g_stepperMutex);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP INTERRUPT SERVICE ROUTINE
// Layer 1: < 0.5 ms response — Direct hardware action only.
// Invariants:
//   - No flash access (millis/forceStop/LOG_*) — cache may be disabled during
//     LittleFS/NVS writes, would crash IWDT.
//   - Only GPIO register writes + atomic flag stores are allowed here.
//   - g_estopTriggerMs is set by safetyTask (after flag load), never by ISR.
// ───────────────────────────────────────────────────────────────────────────────
void IRAM_ATTR estopISR() {
  GPIO.out1_w1ts.val = (1UL << (PIN_ENA - 32));
  g_estopPending.store(true, std::memory_order_release);
  g_wakePending.store(true, std::memory_order_release);
}

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void safety_init() {
  // Configure ESTOP pin as input with pull-up.
  // GPIO34 has NO internal pulls on ESP32-P4 — we rely on the external NC
  // contact pull-up. Sample a few times to reject startup glitches.
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  delay(2);
  uint8_t lowCount = 0;
  for (int i = 0; i < 3; i++) {
    if (digitalRead(PIN_ESTOP) == LOW) lowCount++;
    delayMicroseconds(500);
  }
  bool estopPressed = (lowCount >= 2);
  LOG_I("Safety init: ESTOP=%s (low samples %u/3)",
        estopPressed ? "PRESSED" : "OK", (unsigned)lowCount);

  if (estopPressed) {
    LOG_W("ESTOP pressed at boot — system locked");
    estopLocked.store(true, std::memory_order_release);
    safety_set_fault_reason(FAULT_ESTOP_PRESSED);
    g_estopPending.store(true, std::memory_order_release);
    g_estopTriggerMs.store(millis(), std::memory_order_release);
    g_wakePending.store(true, std::memory_order_release);
  }

  // Initialize watchdog
  safety_init_watchdog();
}

void safety_attach_estop() {
  // Attach interrupt with FALLING edge (active LOW)
  attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estopISR, FALLING);
  LOG_I("ESTOP interrupt attached (FALLING, priority 1)");
}

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP STATUS FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
bool safety_is_estop_active() {
  return (digitalRead(PIN_ESTOP) == LOW);
}

bool safety_is_driver_alarm_latched() {
  return s_driverAlarmLatched.load(std::memory_order_acquire);
}

bool safety_inhibit_motion() {
  return safety_is_estop_active() || s_driverAlarmLatched.load(std::memory_order_acquire);
}

bool safety_can_reset_from_overlay() {
  return (digitalRead(PIN_ESTOP) == HIGH) &&
         !s_driverAlarmLatched.load(std::memory_order_acquire);
}

bool safety_is_estop_locked() {
  return estopLocked.load(std::memory_order_acquire);
}

FaultReason safety_get_fault_reason() {
  uint8_t reason = s_faultReason.load(std::memory_order_acquire);
  if (reason > FAULT_WATCHDOG_RESET) {
    return FAULT_NONE;
  }
  return (FaultReason)reason;
}

const char* safety_fault_reason_name(FaultReason reason) {
  switch (reason) {
    case FAULT_NONE: return "NONE";
    case FAULT_ESTOP_PRESSED: return "E-STOP";
    case FAULT_ESTOP_GLITCH: return "E-STOP GLITCH";
    case FAULT_DRIVER_ALARM: return "DRIVER ALM";
    case FAULT_MOTOR_INIT_FAILED: return "MOTOR INIT";
    case FAULT_DISPLAY_INIT_FAILED: return "DISPLAY INIT";
    case FAULT_LVGL_INIT_FAILED: return "LVGL INIT";
    case FAULT_STORAGE_CORRUPT: return "STORAGE";
    case FAULT_WATCHDOG_RESET: return "WATCHDOG";
    default: return "UNKNOWN";
  }
}

const char* safety_fault_reason_message(FaultReason reason) {
  switch (reason) {
    case FAULT_NONE: return "No latched fault";
    case FAULT_ESTOP_PRESSED: return "Physical E-STOP input is active";
    case FAULT_ESTOP_GLITCH: return "E-STOP input changed during debounce";
    case FAULT_DRIVER_ALARM: return "DM542T driver alarm input is active";
    case FAULT_MOTOR_INIT_FAILED: return "Motor driver init failed";
    case FAULT_DISPLAY_INIT_FAILED: return "Display init failed";
    case FAULT_LVGL_INIT_FAILED: return "LVGL init failed";
    case FAULT_STORAGE_CORRUPT: return "Storage data was invalid";
    case FAULT_WATCHDOG_RESET: return "Watchdog reset was detected";
    default: return "Unknown fault";
  }
}

void safety_reset_estop() {
  estopResetPending.store(true, std::memory_order_release);
}

static void safety_handle_reset() {
  if (safety_can_reset_from_overlay()) {
    estopLocked.store(false, std::memory_order_release);
    g_estopPending.store(false, std::memory_order_release);
    safety_set_fault_reason(FAULT_NONE);
    LOG_I("ESTOP reset");
  } else {
    LOG_W("ESTOP reset failed - input still unsafe");
  }
}

bool safety_check_ui_reset() {
  if (g_uiResetPending.load(std::memory_order_acquire) && safety_can_reset_from_overlay()) {
    g_uiResetPending.store(false, std::memory_order_release);
    estopLocked.store(false, std::memory_order_release);
    g_estopPending.store(false, std::memory_order_release);
    safety_set_fault_reason(FAULT_NONE);
    return true;
  }
  g_uiResetPending.store(false, std::memory_order_release);
  return false;
}

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY TASK — Layer 2: State transition within 5 ms
// ───────────────────────────────────────────────────────────────────────────────
static void safety_poll_driver_alarm(void) {
  const bool low = (digitalRead(PIN_DRIVER_ALM) == LOW);
  if (low) {
    s_almHighMs = 0;
    if (s_almLowMs < 5000) {
      s_almLowMs++;
    }
    if (s_almLowMs >= 5 && !s_driverAlarmLatched.load(std::memory_order_acquire)) {
      s_driverAlarmLatched.store(true, std::memory_order_release);
      safety_set_fault_reason(FAULT_DRIVER_ALARM);
      digitalWrite(PIN_ENA, HIGH);
      g_wakePending.store(true, std::memory_order_release);
      if (estopStepper != nullptr) {
        if (g_stepperMutex && xSemaphoreTake(g_stepperMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          estopStepper->forceStop();
          xSemaphoreGive(g_stepperMutex);
        } else {
          estopStepper->forceStop();
        }
      }
      if (control_get_state() != STATE_ESTOP) {
        control_transition_to(STATE_ESTOP);
        estopLocked.store(true, std::memory_order_release);
      }
      LOG_E("Driver ALM fault (GPIO %u)", (unsigned)PIN_DRIVER_ALM);
    }
  } else {
    s_almLowMs = 0;
    if (s_almHighMs < 5000) {
      s_almHighMs++;
    }
    if (s_almHighMs >= 50) {
      s_driverAlarmLatched.store(false, std::memory_order_release);
    }
  }
}

void safetyTask(void* pvParameters) {
  LOG_I("Safety task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);  // Register with watchdog (BUG-03 fix)

  // Attach ESTOP interrupt here (after all other init is complete)
  safety_attach_estop();

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    // Feed watchdog every cycle
    safety_feed_watchdog();

    safety_poll_driver_alarm();

    if (estopResetPending.exchange(false, std::memory_order_acq_rel)) {
      safety_handle_reset();
    }

    if (g_estopPending.load(std::memory_order_acquire)) {
      uint32_t trigMs = g_estopTriggerMs.load(std::memory_order_acquire);
      if (trigMs == 0) {
        trigMs = millis();
        g_estopTriggerMs.store(trigMs, std::memory_order_release);
        if (estopStepper != nullptr) {
          if (g_stepperMutex && xSemaphoreTake(g_stepperMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            estopStepper->forceStop();
            xSemaphoreGive(g_stepperMutex);
          } else {
            estopStepper->forceStop();
          }
        }
      }

      uint32_t elapsedMs = millis() - trigMs;

      if (elapsedMs >= 5) {
        if (digitalRead(PIN_ESTOP) == LOW) {
          safety_set_fault_reason(FAULT_ESTOP_PRESSED);
          if (control_get_state() != STATE_ESTOP) {
            control_transition_to(STATE_ESTOP);
            estopLocked.store(true, std::memory_order_release);

            #if DEBUG_BUILD
            g_estopConfirmed++;
            LOG_W("ESTOP #%u confirmed", g_estopConfirmed);
            #else
            LOG_E("ESTOP TRIGGERED — State->ESTOP");
            #endif
          }
        } else {
          LOG_W("ESTOP edge released during debounce - locking out");
          safety_set_fault_reason(FAULT_ESTOP_GLITCH);
          if (control_get_state() != STATE_ESTOP) {
            control_transition_to(STATE_ESTOP);
          }
          estopLocked.store(true, std::memory_order_release);
        }
        g_estopPending.store(false, std::memory_order_release);
        g_estopTriggerMs.store(0, std::memory_order_release);
      }
    }

    vTaskDelayUntil(&t, pdMS_TO_TICKS(1));  // 1ms cycle
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// HARDWARE WATCHDOG — 5 second timeout
// ───────────────────────────────────────────────────────────────────────────────
void safety_init_watchdog() {
  // ESP32-P4 watchdog reconfiguration (ESP-IDF 5.x API)
  // Arduino framework already inits the watchdog, so we reconfigure it
  esp_task_wdt_config_t wdt_cfg = {
    .timeout_ms       = 5000,
    .idle_core_mask   = 0,        // Don't watch idle tasks
    .trigger_panic    = true,     // Panic on timeout
  };
  esp_task_wdt_reconfigure(&wdt_cfg);
  LOG_I("Watchdog reconfigured: 5000ms timeout, panic on timeout");
}

void safety_feed_watchdog() {
  esp_task_wdt_reset();
}
