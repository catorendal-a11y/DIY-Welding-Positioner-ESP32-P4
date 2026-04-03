// TIG Rotator Controller - Safety System Implementation
// ESTOP ISR with < 1ms response, hardware watchdog

#include "safety.h"
#include "../config.h"
#include "../motor/motor.h"
#include "../control/control.h"
#include <esp_task_wdt.h>

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS
// ───────────────────────────────────────────────────────────────────────────────
volatile bool g_estopPending = false;
volatile uint32_t g_estopTriggerMs = 0;
static bool estopLocked = false;
static FastAccelStepper* estopStepper = nullptr;
static volatile bool estopResetPending = false;
volatile bool g_uiResetPending = false;

#if DEBUG_BUILD
static uint32_t g_estopISRCount  = 0;
static uint32_t g_estopConfirmed = 0;
#endif

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY INITIALIZATION — Cache stepper pointer for ISR
// ───────────────────────────────────────────────────────────────────────────────
void safety_cache_stepper() {
  estopStepper = motor_get_stepper();
}

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP INTERRUPT SERVICE ROUTINE
// Layer 1: < 0.5 ms response — Direct hardware action only
// ───────────────────────────────────────────────────────────────────────────────
void IRAM_ATTR estopISR() {
  #if DEBUG_BUILD
  g_estopISRCount++;
  #endif

  // CRITICAL: Minimum work, no FreeRTOS calls, no Serial prints
  // This runs at interrupt priority — must exit immediately

  // Layer 1: Disable motor NOW (direct register - IRAM-safe, no flash/PSRAM)
  // GPIO 52 is in high bank (32-63): use out1_w1ts to set HIGH
  GPIO.out1_w1ts.val = (1UL << (PIN_ENA - 32));

  // Stop stepper (forceStop is IRAM-safe)
  if (estopStepper != nullptr) {
    estopStepper->forceStop();
  }

  // Signal safety task for Layer 2 (state transition)
  g_estopPending = true;
  g_estopTriggerMs = millis();
}

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void safety_init() {
  // Configure ESTOP pin as input with pull-up
  pinMode(PIN_ESTOP, INPUT_PULLUP);

  // Verify ESTOP is not pressed at boot (NC contact = HIGH when released)
  bool estopPressed = (digitalRead(PIN_ESTOP) == LOW);
  LOG_I("Safety init: ESTOP=%s", estopPressed ? "PRESSED" : "OK");

  if (estopPressed) {
    LOG_W("ESTOP pressed at boot — system locked");
    estopLocked = true;
    g_estopPending = true;
  g_estopTriggerMs = millis();
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

bool safety_is_estop_locked() {
  return estopLocked;
}

void safety_reset_estop() {
  estopResetPending = true;
}

static void safety_handle_reset() {
  if (digitalRead(PIN_ESTOP) == HIGH) {
    estopLocked = false;
    g_estopPending = false;
    LOG_I("ESTOP reset");
  } else {
    LOG_W("ESTOP reset failed - button still pressed");
  }
}

bool safety_check_ui_reset() {
  if (g_uiResetPending && digitalRead(PIN_ESTOP) == HIGH) {
    g_uiResetPending = false;
    estopLocked = false;
    g_estopPending = false;
    return true;
  }
  g_uiResetPending = false;
  return false;
}

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY TASK — Layer 2: State transition within 5 ms
// ───────────────────────────────────────────────────────────────────────────────
void safetyTask(void* pvParameters) {
  LOG_I("Safety task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);  // Register with watchdog (BUG-03 fix)

  // Attach ESTOP interrupt here (after all other init is complete)
  safety_attach_estop();

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    // Feed watchdog every cycle
    safety_feed_watchdog();

    // Check for pending ESTOP from ISR
    if (estopResetPending) {
      estopResetPending = false;
      safety_handle_reset();
    }

    if (g_estopPending) {
      uint32_t elapsedMs = millis() - g_estopTriggerMs;

      // Debounce: Wait 5ms before state transition
      if (elapsedMs >= 5) {
        // Verify ESTOP is still pressed (not a glitch)
        if (digitalRead(PIN_ESTOP) == LOW) {
          // Layer 2: State transition to ESTOP
          if (control_get_state() != STATE_ESTOP) {
            control_transition_to(STATE_ESTOP);
            estopLocked = true;

            #if DEBUG_BUILD
            g_estopConfirmed++;
            LOG_W("ESTOP #%u confirmed (ISR hit %u times)", g_estopConfirmed, g_estopISRCount);
            g_estopISRCount = 0;
            #else
            LOG_E("ESTOP TRIGGERED — State->ESTOP");
            #endif
          }
        } else {
          // ESTOP released quickly (< 5ms) — treat as glitch
          LOG_W("ESTOP glitch detected (< 5ms)");
        }
        g_estopPending = false;
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
