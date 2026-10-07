// TIG Rotator Controller - Safety System Implementation
// ESTOP direct GPIO action and hardware watchdog; latency requires bench measurement.

#include "safety.h"
#include "../config.h"
#include "../event_log.h"
#include "../motor/motor.h"
#include "../motor/speed.h"
#include "../control/control.h"
#include "../mirror/usb_mirror.h"
#include "../ui/display.h"
#include <esp_task_wdt.h>
#include "freertos/task.h"

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS
// Cross-core atomics live in src/app_state.cpp (see app_state.h).
// ───────────────────────────────────────────────────────────────────────────────
static std::atomic<bool> estopLocked{false};
static std::atomic<uint32_t> readyTasks{0};
void safety_task_ready(uint32_t bit) { readyTasks.fetch_or(bit); }

static std::atomic<bool> estopResetPending{false};
static std::atomic<uint8_t> s_faultReason{FAULT_NONE};
// Any latched fault also disarms the USB mirror: a previously authorized PC
// must not be able to drive the UI (including RESET TO IDLE) after a fault.
// Fault policy: the FIRST latched reason is retained until RESET — later
// faults keep ENA inhibited but do not overwrite the diagnostic cause.
static void safety_latch_fault(FaultReason reason) {
  digitalWrite(PIN_ENA, HIGH);
  usb_mirror_set_armed(false);
  estopLocked.store(true);
  uint8_t expected = FAULT_NONE;
  s_faultReason.compare_exchange_strong(expected, static_cast<uint8_t>(reason),
                                        std::memory_order_acq_rel, std::memory_order_acquire);
  g_wakePending.store(true);
}
void safety_report_motor_fault(FaultReason reason) {
  safety_latch_fault(reason);
  control_transition_to(STATE_ESTOP);
}
void safety_report_input_fault() {
  safety_latch_fault(FAULT_PEDAL_INPUT);
  control_transition_to(STATE_ESTOP);
}

// DM542T ALM: open-drain active LOW on fault; INPUT_PULLUP on PIN_DRIVER_ALM.
static std::atomic<bool> s_driverAlarmLatched{false};
static uint16_t s_almHighMs = 0;

static void safety_set_fault_reason(FaultReason reason) {
  s_faultReason.store((uint8_t)reason, std::memory_order_release);
}


#if DEBUG_BUILD
static uint32_t g_estopConfirmed = 0;
#endif

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY INITIALIZATION — Cache stepper pointer for ISR
// ───────────────────────────────────────────────────────────────────────────────
void safety_cache_stepper() {} // Compatibility: no library pointer crosses task boundaries.

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP INTERRUPT SERVICE ROUTINE
// Layer 1: direct hardware action only; no measured latency is asserted.
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
  // Input conditioning must match the active-LOW interface in HARDWARE_SETUP.
  // Sample at startup; this is not a wiring continuity test.
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  delay(2);
  uint8_t lowCount = 0;
  for (int i = 0; i < 3; i++) {
    if (digitalRead(PIN_ESTOP) == LOW) lowCount++;
    delayMicroseconds(500);
  }
  bool estopPressed = (lowCount >= 2);
  LOG_I("Safety init: ESTOP=%s (low samples %u/3)", estopPressed ? "PRESSED" : "OK", (unsigned)lowCount);

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
bool safety_is_estop_active() { return (digitalRead(PIN_ESTOP) == LOW); }

bool safety_is_driver_alarm_latched() { return s_driverAlarmLatched.load(std::memory_order_acquire); }

bool safety_inhibit_motion() {
  return readyTasks.load() != 15u || estopLocked.load() || g_estopPending.load() ||
         !speed_pedal_input_healthy() || safety_is_estop_active() ||
         digitalRead(PIN_DRIVER_ALM) == LOW ||
         s_driverAlarmLatched.load(std::memory_order_acquire) || g_restartRequired.load() ||
#if HMI_REQUIRED_FOR_MOTION
         !display_touch_operational() ||
#endif
         !input_task_heartbeat_fresh(millis());
}

