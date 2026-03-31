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
static volatile float sliderRPM = MIN_RPM;       // Speed set by GUI slider (Core 0/1 shared)
static uint32_t lastSliderMs = 0;       // Last time slider was used
static Direction currentDir = DIR_CW;   // Current direction
static volatile bool buttonsActive = false;      // Buttons have control, pot locked out
static volatile float lastPotAdc = 2047.5f;      // Last ADC when buttons took over
static volatile bool speedUpdatePending = false;

// ───────────────────────────────────────────────────────────────────────────────
// CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
float rpmToStepHz(float rpm_workpiece) {
  // Formula: RPM × GearRatio × (D_workpiece / D_roller) × StepsPerRev / 60
  // = rpm × 199.5 × 3.75 × 1600 / 60 = rpm × 19950
  // 0.01 RPM -> 199 Hz, 0.1 RPM -> 1995 Hz, 1 RPM -> 19950 Hz
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
}

void speed_slider_set(float rpm) {
  sliderRPM = constrain(rpm, MIN_RPM, MAX_RPM);
  lastSliderMs = millis();
  buttonsActive = true;
  lastPotAdc = adcFiltered;
}

float speed_get_target_rpm() {
  float adc = constrain(adcFiltered, 0.0f, 4095.0f);
  float normalized = (3315.0f - adc) / 3315.0f;
  float pot_rpm = MIN_RPM + constrain(normalized, 0.0f, 1.0f) * (MAX_RPM - MIN_RPM);

  bool active = buttonsActive;
  float srpm = sliderRPM;

  if (active) {
    if (fabs(adcFiltered - lastPotAdc) > 200.0f) {
      buttonsActive = false;
      sliderRPM = pot_rpm;
    } else {
      return srpm;
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

void speed_apply() {
  if (!motor_is_running()) return;
  SystemState state = control_get_state();
  if (state == STATE_JOG || state == STATE_STEP || state == STATE_PULSE || state == STATE_TIMER || state == STATE_STOPPING) return;

  float rpm = speed_get_target_rpm();
  uint32_t mhz = (uint32_t)(rpmToStepHz(rpm) * 1000);

  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->setSpeedInMilliHz(mhz);
    stepper->applySpeedAcceleration();
  }
  speedUpdatePending = false;
}

void speed_request_update() {
  speedUpdatePending = true;
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
