// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion

#include "speed.h"
#include "../config.h"
#include "motor.h"
#include "../storage/storage.h"
#include "../control/control.h"

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define IIR_ALPHA       0.1f    // ADC filter coefficient (lower = smoother)
#define SLIDER_TIMEOUT_MS 1000 // Slider has priority for 10s after last +/- press

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static float adcFiltered = 2047.5f;     // Filtered ADC value (center of 0–4095)
static float sliderRPM = MIN_RPM;       // Speed set by GUI slider
static uint32_t lastSliderMs = 0;       // Last time slider was used
static Direction currentDir = DIR_CW;   // Current direction
static bool buttonsActive = false;      // Buttons have control, pot locked out
static float lastPotAdc = 2047.5f;      // Last ADC when buttons took over

// ───────────────────────────────────────────────────────────────────────────────
// CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
float rpmToStepHz(float rpm_workpiece) {
  // Formula: RPM × GearRatio × (D_workpiece / D_roller) × StepsPerRev / 60
  // = rpm × 108 × 3.75 × 1600 / 60 = rpm × 10800
  // 0.1 RPM -> 1080 Hz, 1 RPM -> 10800 Hz, 3 RPM -> 32400 Hz
  return rpm_workpiece * GEAR_RATIO * (D_EMNE / D_RULLE) * STEPS_PER_REV / 60.0f;
}

long angleToSteps(float degrees) {
  // Convert workpiece angle to motor steps
  float motor_deg = degrees * GEAR_RATIO * (D_RULLE / D_EMNE);
  return (long)(motor_deg / 360.0f * STEPS_PER_REV);
}

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONTROL
// ───────────────────────────────────────────────────────────────────────────────
void speed_init() {
  pinMode(PIN_POT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

  delay(10);
  adcFiltered = (float)analogRead(PIN_POT);
  LOG_I("Speed control init: pot=%.0f (pin=%d)", adcFiltered, PIN_POT);
}

void speed_update_adc() {
  float raw = (float)analogRead(PIN_POT);
  adcFiltered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * adcFiltered;
  LOG_D("ADC raw=%.0f filtered=%.0f rpm=%.2f", raw, adcFiltered, speed_get_target_rpm());
}

void speed_slider_set(float rpm) {
  sliderRPM = constrain(rpm, MIN_RPM, MAX_RPM);
  lastSliderMs = millis();
  if (g_settings.rpm_buttons_enabled) {
    buttonsActive = true;
    lastPotAdc = adcFiltered;
  }
}

float speed_get_target_rpm() {
  float adc = constrain(adcFiltered, 0.0f, 4095.0f);
  float normalized = (3000.0f - adc) / 3000.0f;
  float pot_rpm = MIN_RPM + constrain(normalized, 0.0f, 1.0f) * (MAX_RPM - MIN_RPM);

  if (buttonsActive) {
    if (fabs(adcFiltered - lastPotAdc) > 80.0f) {
      buttonsActive = false;
      sliderRPM = pot_rpm;
    } else {
      return sliderRPM;
    }
  }

  return pot_rpm;
}

float speed_get_actual_rpm() {
  uint32_t hz = motor_get_current_hz();
  if (hz == 0) return 0.0f;
  float rpm_motor = (float)hz * 60.0f / STEPS_PER_REV;
  float rpm_workpiece = rpm_motor / GEAR_RATIO * (D_RULLE / D_EMNE);
  return rpm_workpiece;
}

bool speed_using_slider() {
  return (millis() - lastSliderMs < SLIDER_TIMEOUT_MS);
}

static Direction lastAppliedDir = DIR_CW;

void speed_apply() {
  if (!motor_is_running()) return;
  SystemState state = control_get_state();
  if (state == STATE_JOG || state == STATE_STEP || state == STATE_PULSE || state == STATE_TIMER || state == STATE_STOPPING) return;

  float rpm = speed_get_target_rpm();
  uint32_t hz = (uint32_t)rpmToStepHz(rpm);
  Direction dir = speed_get_direction();

  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->setSpeedInHz(hz);
    if (dir != lastAppliedDir) {
      lastAppliedDir = dir;
    }
    if (dir == DIR_CW) {
      stepper->runForward();
    } else {
      stepper->runBackward();
    }
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// DIRECTION CONTROL
// ───────────────────────────────────────────────────────────────────────────────
Direction speed_get_direction() {
  if (g_settings.dir_switch_enabled) {
    return digitalRead(PIN_DIR_SWITCH) ? DIR_CW : DIR_CCW;
  }
  return currentDir;
}

void speed_set_direction(Direction dir) {
  currentDir = dir;
}
