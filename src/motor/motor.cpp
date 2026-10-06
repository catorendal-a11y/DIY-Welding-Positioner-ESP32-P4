// TIG Rotator Controller - Motor Control Implementation
// Stepper driver (standard PUL/DIR or DM542T timing) + FastAccelStepper library

#include "motor.h"
#include "../storage/storage.h"
#include "speed.h"
#include "microstep.h"
#include "../config.h"
#include "../app_state.h"  // fatal_halt
#include "../safety/safety.h"
#include "../control/control.h"
#include "../control/motion_policy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <FastAccelStepper.h>
#include <atomic>
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER ENGINE AND STEPPER INSTANCE
// ───────────────────────────────────────────────────────────────────────────────
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;
static SnapshotMailbox<MotorDriverInfo> driverInfoMailbox;
bool motor_read_driver_info(MotorDriverInfo& out) { return driverInfoMailbox.read(out); }
static std::atomic<bool> commandedCw{true};
bool motor_direction_is_cw() { return commandedCw.load(); }
void motor_record_direction(bool cw) { commandedCw.store(cw); }

// ───────────────────────────────────────────────────────────────────────────────
// STEPPER MUTEX — all stepper API calls must hold this
// FreeRTOS mutex keeps tick interrupts enabled (prevents IWDT on cross-core contention)
// ───────────────────────────────────────────────────────────────────────────────
SemaphoreHandle_t g_stepperMutex = nullptr;
static std::atomic<bool> cleanupPending{false};
static std::atomic<uint32_t> configuredAcceleration{7500u};
bool motor_cleanup_pending() { return cleanupPending.load(); }
void motor_command_failed() {
  cleanupPending.store(true);
  safety_report_motor_fault(FAULT_MOTOR_COMMAND);
}
#if defined(ARDUINO_ARCH_ESP32)
static TaskHandle_t motorOwner = nullptr;
void motor_bind_owner() { motorOwner = xTaskGetCurrentTaskHandle(); }
static bool motor_owner_ok() { return !motorOwner || motorOwner == xTaskGetCurrentTaskHandle(); }
#else
void motor_bind_owner() {}
static bool motor_owner_ok() { return true; }
#endif
bool motor_lock() {
  if (!motor_owner_ok()) { motor_command_failed(); return false; }
  if (g_stepperMutex && xSemaphoreTake(g_stepperMutex, pdMS_TO_TICKS(2)) == pdTRUE) return true;
  cleanupPending.store(true);
  safety_report_motor_fault(FAULT_MOTOR_TIMEOUT);
  return false;
}


// Published by motor_refresh_hz_cache() (motorTask). UI reads Hz/RPM without taking g_stepperMutex.
static std::atomic<uint32_t> g_stepperMilliHzAbsCached{0};

// DM542T datasheet: DIR setup >=5us before STEP; min pulse >=2.5us; input to 200kHz.
// FastAccelStepper on ESP32 uses MIN_DIR_DELAY_US=200 when a non-zero dir delay is set.
// Standard PUL/DIR: no extra DIR holdoff (delay 0).
static uint16_t motor_dir_delay_us_from_driver(uint8_t stepper_driver) {
  return (stepper_driver == STEPPER_DRIVER_DM542T) ? 200u : 0u;
}

// Leadshine-class enable timing (t1): ENA must be asserted >= 200 ms before the
// first PUL of a start. Applies once per enable (ENA is held LOW across mode
// restarts and pulse cycles); re-armed by motor_disable()/motor_halt().
static uint16_t motor_ena_settle_ms_from_driver(uint8_t stepper_driver) {
  return (stepper_driver == STEPPER_DRIVER_DM542T) ? 200u : 0u;
}
static uint16_t s_enaSettleMs = 0;
static uint32_t s_enaAssertedMs = 0;
// Boot counts as unsettled: the very first enable must also wait out t1.
// s_enaAsserted distinguishes "re-armed, never asserted" from "asserted at
// s_enaAssertedMs" so the elapsed check only runs on a real assertion.
static bool s_enaAsserted = false;
static bool s_enaSettled = false;

