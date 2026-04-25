// TIG Rotator Controller - Motor Control Implementation
// Stepper driver (standard PUL/DIR or DM542T timing) + FastAccelStepper library

#include "motor.h"
#include "../storage/storage.h"
#include "speed.h"
#include "microstep.h"
#include "../config.h"
#include "../app_state.h"   // fatal_halt
#include "../safety/safety.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <FastAccelStepper.h>
#include <atomic>
#include <cstdint>

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER ENGINE AND STEPPER INSTANCE
// ───────────────────────────────────────────────────────────────────────────────
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// STEPPER MUTEX — all stepper API calls must hold this
// FreeRTOS mutex keeps tick interrupts enabled (prevents IWDT on cross-core contention)
// ───────────────────────────────────────────────────────────────────────────────
SemaphoreHandle_t g_stepperMutex = nullptr;

// Published by motor_refresh_hz_cache() (motorTask). UI reads Hz/RPM without taking g_stepperMutex.
static std::atomic<uint32_t> g_stepperMilliHzAbsCached{0};

// DM542T datasheet: DIR setup >=5us before STEP; min pulse >=2.5us; input to 200kHz.
// FastAccelStepper on ESP32 uses MIN_DIR_DELAY_US=200 when a non-zero dir delay is set.
// Standard PUL/DIR: no extra DIR holdoff (delay 0).
static uint16_t motor_dir_delay_us_from_driver(uint8_t stepper_driver) {
  return (stepper_driver == STEPPER_DRIVER_DM542T) ? 200u : 0u;
}

// Re-applying setDirectionPin while stepping corrupts timing / can stall the motor.
static uint16_t s_applied_dir_delay_us = 0;
static bool s_dir_timing_applied = false;
static bool s_temp_accel_applied = false;

static uint32_t motor_clamp_acceleration(uint32_t accel) {
  if (accel < 1000u) return 1000u;
  if (accel > 30000u) return 30000u;
  return accel;
}

static void motor_apply_stepper_dir_timing(uint16_t want) {
  if (stepper == nullptr) return;
  if (s_dir_timing_applied && want == s_applied_dir_delay_us) return;

  if (s_dir_timing_applied && want != s_applied_dir_delay_us && stepper->isRunning()) {
    stepper->forceStop();
    digitalWrite(PIN_ENA, HIGH);
  }
  stepper->setDirectionPin(PIN_DIR, true, want);
  s_applied_dir_delay_us = want;
  s_dir_timing_applied = true;
}