bool safety_can_reset_from_overlay() {
  return !motor_cleanup_pending() && readyTasks.load() == 15u && speed_pedal_input_healthy() &&
         !g_estopPending.load(std::memory_order_acquire) && !g_restartRequired.load() &&
         digitalRead(PIN_ESTOP) == HIGH && digitalRead(PIN_DRIVER_ALM) == HIGH &&
#if HMI_REQUIRED_FOR_MOTION
         display_touch_operational() &&
#endif
         !s_driverAlarmLatched.load(std::memory_order_acquire) &&
         !control_heartbeat_stale(millis()) && input_task_heartbeat_fresh(millis());
}

bool safety_is_estop_locked() { return estopLocked.load(std::memory_order_acquire); }

FaultReason safety_get_fault_reason() {
  uint8_t reason = s_faultReason.load(std::memory_order_acquire);
  if (reason > FAULT_INPUT_STALE) {
    return FAULT_NONE;
  }
  return (FaultReason)reason;
}

void safety_reset_estop() { estopResetPending.store(true, std::memory_order_release); }

static void safety_handle_reset() {
  if (safety_can_reset_from_overlay()) {
    estopLocked.store(false, std::memory_order_release);
    safety_set_fault_reason(FAULT_NONE);
    event_log_add("FAULT RESET");
    LOG_I("ESTOP reset");
  } else {
    LOG_W("ESTOP reset failed - input still unsafe");
  }
}

bool safety_check_ui_reset() {
  if (g_uiResetPending.load(std::memory_order_acquire) && safety_can_reset_from_overlay()) {
    g_uiResetPending.store(false, std::memory_order_release);
    estopLocked.store(false, std::memory_order_release);
    safety_set_fault_reason(FAULT_NONE);
    event_log_add("FAULT RESET");
    return true;
  }
  g_uiResetPending.store(false, std::memory_order_release);
  return false;
}

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY TASK — Layer 2: scheduled state transition (latency requires bench measurement)
// ───────────────────────────────────────────────────────────────────────────────
static void safety_poll_driver_alarm(void) {
  const bool low = (digitalRead(PIN_DRIVER_ALM) == LOW);
  if (low) {
    // The first LOW inhibits ENA, so it must also cancel the motion planner.
    // Otherwise a short alarm leaves the UI running and silently loses steps.
    digitalWrite(PIN_ENA, HIGH);
    s_almHighMs = 0;
    if (!s_driverAlarmLatched.load(std::memory_order_acquire)) {
      s_driverAlarmLatched.store(true, std::memory_order_release);
      safety_latch_fault(FAULT_DRIVER_ALARM);
      // controlTask owns forceStop cleanup.
      if (control_get_state() != STATE_ESTOP) {
        control_transition_to(STATE_ESTOP);
      }
      LOG_E("Driver ALM fault (GPIO %u)", (unsigned)PIN_DRIVER_ALM);
    }
  } else {
    if (s_almHighMs < 5000) {
      s_almHighMs++;
    }
    if (s_almHighMs >= 50) {
      s_driverAlarmLatched.store(false, std::memory_order_release);
    }
  }
}