// Re-applying setDirectionPin while stepping corrupts timing / can stall the motor.
static uint16_t s_applied_dir_delay_us = 0;
static bool s_dir_timing_applied = false;
static bool s_temp_accel_applied = false;

static uint32_t motor_clamp_acceleration(uint32_t accel) {
  if (accel < 1000u) return 1000u;
  if (accel > 30000u) return 30000u;
  return accel;
}

static bool motor_apply_stepper_dir_timing(uint16_t want) {
  if (stepper == nullptr) return false;
  if (s_dir_timing_applied && want == s_applied_dir_delay_us) return true;

  if (s_dir_timing_applied && want != s_applied_dir_delay_us && stepper->isRunning()) {
    digitalWrite(PIN_ENA, HIGH);
    // forceStop() drains the RMT queue asynchronously. Do not reconfigure DIR
    // while that queue can still emit pulses; owner-task fault cleanup retries.
    motor_command_failed();
    return false;
  }
  stepper->setDirectionPin(PIN_DIR, true, want);
  MotorDriverInfo info;
  info.name = stepper->driverTypeString();
  info.direction_before_us = uint32_t(stepper->getDirChangeBeforeTicks()) * stepper->getDirChangeBeforePauseCount() / (TICKS_PER_S / 1000000);
  info.direction_after_us = stepper->getDirChangeAfterTicks() / (TICKS_PER_S / 1000000);
  driverInfoMailbox.publish(info);
  s_applied_dir_delay_us = want;
  s_dir_timing_applied = true;
  return true;
}

// ───────────────────────────────────────────────────────────────────────────────
// GPIO INITIALIZATION — ENA MUST BE HIGH BEFORE CALLING
// ───────────────────────────────────────────────────────────────────────────────
void motor_gpio_init() {
  // ENA pin — output, start HIGH (motor disabled)
  // GPIO contract: LOW requests enable, HIGH inhibits. The actual driver
  // interface must preserve this polarity; see docs/HARDWARE_SETUP.md.
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);  // Motor disabled on boot

  // Direction pin
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);  // Default CCW

  // Step pin
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);

  // ESTOP input (debounce + actions in safety_task; ISR in safety module)
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_DIR_SWITCH, INPUT_PULLUP);
  pinMode(PIN_DRIVER_ALM, INPUT_PULLUP);

  LOG_I("Motor GPIO init OK");
  LOG_I("  ENA=%d (must be HIGH)", digitalRead(PIN_ENA));
  LOG_I("  DIR=%d STEP=%d", digitalRead(PIN_DIR), digitalRead(PIN_STEP));
  LOG_I("  ESTOP=%d DIR_SW=%d ALM=%d", digitalRead(PIN_ESTOP), digitalRead(PIN_DIR_SWITCH),
        digitalRead(PIN_DRIVER_ALM));
}

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
static void motor_init_on_control_core() {
  g_stepperMutex = xSemaphoreCreateMutex();
  if (!g_stepperMutex) fatal_halt("motor: stepper mutex alloc");

  // init(0) pins the library task only. IDF allocates the RMT interrupt on
  // the caller's core, so channel allocation must also execute on Core 0.
#if defined(ARDUINO_ARCH_ESP32)
  configASSERT(xPortGetCoreID() == 0);
  LOG_I("FastAccelStepper channel allocation: Core %d", xPortGetCoreID());
#endif
  engine.init(0);

  // Connect stepper to step pin with RMT driver
  // 1.4.0 can fall back to I2S when RMT is exhausted. This machine requires RMT.
  stepper = engine.stepperConnectToPin(PIN_STEP, FasDriver::RMT);
  if (stepper == nullptr) fatal_halt("motor: FastAccelStepper init");

  int accelSteps = 7500;
  uint8_t driverKind = STEPPER_DRIVER_STANDARD;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  accelSteps = g_settings.acceleration;
  driverKind = g_settings.stepper_driver;
  xSemaphoreGive(g_settings_mutex);

  // Same lock order as motor_apply_settings(): g_settings first, then stepper mutex.
  if (!motor_lock()) return;
  // Configure stepper (DIR delay depends on stepper_driver snapshot)
  motor_apply_stepper_dir_timing(motor_dir_delay_us_from_driver(driverKind));
  s_enaSettleMs = motor_ena_settle_ms_from_driver(driverKind);
  // NOTE: Do NOT call setEnablePin() — ENA is controlled manually via digitalWrite()
  // Keep the LOW=enable / HIGH=inhibit GPIO contract independent of the
  // chosen optocoupler/interface topology. Verify it at the actual driver.
  // FastAccelStepper's setEnablePin conflicts with manual control.

  // Set acceleration and start speed
  if (stepper->setAcceleration(accelSteps) != 0) motor_command_failed();
  configuredAcceleration.store(static_cast<uint32_t>(accelSteps));
  stepper->setLinearAcceleration(200);
  if (stepper->setSpeedInHz(START_SPEED) != 0) motor_command_failed();
  xSemaphoreGive(g_stepperMutex);

  LOG_I("FastAccelStepper init OK");
  LOG_I("  Steps/rev: %u", static_cast<unsigned>(microstep_get_steps_per_rev()));
  LOG_I("  Accel: %d steps/s2", accelSteps);
  LOG_I("  Start speed: %d Hz", START_SPEED);
}