// ───────────────────────────────────────────────────────────────────────────────
// GPIO INITIALIZATION — ENA MUST BE HIGH BEFORE CALLING
// ───────────────────────────────────────────────────────────────────────────────
void motor_gpio_init() {
  // ENA pin — output, start HIGH (motor disabled)
  // Wiring: ENA+ → 5V, ENA- → GPIO52. LOW = enable, HIGH = disable (active LOW)
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
void motor_init() {
  motor_gpio_init();

  g_stepperMutex = xSemaphoreCreateMutex();
  if (!g_stepperMutex) fatal_halt("motor: stepper mutex alloc");

  // Pin stepper engine to Core 0 — motorTask, controlTask and safetyTask all run here.
  // Without pinning, FastAccelStepper's internal timer ISR may fire on Core 1
  // causing inter-core contention and jitter in step pulse timing.
  engine.init(0);

  // Connect stepper to step pin with RMT driver
  stepper = engine.stepperConnectToPin(PIN_STEP);
  if (stepper == nullptr) fatal_halt("motor: FastAccelStepper init");

  int accelSteps = 7500;
  uint8_t driverKind = STEPPER_DRIVER_STANDARD;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  accelSteps = g_settings.acceleration;
  driverKind = g_settings.stepper_driver;
  xSemaphoreGive(g_settings_mutex);

  // Same lock order as motor_apply_settings(): g_settings first, then stepper mutex.
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  // Configure stepper (DIR delay depends on stepper_driver snapshot)
  motor_apply_stepper_dir_timing(motor_dir_delay_us_from_driver(driverKind));
  // NOTE: Do NOT call setEnablePin() — ENA is controlled manually via digitalWrite()
  // Wiring: ENA+→5V, ENA-→GPIO52. LOW=enable, HIGH=disable (active LOW)
  // FastAccelStepper's setEnablePin conflicts with manual control.

  // Set acceleration and start speed
  stepper->setAcceleration(accelSteps);
  stepper->setLinearAcceleration(200);
  stepper->setSpeedInHz(START_SPEED);
  xSemaphoreGive(g_stepperMutex);

  LOG_I("FastAccelStepper init OK");
  LOG_I("  Steps/rev: %u", microstep_get_steps_per_rev());
  LOG_I("  Accel: %d steps/s2", accelSteps);
  LOG_I("  Start speed: %d Hz", START_SPEED);
}

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
bool motor_run_cw() {
  if (safety_inhibit_motion()) return false;
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  // Re-check after mutex: ISR may have asserted ESTOP between outer check and here;
  // never pull ENA LOW if ESTOP is active (would override hardware disable path).
  if (safety_inhibit_motion()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  digitalWrite(PIN_ENA, LOW);
  stepper->runForward();
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: CW");
  return true;
}

bool motor_run_ccw() {
  if (safety_inhibit_motion()) return false;
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  if (safety_inhibit_motion()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  digitalWrite(PIN_ENA, LOW);
  stepper->runBackward();
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: CCW");
  return true;
}

void motor_stop() {
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper == nullptr) { xSemaphoreGive(g_stepperMutex); return; }
  stepper->stopMove();
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: stopping (smooth decel)");
}

void motor_halt() {
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper == nullptr) { xSemaphoreGive(g_stepperMutex); return; }
  stepper->forceStop();
  digitalWrite(PIN_ENA, HIGH);
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: HALT");
}

void motor_disable() {
  // Safe to call before motor_init(): ENA is the only side effect and is
  // already driven HIGH at the very top of setup(). Guard against null mutex.
  if (g_stepperMutex == nullptr) {
    digitalWrite(PIN_ENA, HIGH);
    return;
  }
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  digitalWrite(PIN_ENA, HIGH);
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: disabled");
}

// ───────────────────────────────────────────────────────────────────────────────
// STATUS QUERIES
// ───────────────────────────────────────────────────────────────────────────────
bool motor_is_running() {
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  bool running = (stepper != nullptr) && stepper->isRunning();
  xSemaphoreGive(g_stepperMutex);
  return running;
}

float motor_get_step_frequency_hz() {
  return (float)g_stepperMilliHzAbsCached.load(std::memory_order_acquire) / 1000.0f;
}

void motor_refresh_hz_cache(void) {
  uint32_t absMilli = 0;
  if (g_stepperMutex != nullptr && xSemaphoreTake(g_stepperMutex, portMAX_DELAY) == pdTRUE) {
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
  }
  g_stepperMilliHzAbsCached.store(absMilli, std::memory_order_release);
}

uint32_t motor_get_current_hz() {
  float hz = motor_get_step_frequency_hz();
  if (hz < 0.001f) return 0u;
  if (hz >= (float)UINT32_MAX) return UINT32_MAX;
  return (uint32_t)(hz + 0.5f);
}

uint32_t motor_milli_hz_for_rpm_calibrated(float rpm_workpiece_command) {
  float hz = rpmToStepHzCalibrated(rpm_workpiece_command);
  if (hz != hz || hz < 0.0f) {
    hz = 0.0f;
  }
  double mhzD = (double)hz * 1000.0;
  if (mhzD > (double)UINT32_MAX) {
    mhzD = (double)UINT32_MAX;
  }
  uint32_t mhz = (uint32_t)mhzD;
  if (mhz < (uint32_t)START_SPEED * 1000u) {
    mhz = (uint32_t)START_SPEED * 1000u;
  }
  return mhz;
}

void motor_apply_speed_for_rpm_locked(float rpm_workpiece_command) {
  if (stepper == nullptr) return;
  uint32_t mhz = motor_milli_hz_for_rpm_calibrated(rpm_workpiece_command);
  stepper->setSpeedInMilliHz(mhz);
  stepper->applySpeedAcceleration();
}

void motor_set_target_milli_hz(uint32_t mhz) {
  if (g_stepperMutex == nullptr) return;
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper != nullptr) {
    stepper->setSpeedInMilliHz(mhz);
    stepper->applySpeedAcceleration();
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

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper != nullptr) {
    stepper->setAcceleration(accelSteps);
    stepper->setLinearAcceleration(200);
    motor_apply_stepper_dir_timing(dirDelayUs);
  }
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: accel=%d driver=%s (DIR delay %u us)",
        accelSteps,
        driverKind == STEPPER_DRIVER_DM542T ? "DM542T" : "Standard",
        (unsigned)dirDelayUs);
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
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper != nullptr) {
    stepper->setAcceleration(softAccel);
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
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper != nullptr) {
    stepper->setAcceleration(configured);
  }
  s_temp_accel_applied = false;
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: restored accel=%u", (unsigned)configured);
}

FastAccelStepper* motor_get_stepper() {
  return stepper;
}
