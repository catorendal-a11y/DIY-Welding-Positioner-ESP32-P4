// TIG Rotator Controller - Motor Control Implementation
// TB6600 stepper driver + FastAccelStepper library

#include "motor.h"
#include "../storage/storage.h"
#include "speed.h"
#include "microstep.h"
#include "../config.h"
#include "../safety/safety.h"
#include <FastAccelStepper.h>

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER ENGINE AND STEPPER INSTANCE
// ───────────────────────────────────────────────────────────────────────────────
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// STEPPER MUTEX — all stepper API calls must hold this
// FastAccelStepper is NOT thread-safe — shared between motorTask, controlTask, safetyTask, ISR
// ───────────────────────────────────────────────────────────────────────────────
portMUX_TYPE g_stepperMutex = portMUX_INITIALIZER_UNLOCKED;

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

  // ESTOP input (will be moved to safety module in Phase 3)
  pinMode(PIN_ESTOP, INPUT_PULLUP);
  pinMode(PIN_DIR_SWITCH, INPUT_PULLUP);

  LOG_I("Motor GPIO init OK");
  LOG_I("  ENA=%d (must be HIGH)", digitalRead(PIN_ENA));
  LOG_I("  DIR=%d STEP=%d", digitalRead(PIN_DIR), digitalRead(PIN_STEP));
  LOG_I("  ESTOP=%d DIR_SW=%d", digitalRead(PIN_ESTOP), digitalRead(PIN_DIR_SWITCH));
}

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void motor_init() {
  motor_gpio_init();

  // Pin stepper engine to Core 0 — motorTask, controlTask and safetyTask all run here.
  // Without pinning, FastAccelStepper's internal timer ISR may fire on Core 1
  // causing inter-core contention and jitter in step pulse timing.
  engine.init(0);

  // Connect stepper to step pin with RMT driver
  stepper = engine.stepperConnectToPin(PIN_STEP);
  if (stepper == nullptr) {
    LOG_E("FastAccelStepper init failed");
    ESP.restart();
  }

  // Configure stepper
  stepper->setDirectionPin(PIN_DIR);
  // NOTE: Do NOT call setEnablePin() — ENA is controlled manually via digitalWrite()
  // Wiring: ENA+→5V, ENA-→GPIO52. LOW=enable, HIGH=disable (active LOW)
  // FastAccelStepper's setEnablePin conflicts with manual control.

  // Set acceleration and start speed
  stepper->setAcceleration(g_settings.acceleration);
  stepper->setLinearAcceleration(200);
  stepper->setSpeedInHz(START_SPEED);

  LOG_I("FastAccelStepper init OK");
  LOG_I("  Steps/rev: %u", microstep_get_steps_per_rev());
  LOG_I("  Accel: %d steps/s2", g_settings.acceleration);
  LOG_I("  Start speed: %d Hz", START_SPEED);
}

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void motor_run_cw() {
  if (safety_is_estop_active()) return;
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper == nullptr) { portEXIT_CRITICAL(&g_stepperMutex); return; }
  digitalWrite(PIN_ENA, LOW);
  stepper->runForward();
  portEXIT_CRITICAL(&g_stepperMutex);
  LOG_I("Motor: CW");
}

void motor_run_ccw() {
  if (safety_is_estop_active()) return;
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper == nullptr) { portEXIT_CRITICAL(&g_stepperMutex); return; }
  digitalWrite(PIN_ENA, LOW);
  stepper->runBackward();
  portEXIT_CRITICAL(&g_stepperMutex);
  LOG_I("Motor: CCW");
}

void motor_stop() {
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper == nullptr) { portEXIT_CRITICAL(&g_stepperMutex); return; }
  stepper->stopMove();
  portEXIT_CRITICAL(&g_stepperMutex);
  LOG_I("Motor: stopping (smooth decel)");
}

void motor_halt() {
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper == nullptr) { portEXIT_CRITICAL(&g_stepperMutex); return; }
  stepper->forceStop();
  digitalWrite(PIN_ENA, HIGH);
  portEXIT_CRITICAL(&g_stepperMutex);
  LOG_I("Motor: HALT");
}

void motor_disable() {
  portENTER_CRITICAL(&g_stepperMutex);
  digitalWrite(PIN_ENA, HIGH);   // Disable motor (ENA HIGH = disabled)
  portEXIT_CRITICAL(&g_stepperMutex);
  LOG_I("Motor: disabled");
}

// ───────────────────────────────────────────────────────────────────────────────
// STATUS QUERIES
// ───────────────────────────────────────────────────────────────────────────────
bool motor_is_running() {
  portENTER_CRITICAL(&g_stepperMutex);
  bool running = (stepper != nullptr) && stepper->isRunning();
  portEXIT_CRITICAL(&g_stepperMutex);
  return running;
}

uint32_t motor_get_current_hz() {
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper == nullptr) { portEXIT_CRITICAL(&g_stepperMutex); return 0; }
  int32_t mhZ = stepper->getCurrentSpeedInMilliHz();
  portEXIT_CRITICAL(&g_stepperMutex);
  return (mhZ >= 0) ? (uint32_t)mhZ / 1000 : (uint32_t)(-mhZ) / 1000;
}

void motor_apply_settings() {
  portENTER_CRITICAL(&g_stepperMutex);
  if (stepper != nullptr) {
    stepper->setAcceleration(g_settings.acceleration);
    LOG_I("Motor: acceleration set to %d", g_settings.acceleration);
  }
  portEXIT_CRITICAL(&g_stepperMutex);
}

FastAccelStepper* motor_get_stepper() {
  return stepper;
}