void safety_run_cycle() {
  // Heartbeat supervisors are edge-triggered: latch (and log) on the
  // healthy->stale transition only, never every millisecond while stale.
  static bool controlWasStale = false;
  static bool inputWasStale = false;

  // Dead-man supervisor, edge-triggered: latch (and log) only on the
  // healthy->stale transition, never every millisecond while stale. The
  // input channel arms only after inputTask stamped its first heartbeat.
  const bool controlStale = control_heartbeat_stale(millis());
  if (controlStale && !controlWasStale) {
    safety_latch_fault(FAULT_CONTROL_STALE);
    if (control_get_state() != STATE_ESTOP) control_transition_to(STATE_ESTOP);
    LOG_E("Control heartbeat stale - motion inhibited");
  }
  controlWasStale = controlStale;

  const bool inputStarted = (readyTasks.load(std::memory_order_acquire) & 2u) != 0u &&
                            g_inputHeartbeatValid.load(std::memory_order_acquire);
  const bool inputStale = inputStarted && !input_task_heartbeat_fresh(millis());
  if (inputStale && !inputWasStale) {
    safety_latch_fault(FAULT_INPUT_STALE);
    if (control_get_state() != STATE_ESTOP) control_transition_to(STATE_ESTOP);
    LOG_E("Input task heartbeat stale - motion inhibited");
  }
  inputWasStale = inputStale;

  safety_poll_driver_alarm();
  control_check_stop_deadline(millis());

  if (estopResetPending.exchange(false, std::memory_order_acq_rel)) {
    safety_handle_reset();
  }

  // Redundant level channel: a sustained LOW must latch even when the
  // FALLING edge never reached the ISR (flash-write window, glitch below
  // detector thresholds). Feeds the same 5 ms confirm path as the ISR.
  if (!g_estopPending.load(std::memory_order_acquire) &&
      !estopLocked.load(std::memory_order_acquire) && digitalRead(PIN_ESTOP) == LOW) {
    digitalWrite(PIN_ENA, HIGH); // Redundant channel inhibits before debounce too.
    g_estopTriggerMs.store(millis(), std::memory_order_release);
    g_estopPending.store(true, std::memory_order_release);
    g_wakePending.store(true, std::memory_order_release);
  }

  if (g_estopPending.load(std::memory_order_acquire)) {
    uint32_t trigMs = g_estopTriggerMs.load(std::memory_order_acquire);
    if (trigMs == 0) {
      trigMs = millis();
      g_estopTriggerMs.store(trigMs, std::memory_order_release);
      // controlTask owns forceStop cleanup.
    }

    uint32_t elapsedMs = millis() - trigMs;

    if (elapsedMs >= 5) {
      if (digitalRead(PIN_ESTOP) == LOW) {
        safety_latch_fault(FAULT_ESTOP_PRESSED);
        if (control_get_state() != STATE_ESTOP) {
          control_transition_to(STATE_ESTOP);

#if DEBUG_BUILD
          g_estopConfirmed++;
          LOG_W("ESTOP #%u confirmed", static_cast<unsigned>(g_estopConfirmed));
#else
          LOG_E("ESTOP TRIGGERED — State->ESTOP");
#endif
        }
      } else {
        LOG_W("ESTOP edge released during debounce - locking out");
        safety_latch_fault(FAULT_ESTOP_GLITCH);
        if (control_get_state() != STATE_ESTOP) {
          control_transition_to(STATE_ESTOP);
        }
      }
      g_estopPending.store(false, std::memory_order_release);
      g_estopTriggerMs.store(0, std::memory_order_release);
    }
  }

}

void safetyTask(void* pvParameters) {
  LOG_I("Safety task started on Core %d", xPortGetCoreID());
  safety_register_watchdog();
  // Attach ESTOP interrupt here (after all other init is complete).
  safety_attach_estop();
  safety_task_ready(1u);
  TickType_t t = xTaskGetTickCount();
  for (;;) {
    safety_feed_watchdog();
    safety_run_cycle();
    vTaskDelayUntil(&t, pdMS_TO_TICKS(1));
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// HARDWARE WATCHDOG — 5 second timeout
// ───────────────────────────────────────────────────────────────────────────────
void safety_init_watchdog() {
  // ESP32-P4 watchdog reconfiguration (ESP-IDF 5.x API)
  // Arduino framework already inits the watchdog, so we reconfigure it
  esp_task_wdt_config_t wdt_cfg = {
      .timeout_ms = 5000,
      .idle_core_mask = 0,    // Don't watch idle tasks
      .trigger_panic = true,  // Panic on timeout
  };
  esp_err_t result = esp_task_wdt_reconfigure(&wdt_cfg);
  if (result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_init(&wdt_cfg);
  if (result != ESP_OK) fatal_halt("watchdog configuration failed");
  LOG_I("Watchdog reconfigured: 5000ms timeout, panic on timeout");
}

void safety_feed_watchdog() {
  if (esp_task_wdt_reset() != ESP_OK) fatal_halt("watchdog feed failed");
}

void safety_register_watchdog() {
  if (esp_task_wdt_add(nullptr) != ESP_OK) fatal_halt("watchdog registration failed");
}
