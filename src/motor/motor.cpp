// TIG Rotator Controller - Motor Control Implementation
// TB6600 stepper driver + FastAccelStepper library

#include "motor.h"
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
  // CRITICAL: ENA must already be HIGH when this is called
  assert(digitalRead(PIN_ENA) == HIGH && "ENA must be HIGH before motor_gpio_init()");

  // Direction pin
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);  // Default CCW

  // Step pin
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);

  // ESTOP input (will be moved to safety module in Phase 3)
  pinMode(PIN_ESTOP, INPUT_PULLUP);

  // Potentiometer ADC
  analogReadResolution(12);  // 12-bit ADC (0–4095)
  analogSetPinAttenuation(PIN_POT, ADC_11db);  // Full range 0–3.3V

  LOG_I("Motor GPIO init OK");
  LOG_I("  ENA=%d (must be HIGH)", digitalRead(PIN_ENA));
  LOG_I("  DIR=%d STEP=%d", digitalRead(PIN_DIR), digitalRead(PIN_STEP));
  LOG_I("  ESTOP=%d", digitalRead(PIN_ESTOP));
}

// ───────────────────────────────────────────────────────────────────────────────
// FASTACCELSTEPPER INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void motor_init() {
  motor_gpio_init();

  // Initialize engine
  engine.init();

  // Connect stepper to step pin
  stepper = engine.stepperConnectToPin(PIN_STEP);
  if (stepper == nullptr) {
    LOG_E("FastAccelStepper init failed");
    ESP.restart();
  }

  // Configure stepper
  stepper->setDirectionPin(PIN_DIR);
  stepper->setEnablePin(PIN_ENA, false);  // false = active LOW
  stepper->setAutoEnable(false);  // Manual enable control

  // Set acceleration and initial speed
  stepper->setAcceleration(ACCELERATION);   // steps/s² from config.h
  stepper->setSpeedInHz(10000);            // 10 kHz = 5 RPM workpiece (for testing)

  LOG_I("FastAccelStepper init OK");
  LOG_I("  Steps/rev: %d", STEPS_PER_REV);
  LOG_I("  Initial speed: 10000 Hz (5 RPM workpiece)");
}

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR CONTROL FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void motor_run_cw() {
  digitalWrite(PIN_DIR, HIGH);   // CW direction
  digitalWrite(PIN_ENA, LOW);    // Enable motor
  stepper->runForward();
  LOG_I("Motor: CW");
}

void motor_run_ccw() {
  digitalWrite(PIN_DIR, LOW);    // CCW direction
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

// ───────────────────────────────────────────────────────────────────────────────
// STATUS QUERIES
// ───────────────────────────────────────────────────────────────────────────────
bool motor_is_running() {
  return (stepper != nullptr) && stepper->isRunning();
}

uint32_t motor_get_current_hz() {
  if (stepper == nullptr) return 0;
  return stepper->getCurrentSpeedInMilliHz() / 1000;
}

FastAccelStepper* motor_get_stepper() {
  return stepper;
}
