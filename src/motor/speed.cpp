// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion
// Optional ADS1115 pedal ADC on touch I2C (see ENABLE_ADS1115_PEDAL in config.h)

#include "speed.h"
#include "../control/motion_policy.h"
#include "../config.h"
#include "motor.h"
#include "microstep.h"
#include "calibration.h"
#include "../storage/storage.h"
#include "../control/control.h"
#include <atomic>
#include "../control/input_policy.h"
#include "../safety/safety.h"
#if ENABLE_ADS1115_PEDAL
#include "../ui/display.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#endif
#include "freertos/task.h"
#include "freertos/semphr.h"

static_assert(std::atomic<float>::is_always_lock_free,
              "std::atomic<float> must be lock-free for inter-core RPM sharing");

// Same total as docs/images/motor.worm.svg: "Total Ratio 1:108" = 108 motor revs per 1 output rev.
static_assert((int)(60.0f * 72.0f / 40.0f + 0.5f) == 108,
              "GEAR_RATIO must match motor.worm.svg (1:108 = 60*72/40)");

#if ENABLE_ADS1115_PEDAL
// Full 7-bit address probe on touch I2C: LOG_I is stripped in release; Serial is not.
// Set to 0 (or -DADS_BOOT_I2C_RAW_SCAN=0) after field verification to shorten boot.
#ifndef ADS_BOOT_I2C_RAW_SCAN
#define ADS_BOOT_I2C_RAW_SCAN 1
#endif

#define ADS_REG_PTR_CONVERT 0x00
#define ADS_REG_PTR_CONFIG 0x01
#define ADS_REG_PTR_LOTH 0x02
#define ADS_REG_PTR_HITH 0x03
#define ADS_I2C_INIT_TIMEOUT_MS 80
#define ADS_I2C_RUNTIME_TIMEOUT_MS 2

#define ADS_CFG_CQUE_1CONV 0x0000
#define ADS_CFG_CLAT_NONLAT 0x0000
#define ADS_CFG_CPOL_ACTVLOW 0x0000
#define ADS_CFG_CMODE_TRAD 0x0000
#define ADS_CFG_MODE_SINGLE 0x0100
#define ADS_CFG_PGA_4_096V 0x0200
#define ADS_CFG_DR_128SPS 0x0080
#define ADS_CFG_MUX_SINGLE_0 0x4000
#define ADS_CFG_OS_START 0x8000

static i2c_master_dev_handle_t s_ads_dev = nullptr;

static bool ads_i2c_write_reg_timeout(uint8_t reg, uint16_t val, uint32_t timeout_ms) {
  if (!s_ads_dev) return false;
  uint8_t buf[3] = {reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF)};
  return i2c_master_transmit(s_ads_dev, buf, sizeof(buf), timeout_ms) == ESP_OK;
}

static bool ads_i2c_read_reg_timeout(uint8_t reg, uint16_t* out, uint32_t timeout_ms) {
  if (!s_ads_dev || !out) return false;
  uint8_t data[2];
  esp_err_t e = i2c_master_transmit_receive(s_ads_dev, &reg, 1, data, 2, timeout_ms);
  if (e != ESP_OK) return false;
  *out = ((uint16_t)data[0] << 8) | data[1];
  return true;
}

