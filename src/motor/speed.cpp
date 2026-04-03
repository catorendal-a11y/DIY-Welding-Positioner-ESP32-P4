// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion

#include "speed.h"
#include "../config.h"
#include "motor.h"
#include "microstep.h"
#include "calibration.h"
#include "../storage/storage.h"
#include "../control/control.h"
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ───────────────────────────────────────────────────────────────────────────────
#define IIR_ALPHA       0.1f    // ADC filter coefficient (lower = smoother)
#define SLIDER_TIMEOUT_MS 1000 // Slider has priority for 1s after last +/- press

// ───────────────────────────────────────────────────────────────────────────────
// STATE VARIABLES
// ───────────────────────────────────────────────────────────────────────────────
static float adcFiltered = 2047.5f;     // Filtered ADC value (center of 0–4095)
static std::atomic<float> sliderRPM{MIN_RPM};       // Speed set by GUI slider (Core 0/1 shared)
static uint32_t lastSliderMs = 0;       // Last time slider was used
static Direction currentDir = DIR_CW;   // Current direction
static volatile bool buttonsActive = false;      // Buttons have control, pot locked out
static volatile float lastPotAdc = 2047.5f;      // Last ADC when buttons took over
static volatile bool pedalEnabled = false;
static float pedalFiltered = 2047.5f;
static std::atomic<float> cachedTargetRpm{0.0f};

// ───────────────────────────────────────────────────────────────────────────────
// CONVERSION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
float rpmToStepHz(float rpm_workpiece) {
  // Formula: RPM × GearRatio × (D_workpiece / D_roller) × StepsPerRev / 60
  // = rpm × 199.5 × 3.75 × 1600 / 60 = rpm × 19950
  // 0.01 RPM -> 199 Hz, 0.1 RPM -> 1995 Hz, 1 RPM -> 19950 Hz
  return rpm_workpiece * GEAR_RATIO * (D_EMNE / D_RULLE) * microstep_get_steps_per_rev() / 60.0f;
}

long angleToSteps(float degrees) {
  float motor_deg = degrees * GEAR_RATIO * (D_RULLE / D_EMNE);
  long steps = (long)(motor_deg / 360.0f * microstep_get_steps_per_rev());
  return calibration_apply_steps(steps);
}

// ───────────────────────────────────────────────────────────────────────────────
// SPEED CONTROL
// ───────────────────────────────────────────────────────────────────────────────
void speed_init() {
  pinMode(PIN_POT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

  pinMode(PIN_PEDAL, INPUT);
  analogSetPinAttenuation(PIN_PEDAL, ADC_11db);

  delay(10);
  adcFiltered = (float)analogRead(PIN_POT);
  pedalFiltered = (float)analogRead(PIN_PEDAL);
  LOG_I("Speed control init: pot=%.0f pedal=%.0f", adcFiltered, pedalFiltered);
}

void speed_update_adc() {
  float raw = (float)analogRead(PIN_POT);
  adcFiltered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * adcFiltered;

  if (pedalEnabled) {
    float praw = (float)analogRead(PIN_PEDAL);
    pedalFiltered = IIR_ALPHA * praw + (1.0f - IIR_ALPHA) * pedalFiltered;
  }
}

void speed_slider_set(float rpm) {
  sliderRPM.store(constrain(rpm, MIN_RPM, MAX_RPM), std::memory_order_release);
  lastSliderMs = millis();
  buttonsActive = true;
  lastPotAdc = adcFiltered;
}

float speed_get_target_rpm() {
  return cachedTargetRpm.load(std::memory_order_acquire);
}

float speed_get_actual_rpm() {
  uint32_t hz = motor_get_current_hz();
  if (hz == 0) return 0.0f;
  float rpm_motor = (float)hz * 60.0f / microstep_get_steps_per_rev();
  float rpm_workpiece = rpm_motor / GEAR_RATIO * (D_RULLE / D_EMNE);
  return calibration_apply_angle(rpm_workpiece);
}

bool speed_using_slider() {
  return (millis() - lastSliderMs < SLIDER_TIMEOUT_MS);
}

void speed_apply() {
  bool usePedal = pedalEnabled && pedalFiltered > 100.0f && pedalFiltered < 3900.0f;
  float activeAdc = usePedal ? pedalFiltered : adcFiltered;
  float adc = constrain(activeAdc, 0.0f, 4095.0f);
  float normalized = (3315.0f - adc) / 3315.0f;
  float pot_rpm = MIN_RPM + constrain(normalized, 0.0f, 1.0f) * (MAX_RPM - MIN_RPM);

  bool active = buttonsActive;
  float srpm = sliderRPM.load(std::memory_order_relaxed);

  if (active) {
    if (fabs(activeAdc - lastPotAdc) > 200.0f) {
      buttonsActive = false;
      sliderRPM.store(pot_rpm, std::memory_order_relaxed);
      cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
    } else {
      cachedTargetRpm.store(srpm, std::memory_order_relaxed);
    }
  } else {
    cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
  }

  if (!motor_is_running()) return;
  SystemState state = control_get_state();
  if (state == STATE_JOG || state == STATE_STEP || state == STATE_STOPPING) return;

  uint32_t mhz = (uint32_t)(rpmToStepHz(cachedTargetRpm) * 1000);

  portENTER_CRITICAL(&g_stepperMutex);
  FastAccelStepper* stepper = motor_get_stepper();
  if (stepper != nullptr) {
    stepper->setSpeedInMilliHz(mhz);
    stepper->applySpeedAcceleration();
  }
  portEXIT_CRITICAL(&g_stepperMutex);
}

void speed_request_update() {
}

// ───────────────────────────────────────────────────────────────────────────────
// DIRECTION CONTROL
// ───────────────────────────────────────────────────────────────────────────────
Direction speed_get_direction() {
  if (g_dir_switch_cache) {
    return digitalRead(PIN_DIR_SWITCH) ? DIR_CW : DIR_CCW;
  }
  return currentDir;
}

void speed_set_direction(Direction dir) {
  currentDir = dir;
}

void speed_set_pedal_enabled(bool enabled) {
  pedalEnabled = enabled;
  if (enabled) {
    pedalFiltered = (float)analogRead(PIN_PEDAL);
    lastPotAdc = pedalFiltered;
    buttonsActive = false;
  }
}

bool speed_get_pedal_enabled() {
  return pedalEnabled;
}

bool speed_pedal_connected() {
  return pedalEnabled && pedalFiltered > 100.0f && pedalFiltered < 3900.0f;
}