void motor_init() {
  motor_gpio_init(); // Keep ENA inhibited before allocation or any boot failure.
#if defined(ARDUINO_ARCH_ESP32)
  if (xPortGetCoreID() != 0) {
    const TaskHandle_t caller = xTaskGetCurrentTaskHandle();
    const auto initialize = [](void* context) {
      motor_init_on_control_core();
      xTaskNotifyGive(static_cast<TaskHandle_t>(context));
      vTaskDelete(nullptr);
    };
    if (xTaskCreatePinnedToCore(initialize,"motor-init",8192,caller,5,nullptr,0) != pdPASS)
      fatal_halt("motor: initialization task allocation");
    if (ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(5000)) == 0)
      fatal_halt("motor: initialization timeout");
    return;
  }
#endif
  motor_init_on_control_core();
}

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
bool motor_run_cw() {
  if (safety_inhibit_motion() || control_motion_blocked()) return false;
  if (!motor_lock()) return false;
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  // Re-check after mutex: ISR may have asserted ESTOP between outer check and here;
  // never pull ENA LOW if ESTOP is active (would override hardware disable path).
  if (safety_inhibit_motion() || control_motion_blocked()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  digitalWrite(PIN_ENA, LOW);
  if (safety_inhibit_motion() || control_motion_blocked()) {
    digitalWrite(PIN_ENA, HIGH);
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  motor_record_direction(true);
  if (stepper->runForward() != MoveResultCode::OK) {
    motor_command_failed();
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: CW");
  return true;
}

bool motor_run_ccw() {
  if (safety_inhibit_motion() || control_motion_blocked()) return false;
  if (!motor_lock()) return false;
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  if (safety_inhibit_motion() || control_motion_blocked()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  digitalWrite(PIN_ENA, LOW);
  if (safety_inhibit_motion() || control_motion_blocked()) {
    digitalWrite(PIN_ENA, HIGH);
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  motor_record_direction(false);
  if (stepper->runBackward() != MoveResultCode::OK) {
    motor_command_failed();
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: CCW");
  return true;
}

void motor_stop() {
  if (!motor_lock()) return;
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return;
  }
  stepper->stopMove();
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: stopping (smooth decel)");
}

bool motor_halt() {
  digitalWrite(PIN_ENA, HIGH); // Inhibit before waiting on library cleanup.
  // Inhibiting re-arms the settle window for the next enable.
  s_enaAsserted = false;
  s_enaSettled = (s_enaSettleMs == 0u);
  if (!motor_lock()) return false;
  if (stepper) stepper->forceStop();
  // forceStop() stops adding commands, not necessarily the pulses already
  // queued. Keep reset inhibited until isRunning() confirms the queue drained.
  const bool stopped = !stepper || !stepper->isRunning();
  cleanupPending.store(!stopped);
  xSemaphoreGive(g_stepperMutex);
  return stopped;
}

void motor_disable() {
  digitalWrite(PIN_ENA, HIGH);
  // A fresh enable must wait out the settle window again.
  s_enaAsserted = false;
  s_enaSettled = (s_enaSettleMs == 0u);
}

bool motor_prepare_start() {
  if (safety_inhibit_motion() || control_motion_blocked()) return false;
  // Only a fully validated start reaches this point (controlTask admission);
  // assert the enable and stamp the clock so the settle window is measured
  // from this assertion, never from boot or a previous enable.
  digitalWrite(PIN_ENA, LOW);
  if (safety_inhibit_motion() || control_motion_blocked()) {
    digitalWrite(PIN_ENA, HIGH);
    return false;
  }
  s_enaAssertedMs = millis();
  s_enaAsserted = true;
  s_enaSettled = (s_enaSettleMs == 0u);
  return true;
}

bool motor_ena_settle_pending() {
  if (s_enaSettleMs == 0u || s_enaSettled) return false;
  if (!s_enaAsserted) return true;  // Re-armed; the next prepare stamps it.
  if (millis() - s_enaAssertedMs >= s_enaSettleMs) {
    s_enaSettled = true;
    return false;
  }
  return true;
}

// ───────────────────────────────────────────────────────────────────────────────
// STATUS QUERIES
// ───────────────────────────────────────────────────────────────────────────────
bool motor_is_running() {
  if (!motor_lock()) return true; // Unknown must never be treated as stopped.
  bool running = (stepper != nullptr) && stepper->isRunning();
  xSemaphoreGive(g_stepperMutex);
  return running;
}

float motor_get_step_frequency_hz() {
  return (float)g_stepperMilliHzAbsCached.load(std::memory_order_acquire) / 1000.0f;
}

void motor_refresh_hz_cache(void) {
  uint32_t absMilli = 0;
  if (motor_owner_ok() && g_stepperMutex != nullptr && xSemaphoreTake(g_stepperMutex, 0) == pdTRUE) {
    if (stepper != nullptr) {
      int32_t mhZ = stepper->getCurrentSpeedInMilliHz();
      int64_t a = (int64_t)mhZ;
      if (a < 0) {
        a = -a;
      }
      if (a > (int64_t)UINT32_MAX) {
        a = (int64_t)UINT32_MAX;
      }
      absMilli = (uint32_t)a;
    }
    xSemaphoreGive(g_stepperMutex);
    g_stepperMilliHzAbsCached.store(absMilli, std::memory_order_release);
  }
}

uint32_t motor_get_current_hz() {
  float hz = motor_get_step_frequency_hz();
  if (hz < 0.001f) return 0u;
  if (hz >= (float)UINT32_MAX) return UINT32_MAX;
  return (uint32_t)(hz + 0.5f);
}

uint32_t motor_milli_hz_for_rpm_calibrated(float rpm_workpiece_command) {
  if (!std::isfinite(rpm_workpiece_command) || rpm_workpiece_command < MIN_RPM || rpm_workpiece_command > speed_get_rpm_max()) return 0;
  const double milli_hz = double(rpmToStepHzCalibrated(rpm_workpiece_command)) * 1000.0;
  if (!std::isfinite(milli_hz) || milli_hz < START_SPEED * 1000.0 || milli_hz > UINT32_MAX) return 0;
  return static_cast<uint32_t>(milli_hz);
}

bool motor_apply_speed_for_rpm_locked(float rpm_workpiece_command) {
  if (stepper == nullptr) return false;
  uint32_t mhz = motor_milli_hz_for_rpm_calibrated(rpm_workpiece_command);
  if (!mhz) return false;
  if (stepper->setSpeedInMilliHz(mhz) != 0) { motor_command_failed(); return false; }
  stepper->applySpeedAcceleration();
  return true;
}

void motor_set_target_milli_hz(uint32_t mhz) {
  if (g_stepperMutex == nullptr) return;
  if (!motor_lock()) return;
  if (stepper != nullptr) {
    if (stepper->setSpeedInMilliHz(mhz) != 0) motor_command_failed();
    else stepper->applySpeedAcceleration();
  }
  xSemaphoreGive(g_stepperMutex);
}

void motor_apply_settings() {
  int accelSteps = 7500;
  uint8_t driverKind = STEPPER_DRIVER_STANDARD;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  accelSteps = g_settings.acceleration;
  driverKind = g_settings.stepper_driver;
  xSemaphoreGive(g_settings_mutex);
  const uint16_t dirDelayUs = motor_dir_delay_us_from_driver(driverKind);
  s_enaSettleMs = motor_ena_settle_ms_from_driver(driverKind);

  if (!motor_lock()) return;
  if (stepper != nullptr) {
    if (!motor_apply_stepper_dir_timing(dirDelayUs)) { xSemaphoreGive(g_stepperMutex); return; }
    if (stepper->setAcceleration(accelSteps) != 0) motor_command_failed();
    configuredAcceleration.store(static_cast<uint32_t>(accelSteps));
    stepper->setLinearAcceleration(200);
  }
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: accel=%d driver=%s (DIR delay %u us)", accelSteps,
        driverKind == STEPPER_DRIVER_DM542T ? "DM542T" : "Standard", (unsigned)dirDelayUs);
}

void motor_apply_soft_start_acceleration() {
  uint32_t configured = 7500u;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  configured = (uint32_t)g_settings.acceleration;
  xSemaphoreGive(g_settings_mutex);

  uint32_t softAccel = motor_clamp_acceleration(configured / 4u);
  if (softAccel >= configured) {
    softAccel = motor_clamp_acceleration(configured);
  }

  if (g_stepperMutex == nullptr) return;
  if (!motor_lock()) return;
  if (stepper != nullptr) {
    if (stepper->setAcceleration(softAccel) != 0) motor_command_failed();
    configuredAcceleration.store(softAccel);
    s_temp_accel_applied = true;
  }
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: soft start accel=%u (configured=%u)", (unsigned)softAccel, (unsigned)configured);
}

void motor_restore_configured_acceleration() {
  if (!s_temp_accel_applied) return;

  uint32_t configured = 7500u;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  configured = motor_clamp_acceleration((uint32_t)g_settings.acceleration);
  xSemaphoreGive(g_settings_mutex);

  if (g_stepperMutex == nullptr) {
    s_temp_accel_applied = false;
    return;
  }
  if (!motor_lock()) return;
  if (stepper != nullptr) {
    if (stepper->setAcceleration(configured) != 0) motor_command_failed();
    configuredAcceleration.store(configured);
  }
  s_temp_accel_applied = false;
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: restored accel=%u", (unsigned)configured);
}

bool motor_read_position(int32_t* position) {
  if (!position || !motor_lock()) return false;
  const bool ok = stepper != nullptr;
  if (ok) *position = stepper->getCurrentPosition();
  xSemaphoreGive(g_stepperMutex); return ok;
}
bool motor_move_steps(long steps, float rpm, int32_t* start_position) {
  if (!start_position || steps == 0 || steps < -INT32_MAX || steps > INT32_MAX ||
      !motor_milli_hz_for_rpm_calibrated(rpm) || !motor_lock()) return false;
  bool ok = stepper && !safety_inhibit_motion() && !control_motion_blocked();
  if (ok) { *start_position = stepper->getCurrentPosition(); ok = motor_apply_speed_for_rpm_locked(rpm); }
  if (ok) {
    digitalWrite(PIN_ENA, LOW);
    if (safety_inhibit_motion() || control_motion_blocked()) ok = false;
    else { motor_record_direction(steps >= 0); ok = stepper->move(steps) == MoveResultCode::OK; if (!ok) motor_command_failed(); }
  }
  if (!ok) digitalWrite(PIN_ENA, HIGH);
  xSemaphoreGive(g_stepperMutex); return ok;
}

bool motor_move_timeout_ms(uint32_t pulses, float rpm, uint32_t& budget) {
  const uint32_t rate = motor_milli_hz_for_rpm_calibrated(rpm);
  return motion_move_timeout_ms(pulses, rate, motion_stop_timeout_ms(rate, configuredAcceleration.load()), budget);
}
uint32_t motor_stop_timeout_ms() {
  return motion_stop_timeout_ms(g_stepperMilliHzAbsCached.load(), configuredAcceleration.load());
}
