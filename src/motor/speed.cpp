// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion

#include "speed.h"
#include "../config.h"
#include "motor.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define IIR_ALPHA       0.1f    // ADC filter coefficient (lower = smoother)
#define SLIDER_TIMEOUT_MS 500   // How long slider preference lasts

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static float adcFiltered = 2047.5f;     // Filtered ADC value (center of 0–4095)
static float sliderRPM = MIN_RPM;       // Speed set by GUI slider
static uint32_t lastSliderMs = 0;       // Last time slider was used
static Direction currentDir = DIR_CW;   // Current direction

// ───────────────────────────────────────────────────────────────────────────────
// CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
float rpmToStepHz(float rpm_workpiece) {
  // Formula: RPM_workpiece × GearRatio × (D_workpiece / D_roller) × StepsPerRev / 60
  // Simplified: rpm × 20 × (300/80) × 1600 / 60
  // Result: 1 RPM → 2000 Hz, 5 RPM → 10000 Hz

  return rpm_workpiece * GEAR_RATIO * (D_EMNE / D_RULLE) * STEPS_PER_REV / 60.0f;
}

long angleToSteps(float degrees) {
  // Convert workpiece angle to motor steps
  // GearRatio × (D_roller / D_workpiece) × (angle/360) × StepsPerRev

  float motor_deg = degrees * GEAR_RATIO * (D_RULLE / D_EMNE);
  return (long)(motor_deg / 360.0f * STEPS_PER_REV);
}

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONTROL
// ───────────────────────────────────────────────────────────────────────────────
void speed_init() {
  // Read initial ADC value
  adcFiltered = (float)analogRead(PIN_POT);
  LOG_I("Speed control init: pot=%.0f", adcFiltered);
}

void speed_update_adc() {
  // IIR filter: new = α × raw + (1-α) × old
  float raw = (float)analogRead(PIN_POT);
  adcFiltered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * adcFiltered;
}

void speed_slider_set(float rpm) {
  sliderRPM = constrain(rpm, MIN_RPM, MAX_RPM);
  lastSliderMs = millis();
}

float speed_get_target_rpm() {
  // Slider has priority for 500ms after last use
  if (millis() - lastSliderMs < SLIDER_TIMEOUT_MS) {
    return sliderRPM;
  }

  // Otherwise use potentiometer
  float adc = constrain(adcFiltered, 50.0f, 4045.0f);
  return MIN_RPM + (adc - 50.0f) / 3995.0f * (MAX_RPM - MIN_RPM);
}

float speed_get_actual_rpm() {
  // Convert motor speed back to workpiece RPM
  uint32_t hz = motor_get_current_hz();
  if (hz == 0) return 0.0f;

  // Reverse of rpmToStepHz formula
  float rpm_motor = (float)hz * 60.0f / STEPS_PER_REV;
  float rpm_workpiece = rpm_motor / GEAR_RATIO * (D_RULLE / D_EMNE);

  return rpm_workpiece;
}

bool speed_using_slider() {
  return (millis() - lastSliderMs < SLIDER_TIMEOUT_MS);
}

void speed_apply() {
  // Only update if motor is running
  if (!motor_is_running()) return;

  float rpm = speed_get_target_rpm();
  uint32_t hz = (uint32_t)rpmToStepHz(rpm);

  // NOTE: Direction is set by mode entry functions (motor_run_cw/ccw, jog_start, etc.)
  // Do NOT set DIR pin here — it would conflict with jog direction overrides.

  // Apply speed using motor API
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->setSpeedInHz(hz);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// DIRECTION CONTROL
// ───────────────────────────────────────────────────────────────────────────────
Direction speed_get_direction() {
  return currentDir;
}

void speed_set_direction(Direction dir) {
  currentDir = dir;
}
