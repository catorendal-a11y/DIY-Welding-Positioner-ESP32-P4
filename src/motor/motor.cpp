// TIG Rotator Controller - Motor Control Implementation
// TB6600 stepper driver + FastAccelStepper library

#include "motor.h"
#include "../storage/storage.h"
#include "speed.h"
#include "../config.h"
#include <FastAccelStepper.h>

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER ENGINE AND STEPPER INSTANCE
// ───────────────────────────────────────────────────────────────────────────────
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;

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

  // Initialize engine
  engine.init();

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
  stepper->setAcceleration(ACCELERATION);
  // Linear ramp through resonance zone (NEMA 23: 100-300 RPM)
  // 200 steps linear phase → handover at ~sqrt(1.5 * 7000 * 200) = ~1449 Hz
  stepper->setLinearAcceleration(200);
  stepper->setSpeedInHz(START_SPEED);

  LOG_I("FastAccelStepper init OK");
  LOG_I("  Steps/rev: %d", STEPS_PER_REV);
  LOG_I("  Accel: %d steps/s2", ACCELERATION);
  LOG_I("  Start speed: %d Hz", START_SPEED);
}

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void motor_run_cw() {
  digitalWrite(PIN_ENA, LOW);    // Enable motor
  stepper->runForward();
  LOG_I("Motor: CW");
}

void motor_run_ccw() {
  digitalWrite(PIN_ENA, LOW);    // Enable motor
  stepper->runBackward();
  LOG_I("Motor: CCW");
}

void motor_stop() {
  stepper->stopMove();
  LOG_I("Motor: stopping (smooth decel)");
}

void motor_halt() {
  stepper->forceStop();
  digitalWrite(PIN_ENA, HIGH);   // Disable motor immediately
  LOG_I("Motor: HALT");
}

void motor_disable() {
  digitalWrite(PIN_ENA, HIGH);   // Disable motor (ENA HIGH = disabled)
  LOG_I("Motor: disabled");
}

// ───────────────────────────────────────────────────────────────────────────────
// STATUS QUERIES
// ───────────────────────────────────────────────────────────────────────────────
bool motor_is_running() {
  return (stepper != nullptr) && stepper->isRunning();
}

uint32_t motor_get_current_hz() {
  if (stepper == nullptr) return 0;
  int32_t mhZ = stepper->getCurrentSpeedInMilliHz();
  return (mhZ >= 0) ? (uint32_t)mhZ / 1000 : (uint32_t)(-mhZ) / 1000;
}

void motor_apply_settings() {
  if (stepper != nullptr) {
    stepper->setAcceleration(g_settings.acceleration);
    LOG_I("Motor: acceleration set to %d", g_settings.acceleration);
  }
}

FastAccelStepper* motor_get_stepper() {
  return stepper;
}
