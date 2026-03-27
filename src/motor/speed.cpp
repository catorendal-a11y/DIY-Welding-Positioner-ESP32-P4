// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion

#include "speed.h"
#include "../config.h"
#include "motor.h"
#include "calibration.h"
#include "microstep.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define IIR_ALPHA       0.1f    // ADC filter coefficient (lower = smoother)
#define SLIDER_TIMEOUT_MS 10000  // Button/slider priority for 10 seconds (was 500ms)

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static float adcFiltered = 2047.5f;     // Filtered ADC value (center of 0–4095)
static float sliderRPM = MIN_RPM;       // Speed set by GUI slider/buttons
static uint32_t lastSliderMs = 0;       // Last time slider/buttons were used
static bool usingButtons = false;      // True when +/- buttons are being used
static Direction currentDir = DIR_CW;   // Current direction

// ───────────────────────────────────────────────────────────────────────────────
// CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
float rpmToStepHz(float rpm_output) {
  // Direct gear ratio: 1:108 total
  // Formula: RPM_output × GearRatio × StepsPerRev / 60
  // StepsPerRev is dynamic based on microstep setting
  uint32_t stepsPerRev = microstep_get_steps_per_rev();
  return rpm_output * GEAR_RATIO * stepsPerRev / 60.0f;
}

long angleToSteps(float degrees) {
  // Convert output angle to motor steps with calibration
  // Formula: degrees × GearRatio × StepsPerRev / 360 × calibrationFactor
  // calibrationFactor > 1.0 = need more steps (actual rotation less than expected)
  // calibrationFactor < 1.0 = need fewer steps (actual rotation more than expected)
  // StepsPerRev is dynamic based on microstep setting

  uint32_t stepsPerRev = microstep_get_steps_per_rev();
  float theoreticalSteps = degrees * GEAR_RATIO * stepsPerRev / 360.0f;
  return (long)(theoreticalSteps * calibration_get_factor());
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
  usingButtons = true;  // Mark that buttons are being used
}

float speed_get_target_rpm() {
  // If buttons were used recently, stay in button mode for 10 seconds
  if (usingButtons && (millis() - lastSliderMs < SLIDER_TIMEOUT_MS)) {
    return sliderRPM;
  }

  // Check if potentiometer has changed significantly (if so, take over)
  float raw = (float)analogRead(PIN_POT);
  float adc = constrain(raw, 50.0f, 4045.0f);
  float potRPM = MIN_RPM + (adc - 50.0f) / 3995.0f * (MAX_RPM - MIN_RPM);

  // If pot differs from current slider value by more than 0.2 RPM, pot takes over
  if (usingButtons && fabs(potRPM - sliderRPM) > 0.2f) {
    usingButtons = false;  // Potentiometer takes control
    sliderRPM = potRPM;    // Sync to pot position
  }

  if (!usingButtons) {
    // Use potentiometer
    return potRPM;
  }

  // Fallback to slider value
  return sliderRPM;
}

float speed_get_actual_rpm() {
  // Convert motor speed back to output RPM
  uint32_t hz = motor_get_current_hz();
  if (hz == 0) return 0.0f;

  // Sanity check: Hz should be reasonable
  // If Hz is unreasonably high (>100kHz), return 0
  if (hz > 100000) return 0.0f;

  // Reverse of rpmToStepHz formula
  // RPM = Hz × 60 / (GearRatio × StepsPerRev)
  uint32_t stepsPerRev = microstep_get_steps_per_rev();
  float rpm = (float)hz * 60.0f / (GEAR_RATIO * stepsPerRev);

  // Threshold: if RPM is very low (< 0.1), treat as stopped
  // This eliminates noise readings when motor is disabled
  if (rpm < 0.1f) return 0.0f;

  // Clamp to reasonable range
  if (rpm < 0) return 0.0f;
  if (rpm > MAX_RPM * 2) return MAX_RPM;  // Cap at 2x max for safety

  return rpm;
}

bool speed_using_slider() {
  return (millis() - lastSliderMs < SLIDER_TIMEOUT_MS);
}

void speed_apply() {
  // Get target RPM from pot or slider
  float rpm = speed_get_target_rpm();
  uint32_t hz = (uint32_t)rpmToStepHz(rpm);

  // Apply speed using motor API
  // NOTE: We set speed even when not running, so it's ready when motor starts
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->setSpeedInHz(hz);
  }

  // DEBUG: Log when speed changes (only every 100 cycles to avoid spam)
  static uint32_t lastDebugHz = 0;
  static uint32_t debugCount = 0;
  if (++debugCount >= 100) {
    debugCount = 0;
    if (hz != lastDebugHz) {
      LOG_D("Speed: %.1f RPM = %u Hz", rpm, hz);
      lastDebugHz = hz;
    }
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