// Synchronous blocking read — used only at init. Do not call from motorTask.
static int16_t ads_read_channel0_blocking() {
  if (!s_ads_dev) return 0;
  uint16_t config = ADS_CFG_CQUE_1CONV | ADS_CFG_CLAT_NONLAT | ADS_CFG_CPOL_ACTVLOW | ADS_CFG_CMODE_TRAD |
                    ADS_CFG_MODE_SINGLE | ADS_CFG_PGA_4_096V | ADS_CFG_DR_128SPS | ADS_CFG_MUX_SINGLE_0 |
                    ADS_CFG_OS_START;
  if (!ads_i2c_write_reg_timeout(ADS_REG_PTR_CONFIG, config, ADS_I2C_INIT_TIMEOUT_MS)) return 0;
  if (!ads_i2c_write_reg_timeout(ADS_REG_PTR_HITH, 0x8000, ADS_I2C_INIT_TIMEOUT_MS)) return 0;
  if (!ads_i2c_write_reg_timeout(ADS_REG_PTR_LOTH, 0, ADS_I2C_INIT_TIMEOUT_MS)) return 0;
  for (int i = 0; i < 25; i++) {
    uint16_t st = 0;
    if (!ads_i2c_read_reg_timeout(ADS_REG_PTR_CONFIG, &st, ADS_I2C_INIT_TIMEOUT_MS)) return 0;
    if (st & 0x8000) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  uint16_t raw = 0;
  if (!ads_i2c_read_reg_timeout(ADS_REG_PTR_CONVERT, &raw, ADS_I2C_INIT_TIMEOUT_MS)) return 0;
  return (int16_t)raw;
}

// Non-blocking ADS1115 state machine — fits inside motorTask 5ms budget.
// ADS1115 @ 128 SPS needs ~8ms; we start a conversion on one tick and read the
// result on a later tick instead of polling-blocking on I2C.
enum AdsState : uint8_t { ADS_IDLE, ADS_CONVERTING };
static AdsState s_adsState = ADS_IDLE;
static uint32_t s_adsStartedMs = 0;
static int16_t s_adsLastValue = 0;
static std::atomic<uint32_t> adsSampleMs{0};
static std::atomic<bool> adsSampleValid{false};

// Kick off a new conversion (non-blocking). Returns false on I2C error.
static bool ads_start_conversion() {
  if (!s_ads_dev) return false;
  uint16_t config = ADS_CFG_CQUE_1CONV | ADS_CFG_CLAT_NONLAT | ADS_CFG_CPOL_ACTVLOW | ADS_CFG_CMODE_TRAD |
                    ADS_CFG_MODE_SINGLE | ADS_CFG_PGA_4_096V | ADS_CFG_DR_128SPS | ADS_CFG_MUX_SINGLE_0 |
                    ADS_CFG_OS_START;
  return ads_i2c_write_reg_timeout(ADS_REG_PTR_CONFIG, config, ADS_I2C_RUNTIME_TIMEOUT_MS);
}

// If a conversion is in flight and >= ~8ms old, read and complete it.
// Returns true when a fresh value was fetched; *out holds last good value.
static bool ads_poll_and_start(int16_t* out) {
  const uint32_t now = millis();
  if (out) *out = s_adsLastValue;
  if (s_adsState == ADS_IDLE) {
    if (ads_start_conversion()) {
      s_adsState = ADS_CONVERTING;
      s_adsStartedMs = now;
    }
    return false;
  }
  if (now - s_adsStartedMs < 9u) return false;
  uint16_t status = 0;
  if (!ads_i2c_read_reg_timeout(ADS_REG_PTR_CONFIG, &status, ADS_I2C_RUNTIME_TIMEOUT_MS)) {
    s_adsState = ADS_IDLE;
    return false;
  }
  if (!(status & ADS_CFG_OS_START)) {
    if (now - s_adsStartedMs >= 40u) {
      adsSampleValid.store(false);
      s_adsState = ADS_IDLE;
    }
    return false; // Conversion is busy; never timestamp the previous result as fresh.
  }
  uint16_t raw = 0;
  const bool fresh = ads_i2c_read_reg_timeout(ADS_REG_PTR_CONVERT, &raw, ADS_I2C_RUNTIME_TIMEOUT_MS);
  s_adsState = ADS_IDLE;
  if (fresh) {
    s_adsLastValue = static_cast<int16_t>(raw);
    adsSampleMs.store(now);
    adsSampleValid.store(s_adsLastValue >= 0 && s_adsLastValue <= 28000);
    if (out) *out = s_adsLastValue;
  }
  // Start the next conversion on a later poll: at most two 2ms I2C calls here.
  return fresh;
}
#endif

#define IIR_ALPHA 0.1f
#define SLIDER_TIMEOUT_MS 1000
// Direction switch (GPIO29) stability filter: the planner consumes the stable
// level only after it has persisted this long, so bounce/EMI at START cannot
// pick a transient direction. Wake detection stays on the raw edge.
#define DIR_SWITCH_STABLE_MS 30

static std::atomic<float> adcFiltered{2047.5f};
static std::atomic<float> sliderRPM{MIN_RPM};
static std::atomic<float> rpmMaxUi{MAX_RPM};
static std::atomic<uint32_t> lastSliderMs{0};
static std::atomic<uint8_t> currentDir{DIR_CW};
static std::atomic<uint8_t> programDirectionOverride{DIR_CW};
static std::atomic<bool> programDirectionOverrideActive{false};
static std::atomic<bool> buttonsActive{false};
static std::atomic<float> lastPotAdc{2047.5f};
static std::atomic<bool> baselinePedal{false};
static std::atomic<bool> pedalEnabled{false};
static std::atomic<bool> pedalApplyPending{false};
static std::atomic<float> pedalFiltered{2047.5f};
static std::atomic<float> cachedTargetRpm{MIN_RPM};
static std::atomic<bool> sliderPriorityOverride{false};
static uint8_t lastDirSwitchState = 1;
static std::atomic<bool> stableDirSwitch{true};
static uint8_t dirSwitchCandidate = 1;
static uint32_t dirSwitchCandidateMs = 0;
#define POT_WAKE_THRESHOLD 30

static bool ads1115Connected = false;
#if ENABLE_ADS1115_PEDAL
#define ADS1115_TO_ADC_SCALE (4095.0f / 26667.0f)
#endif

// UI-set OD in millimeters; 0 = use D_EMNE (meters) from config
static std::atomic<float> g_workpiece_od_mm{0.0f};

static float effective_emne_d_m(void) {
  float mm = g_workpiece_od_mm.load(std::memory_order_relaxed);
  if (mm < 1.0f || mm > 20000.0f) return D_EMNE;
  return mm / 1000.0f;
}

static float diameter_mm_to_m(float mm) {
  if (mm < 1.0f || mm > 20000.0f) return D_EMNE;
  return mm / 1000.0f;
}

void speed_set_workpiece_diameter_mm(float mm_od) {
  if (!std::isfinite(mm_od) || mm_od < 1.0f || mm_od > 20000.0f) {
    g_workpiece_od_mm.store(0.0f, std::memory_order_relaxed);
  } else {
    g_workpiece_od_mm.store(mm_od, std::memory_order_relaxed);
  }
}

float speed_get_workpiece_diameter_mm(void) { return g_workpiece_od_mm.load(std::memory_order_relaxed); }

// Workpiece <-> motor (no slip at roller contact):
//   v = pi * d_emne * f_wp = pi * D_RULLE * f_out  =>  f_out = f_wp * (d_emne / D_RULLE).
//   steps_per_gear_output_rev = spr * GEAR_RATIO (one 360° on 72T output).
//   steps per workpiece rev = steps_per_gear_output_rev * (d_emne / D_RULLE).
// Forward Hz: rpm_workpiece * steps_per_workpiece_rev / 60. Inverse: speed_get_actual_rpm.
float speed_steps_per_gear_output_rev(void) { return (float)microstep_get_steps_per_rev() * GEAR_RATIO; }

float rpmToStepHz(float rpm_workpiece) {
  const float d_m = effective_emne_d_m();
  const float steps_per_wp_rev = speed_steps_per_gear_output_rev() * (d_m / D_RULLE);
  return rpm_workpiece * steps_per_wp_rev / 60.0f;
}

// Command-side calibration (same factor intent as calibration_apply_steps on angleToSteps).
float rpmToStepHzCalibrated(float rpm_command) { return rpmToStepHz(rpm_command * calibration_get_factor()); }

long angleToSteps(float degrees) {
  return angleToStepsForDiameter(degrees, speed_get_workpiece_diameter_mm());
}

long angleToStepsForDiameter(float degrees, float mm_od) {
  const float d_m = diameter_mm_to_m(mm_od);
  const float steps_per_wp_rev = speed_steps_per_gear_output_rev() * (d_m / D_RULLE);
  int32_t steps = 0;
  motion_checked_steps(double(degrees) / 360.0 * double(steps_per_wp_rev) * calibration_get_factor(), steps);
  return steps;
}

void speed_init() {
  analogReadResolution(12);
  (void)analogRead(PIN_POT);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

#if ENABLE_ADS1115_PEDAL
  i2c_master_bus_handle_t bus = display_touch_i2c_bus_handle();
  if (!bus) {
    LOG_E("ADS1115: touch I2C bus handle is null (display/touch I2C not up)");
  } else {
    // I2C bus scan 0x01-0x7E (7-bit; 0x00 not probed). LOG_I = debug only; Serial = all builds when enabled.
    LOG_I("I2C bus scan:");
#if ADS_BOOT_I2C_RAW_SCAN
    Serial.println("[I2C] scan 0x01-0x7E (touch bus, raw Serial):");
#endif
    for (uint8_t addr = 1; addr < 127; addr++) {
      esp_err_t p = i2c_master_probe(bus, addr, pdMS_TO_TICKS(15));
      if (p == ESP_OK) {
#if ADS_BOOT_I2C_RAW_SCAN
        Serial.printf("[I2C] ACK 0x%02X\n", addr);
#endif
        LOG_I("  found device at 0x%02X", addr);
      }
    }
#if ADS_BOOT_I2C_RAW_SCAN
    Serial.println("[I2C] scan end");
#endif
    LOG_I("I2C scan done");

    // ADS1115 I2C address = 0x48 + (ADDR pin: GND,SCL,SDA,VDD) -> try all four
    static const uint8_t kAdsTryAddrs[] = {0x48, 0x49, 0x4A, 0x4B};
    for (size_t i = 0; i < sizeof(kAdsTryAddrs) && !ads1115Connected; ++i) {
      const uint8_t a = kAdsTryAddrs[i];
      if (i2c_master_probe(bus, a, pdMS_TO_TICKS(60)) != ESP_OK) continue;

      i2c_device_config_t dev_cfg = {};
      dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
      dev_cfg.device_address = a;
      dev_cfg.scl_speed_hz = 400000;
      if (i2c_master_bus_add_device(bus, &dev_cfg, &s_ads_dev) != ESP_OK) continue;

      uint16_t cfgProbe = 0;
      if (ads_i2c_read_reg_timeout(ADS_REG_PTR_CONFIG, &cfgProbe, ADS_I2C_INIT_TIMEOUT_MS)) {
        ads1115Connected = true;
        LOG_I("ADS1115 on touch I2C 0x%02X", a);
        if (a != ADS1115_ADDR) {
          LOG_I("ADS1115: ADDR not default (expected 0x48, using 0x%02X)", a);
        }
        break;
      }
      i2c_master_bus_rm_device(s_ads_dev);
      s_ads_dev = nullptr;
    }
    if (!ads1115Connected) {
      LOG_E(
          "ADS1115: no device at 0x48-0x4B on touch I2C - check VDD,GND to 3V3/GND; SDA,SCL to GPIO7/8; "
          "ADDR to GND (required on 10-pin modules); pot wiper to A0. Scan showed other devices only.");
    }
  }
#endif

  delay(10);
  adcFiltered.store((float)analogRead(PIN_POT), std::memory_order_release);
#if ENABLE_ADS1115_PEDAL
  if (ads1115Connected) {
    int16_t v = ads_read_channel0_blocking();
    pedalFiltered = (float)v * ADS1115_TO_ADC_SCALE;
  } else
#endif
  {
    pedalFiltered = adcFiltered.load(std::memory_order_acquire);
  }
  lastDirSwitchState = digitalRead(PIN_DIR_SWITCH);
  stableDirSwitch.store(lastDirSwitchState != 0, std::memory_order_relaxed);
  dirSwitchCandidate = lastDirSwitchState;
  dirSwitchCandidateMs = 0;
  speed_sync_rpm_limits_from_settings();

  bool pedalPersist = false;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  pedalPersist = g_settings.pedal_enabled;
  xSemaphoreGive(g_settings_mutex);
  pedalEnabled.store(pedalPersist, std::memory_order_release);

  LOG_I("Speed control init: pot=%.0f pedal=%.0f ads=%d pedal_on=%d",
        adcFiltered.load(std::memory_order_acquire), pedalFiltered.load(), (int)ads1115Connected,
        (int)pedalEnabled.load(std::memory_order_acquire));
}

void speed_sync_rpm_limits_from_settings() {
  float mx = MAX_RPM;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  mx = g_settings.max_rpm;
  xSemaphoreGive(g_settings_mutex);
  if (!std::isfinite(mx)) mx = MAX_RPM;
  if (mx < MIN_RPM) mx = MIN_RPM;
  if (mx > MAX_RPM) mx = MAX_RPM;
  rpmMaxUi.store(mx, std::memory_order_release);

  float cap = rpmMaxUi.load(std::memory_order_relaxed);
  float sr = sliderRPM.load(std::memory_order_relaxed);
  if (sr > cap) sliderRPM.store(cap, std::memory_order_relaxed);
  float ct = cachedTargetRpm.load(std::memory_order_relaxed);
  if (ct > cap) cachedTargetRpm.store(cap, std::memory_order_relaxed);
}

float speed_get_rpm_min() { return speed_get_rpm_min_for_diameter(speed_get_workpiece_diameter_mm()); }
float speed_get_rpm_min_for_diameter(float mm) {
  const float hz = speed_steps_per_gear_output_rev() * diameter_mm_to_m(mm) / D_RULLE * calibration_get_factor() / 60.0f;
  if (!std::isfinite(hz) || hz <= 0) return MAX_RPM + 1;
  // Round upwards to the UI's 0.001 RPM precision, never hide a rate increase.
  return std::max(MIN_RPM, std::ceil(START_SPEED / hz * 1000.0f) / 1000.0f);
}
float speed_clamp_rpm(float rpm) {
  const float minimum = speed_get_rpm_min(), maximum = speed_get_rpm_max();
  if (!std::isfinite(rpm) || minimum > maximum) return 0;
  return constrain(rpm, minimum, maximum);
}
float speed_get_rpm_max() { return rpmMaxUi.load(std::memory_order_acquire); }

void speed_update_adc() {
  bool pedalSettingsTick = pedalApplyPending.exchange(false, std::memory_order_acq_rel);
  if (pedalSettingsTick) {
    buttonsActive.store(false, std::memory_order_release);
  }

  float raw = (float)analogRead(PIN_POT);
  float prev = adcFiltered.load(std::memory_order_acquire);
  float filtered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * prev;
  adcFiltered.store(filtered, std::memory_order_release);
  if (fabsf(filtered - prev) > POT_WAKE_THRESHOLD) {
    g_wakePending.store(true, std::memory_order_release);
  }

#if ENABLE_ADS1115_PEDAL
  if (pedalEnabled.load(std::memory_order_acquire) && ads1115Connected) {
    int16_t adsVal = 0;
    const bool fresh = ads_poll_and_start(&adsVal);
    float pedalAdc = (float)adsVal * ADS1115_TO_ADC_SCALE;
    if (fresh && pedalSettingsTick) {
      pedalFiltered = pedalAdc;
      lastPotAdc.store(pedalAdc, std::memory_order_release);
      baselinePedal.store(true);
    } else if (fresh) {
      pedalFiltered = IIR_ALPHA * pedalAdc + (1.0f - IIR_ALPHA) * pedalFiltered;
    }
  }
#endif

  if (g_dir_switch_cache.load(std::memory_order_acquire)) {
    uint8_t state = digitalRead(PIN_DIR_SWITCH);
    if (state != lastDirSwitchState) {
      lastDirSwitchState = state;
      g_wakePending.store(true, std::memory_order_release);
    }
    if (state == stableDirSwitch.load(std::memory_order_relaxed)) {
      dirSwitchCandidate = state;
      dirSwitchCandidateMs = 0;
    } else if (state != dirSwitchCandidate) {
      dirSwitchCandidate = state;
      dirSwitchCandidateMs = millis();
    } else if (dirSwitchCandidateMs != 0u && millis() - dirSwitchCandidateMs >= DIR_SWITCH_STABLE_MS) {
      stableDirSwitch.store(dirSwitchCandidate != 0, std::memory_order_relaxed);
      dirSwitchCandidate = state;
      dirSwitchCandidateMs = 0;
    }
  }
}

void speed_slider_set(float rpm) {
  if (!std::isfinite(rpm)) return;
  float r = speed_clamp_rpm(rpm);
  sliderRPM.store(r, std::memory_order_release);
  lastSliderMs.store(millis(), std::memory_order_release);
  const bool pedal = speed_get_pedal_enabled() && speed_ads1115_pedal_present();
  lastPotAdc.store(pedal ? pedalFiltered.load() : adcFiltered.load(), std::memory_order_release);
  baselinePedal.store(pedal);
  buttonsActive.store(true, std::memory_order_release);
  // Immediate: controlTask may call step_execute before next speed_apply tick.
  cachedTargetRpm.store(r, std::memory_order_release);
}

void speed_set_slider_priority(bool on) { sliderPriorityOverride.store(on, std::memory_order_release); }

float speed_get_target_rpm() { return cachedTargetRpm.load(std::memory_order_acquire); }

float speed_get_actual_rpm() {
  // Hz from motor_refresh_hz_cache() (no g_stepperMutex — safe from lvglTask).
  float hz = motor_get_step_frequency_hz();
  if (hz < 1e-9f) return 0.0f;
  uint32_t spr = microstep_get_steps_per_rev();
  if (spr == 0u) return 0.0f;
  float rpm_motor = hz * 60.0f / (float)spr;
  const float d_m = effective_emne_d_m();
  float rpm_workpiece = rpm_motor / GEAR_RATIO * (D_RULLE / d_m);
  return calibration_apply_angle(rpm_workpiece);
}

bool speed_using_slider() {
  return (millis() - lastSliderMs.load(std::memory_order_acquire) < SLIDER_TIMEOUT_MS);
}

static std::atomic<SpeedInputSource> inputSource{SPEED_SOURCE_POT};
SpeedInputSource speed_get_input_source() { return inputSource.load(std::memory_order_acquire); }

void speed_apply() {
  const SystemState state = control_get_state();
  const bool liveSpeedState = (state == STATE_RUNNING || state == STATE_PULSE);
  const bool motorRunning = liveSpeedState && motor_is_running();

  const bool usePedal = speed_get_pedal_enabled() && speed_ads1115_pedal_present();
  if (usePedal && !speed_pedal_input_healthy()) {
    cachedTargetRpm.store(MIN_RPM);
    if (state != STATE_IDLE && state != STATE_ESTOP) safety_report_input_fault();
    return;
  }
  float activeAdc = usePedal ? pedalFiltered.load(std::memory_order_acquire) : adcFiltered.load(std::memory_order_acquire);
  float adc = constrain(activeAdc, 0.0f, 4095.0f);
  float normalized = (3315.0f - adc) / 3315.0f;
  normalized = constrain(normalized, 0.0f, 1.0f);
  float snapMax = motorRunning ? (float)POT_ADC_SNAP_MAX_RPM_RUNNING : (float)POT_ADC_SNAP_MAX_RPM;
  if (snapMax > 0.0f && adc <= snapMax) {
    normalized = 1.0f;
  }
  float cap = rpmMaxUi.load(std::memory_order_relaxed);
  const float minimum = speed_get_rpm_min();
  float pot_rpm = speed_clamp_rpm(minimum + normalized * (cap - minimum));

  bool active = buttonsActive.load(std::memory_order_acquire);
  float srpm = speed_clamp_rpm(sliderRPM.load(std::memory_order_relaxed));

  if (sliderPriorityOverride.load(std::memory_order_acquire) || programDirectionOverrideActive.load()) {
    cachedTargetRpm.store(srpm, std::memory_order_relaxed);
  } else if (active) {
    if (baselinePedal.exchange(usePedal) != usePedal) lastPotAdc.store(activeAdc);
    float lastAdc = lastPotAdc.load(std::memory_order_relaxed);
    float adcDelta = fabsf(activeAdc - lastAdc);
    if (adcDelta > POT_UI_TAKEOVER_ADC_DELTA) {
      buttonsActive.store(false, std::memory_order_release);
      sliderRPM.store(pot_rpm, std::memory_order_relaxed);
      cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
    } else {
      cachedTargetRpm.store(srpm, std::memory_order_relaxed);
    }
  } else {
    cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
  }

  const bool uiSource = programDirectionOverrideActive.load() ||
                        sliderPriorityOverride.load(std::memory_order_acquire) ||
                        buttonsActive.load(std::memory_order_acquire);
  inputSource.store(uiSource   ? SPEED_SOURCE_UI
                    : usePedal ? SPEED_SOURCE_PEDAL
                               : SPEED_SOURCE_POT,
                    std::memory_order_release);

  if (!motorRunning) return;

  uint32_t mhz = motor_milli_hz_for_rpm_calibrated(cachedTargetRpm.load(std::memory_order_relaxed));
  motor_set_target_milli_hz(mhz);
}

Direction speed_get_requested_direction() {
  Direction dir;
  if (programDirectionOverrideActive.load(std::memory_order_acquire)) {
    dir = (Direction)programDirectionOverride.load(std::memory_order_acquire);
  } else if (g_dir_switch_cache.load(std::memory_order_acquire)) {
    // Debounced level: a START must not sample a bouncing/EMI direction.
    dir = stableDirSwitch.load(std::memory_order_relaxed) ? DIR_CW : DIR_CCW;
  } else {
    dir = (Direction)currentDir.load(std::memory_order_acquire);
  }
  return dir;
}

Direction speed_get_direction() {
  return speed_resolve_direction(speed_get_requested_direction());
}

void speed_set_direction(Direction dir) {
  currentDir.store((uint8_t)dir, std::memory_order_release);
  programDirectionOverrideActive.store(false, std::memory_order_release);
}

void speed_set_program_direction_override(Direction dir) {
  currentDir.store((uint8_t)dir, std::memory_order_release);
  programDirectionOverride.store((uint8_t)dir, std::memory_order_release);
  programDirectionOverrideActive.store(true, std::memory_order_release);
}

void speed_clear_program_direction_override() {
  programDirectionOverrideActive.store(false, std::memory_order_release);
}

bool speed_program_direction_override_active() {
  return programDirectionOverrideActive.load(std::memory_order_acquire);
}

void speed_set_pedal_enabled(bool enabled) {
  pedalEnabled.store(enabled, std::memory_order_release);
  pedalApplyPending.store(true, std::memory_order_release);
}

bool speed_get_pedal_enabled() { return pedalEnabled.load(std::memory_order_acquire); }

bool speed_pedal_switch_enabled() { return pedalEnabled.load(std::memory_order_acquire); }

bool speed_pedal_analog_available() {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected && pedalEnabled.load(std::memory_order_acquire);
#else
  return false;
#endif
}

bool speed_pedal_connected() { return speed_pedal_switch_enabled(); }

bool speed_ads1115_pedal_present(void) {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected;
#else
  return false;
#endif
}

bool speed_pedal_input_healthy() {
#if ENABLE_ADS1115_PEDAL
  // GPIO33 switch stays usable when no ADS1115 answered at boot; only an
  // ADS1115 that was actually detected must keep delivering fresh samples.
  return pedal_input_healthy(speed_get_pedal_enabled(), ads1115Connected, adsSampleValid.load(),
                             adsSampleMs.load(), millis());
#else
  return true;  // Explicit switch-only build.
#endif
}

Direction speed_resolve_direction(Direction requested) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const bool invert = g_settings.invert_direction;
  xSemaphoreGive(g_settings_mutex);
  return motion_direction_cw(requested == DIR_CW, invert) ? DIR_CW : DIR_CCW;
}
