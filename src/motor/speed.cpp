// TIG Rotator Controller - Speed Control Implementation
// ADC IIR filter, slider/pot source selection, RPM conversion
// Optional ADS1115 pedal ADC on touch I2C (see ENABLE_ADS1115_PEDAL in config.h)

#include "speed.h"
#include "../config.h"
#include "motor.h"
#include "microstep.h"
#include "calibration.h"
#include "../storage/storage.h"
#include "../control/control.h"
#include <atomic>
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
static_assert((int)(60.0f * 72.0f / 40.0f + 0.5f) == 108, "GEAR_RATIO must match motor.worm.svg (1:108 = 60*72/40)");

#if ENABLE_ADS1115_PEDAL
#define ADS_REG_PTR_CONVERT 0x00
#define ADS_REG_PTR_CONFIG  0x01
#define ADS_REG_PTR_LOTH    0x02
#define ADS_REG_PTR_HITH    0x03

#define ADS_CFG_CQUE_1CONV    0x0000
#define ADS_CFG_CLAT_NONLAT   0x0000
#define ADS_CFG_CPOL_ACTVLOW  0x0000
#define ADS_CFG_CMODE_TRAD    0x0000
#define ADS_CFG_MODE_SINGLE   0x0100
#define ADS_CFG_PGA_4_096V    0x0200
#define ADS_CFG_DR_128SPS     0x0080
#define ADS_CFG_MUX_SINGLE_0  0x4000
#define ADS_CFG_OS_START      0x8000

static i2c_master_dev_handle_t s_ads_dev = nullptr;

static bool ads_i2c_write_reg(uint8_t reg, uint16_t val) {
  if (!s_ads_dev) return false;
  uint8_t buf[3] = { reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
  return i2c_master_transmit(s_ads_dev, buf, sizeof(buf), pdMS_TO_TICKS(80)) == ESP_OK;
}

static bool ads_i2c_read_reg(uint8_t reg, uint16_t* out) {
  if (!s_ads_dev || !out) return false;
  uint8_t data[2];
  esp_err_t e = i2c_master_transmit_receive(s_ads_dev, &reg, 1, data, 2, pdMS_TO_TICKS(80));
  if (e != ESP_OK) return false;
  *out = ((uint16_t)data[0] << 8) | data[1];
  return true;
}

// Synchronous blocking read — used only at init. Do not call from motorTask.
static int16_t ads_read_channel0_blocking() {
  if (!s_ads_dev) return 0;
  uint16_t config = ADS_CFG_CQUE_1CONV | ADS_CFG_CLAT_NONLAT | ADS_CFG_CPOL_ACTVLOW | ADS_CFG_CMODE_TRAD
      | ADS_CFG_MODE_SINGLE | ADS_CFG_PGA_4_096V | ADS_CFG_DR_128SPS | ADS_CFG_MUX_SINGLE_0 | ADS_CFG_OS_START;
  if (!ads_i2c_write_reg(ADS_REG_PTR_CONFIG, config)) return 0;
  if (!ads_i2c_write_reg(ADS_REG_PTR_HITH, 0x8000)) return 0;
  if (!ads_i2c_write_reg(ADS_REG_PTR_LOTH, 0)) return 0;
  for (int i = 0; i < 25; i++) {
    uint16_t st = 0;
    if (!ads_i2c_read_reg(ADS_REG_PTR_CONFIG, &st)) return 0;
    if (st & 0x8000) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  uint16_t raw = 0;
  if (!ads_i2c_read_reg(ADS_REG_PTR_CONVERT, &raw)) return 0;
  return (int16_t)raw;
}

// Non-blocking ADS1115 state machine — fits inside motorTask 5ms budget.
// ADS1115 @ 128 SPS needs ~8ms; we start a conversion on one tick and read the
// result on a later tick instead of polling-blocking on I2C.
enum AdsState : uint8_t { ADS_IDLE, ADS_CONVERTING };
static AdsState s_adsState = ADS_IDLE;
static uint32_t s_adsStartedMs = 0;
static int16_t s_adsLastValue = 0;

// Kick off a new conversion (non-blocking). Returns false on I2C error.
static bool ads_start_conversion() {
  if (!s_ads_dev) return false;
  uint16_t config = ADS_CFG_CQUE_1CONV | ADS_CFG_CLAT_NONLAT | ADS_CFG_CPOL_ACTVLOW | ADS_CFG_CMODE_TRAD
      | ADS_CFG_MODE_SINGLE | ADS_CFG_PGA_4_096V | ADS_CFG_DR_128SPS | ADS_CFG_MUX_SINGLE_0 | ADS_CFG_OS_START;
  return ads_i2c_write_reg(ADS_REG_PTR_CONFIG, config);
}

// If a conversion is in flight and >= ~8ms old, read and complete it.
// Returns true when a fresh value was fetched; *out holds last good value.
static bool ads_poll_and_start(int16_t* out) {
  const uint32_t now = millis();
  if (s_adsState == ADS_CONVERTING && (now - s_adsStartedMs) >= 9u) {
    uint16_t raw = 0;
    if (ads_i2c_read_reg(ADS_REG_PTR_CONVERT, &raw)) {
      s_adsLastValue = (int16_t)raw;
    }
    s_adsState = ADS_IDLE;
  }
  if (s_adsState == ADS_IDLE) {
    if (ads_start_conversion()) {
      s_adsState = ADS_CONVERTING;
      s_adsStartedMs = now;
    }
  }
  if (out) *out = s_adsLastValue;
  return s_adsState == ADS_IDLE;  // Unused for now; informational.
}
#endif

#define IIR_ALPHA       0.1f
#define SLIDER_TIMEOUT_MS 1000

static float adcFiltered = 2047.5f;
static std::atomic<float> sliderRPM{MIN_RPM};
static std::atomic<float> rpmMaxUi{MAX_RPM};
static std::atomic<uint32_t> lastSliderMs{0};
static std::atomic<uint8_t> currentDir{DIR_CW};
static std::atomic<bool> buttonsActive{false};
static std::atomic<float> lastPotAdc{2047.5f};
static std::atomic<bool> pedalEnabled{false};
static std::atomic<bool> pedalApplyPending{false};
static float pedalFiltered = 2047.5f;
static std::atomic<float> cachedTargetRpm{MIN_RPM};
static std::atomic<bool> sliderPriorityOverride{false};
static uint8_t lastDirSwitchState = 1;
#define POT_WAKE_THRESHOLD 30

static bool ads1115Connected = false;
#if ENABLE_ADS1115_PEDAL
#define ADS1115_TO_ADC_SCALE (4095.0f / 26667.0f)
#endif

// UI-set OD in millimeters; 0 = use D_EMNE (meters) from config
static std::atomic<float> g_workpiece_od_mm{0.0f};

static float effective_emne_d_m(void) {
  float mm = g_workpiece_od_mm.load(std::memory_order_relaxed);
  if (mm < 1.0f) return D_EMNE;
  return mm / 1000.0f;
}

void speed_set_workpiece_diameter_mm(float mm_od) {
  if (mm_od < 1.0f || mm_od > 20000.0f) {
    g_workpiece_od_mm.store(0.0f, std::memory_order_relaxed);
  } else {
    g_workpiece_od_mm.store(mm_od, std::memory_order_relaxed);
  }
}

float speed_get_workpiece_diameter_mm(void) {
  return g_workpiece_od_mm.load(std::memory_order_relaxed);
}

// Workpiece <-> motor (no slip at roller contact):
//   v = pi * d_emne * f_wp = pi * D_RULLE * f_out  =>  f_out = f_wp * (d_emne / D_RULLE).
//   f_motor = GEAR_RATIO * f_out  (1:108 total; see motor.worm.svg + config.h GEAR_RATIO).
// Forward Hz:  rpmToStepHz. Inverse display: speed_get_actual_rpm (same GEAR_RATIO and d/D_RULLE).
float rpmToStepHz(float rpm_workpiece) {
  const float d_m = effective_emne_d_m();
  return rpm_workpiece * GEAR_RATIO * (d_m / D_RULLE) * microstep_get_steps_per_rev() / 60.0f;
}

// Command-side calibration (same factor intent as calibration_apply_steps on angleToSteps).
float rpmToStepHzCalibrated(float rpm_command) {
  return rpmToStepHz(rpm_command * calibration_get_factor());
}

long angleToSteps(float degrees) {
  const float d_m = effective_emne_d_m();
  float motor_deg = degrees * GEAR_RATIO * (d_m / D_RULLE);
  long steps = (long)(motor_deg / 360.0f * microstep_get_steps_per_rev());
  return calibration_apply_steps(steps);
}

void speed_init() {
  analogReadResolution(12);
  (void)analogRead(PIN_POT);
  analogSetPinAttenuation(PIN_POT, ADC_11db);

#if ENABLE_ADS1115_PEDAL
  i2c_master_bus_handle_t bus = display_touch_i2c_bus_handle();
  if (bus) {
    esp_err_t probe = i2c_master_probe(bus, ADS1115_ADDR, pdMS_TO_TICKS(80));
    if (probe != ESP_OK) {
      LOG_D("ADS1115 probe 0x%02X: %s", ADS1115_ADDR, esp_err_to_name(probe));
    } else {
      i2c_device_config_t dev_cfg = {};
      dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
      dev_cfg.device_address = ADS1115_ADDR;
      dev_cfg.scl_speed_hz = 400000;
      if (i2c_master_bus_add_device(bus, &dev_cfg, &s_ads_dev) == ESP_OK) {
        uint16_t cfgProbe = 0;
        if (ads_i2c_read_reg(ADS_REG_PTR_CONFIG, &cfgProbe)) {
          ads1115Connected = true;
          LOG_I("ADS1115 on touch I2C 0x%02X", ADS1115_ADDR);
        } else {
          i2c_master_bus_rm_device(s_ads_dev);
          s_ads_dev = nullptr;
          LOG_D("ADS1115 config read failed at 0x%02X", ADS1115_ADDR);
        }
      }
    }
  }
#endif

  delay(10);
  adcFiltered = (float)analogRead(PIN_POT);
#if ENABLE_ADS1115_PEDAL
  if (ads1115Connected) {
    int16_t v = ads_read_channel0_blocking();
    pedalFiltered = (float)v * ADS1115_TO_ADC_SCALE;
  } else
#endif
  {
    pedalFiltered = adcFiltered;
  }
  lastDirSwitchState = digitalRead(PIN_DIR_SWITCH);
  speed_sync_rpm_limits_from_settings();

  bool pedalPersist = false;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  pedalPersist = g_settings.pedal_enabled;
  xSemaphoreGive(g_settings_mutex);
#if ENABLE_ADS1115_PEDAL
  if (pedalPersist && ads1115Connected) {
    pedalEnabled.store(true, std::memory_order_release);
  } else {
    pedalEnabled.store(false, std::memory_order_release);
    if (pedalPersist && !ads1115Connected) {
      xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
      g_settings.pedal_enabled = false;
      xSemaphoreGive(g_settings_mutex);
      storage_save_settings();
    }
  }
#else
  (void)pedalPersist;
  pedalEnabled.store(false, std::memory_order_release);
#endif

  LOG_I("Speed control init: pot=%.0f pedal=%.0f ads=%d pedal_on=%d", adcFiltered, pedalFiltered,
        (int)ads1115Connected, (int)pedalEnabled.load(std::memory_order_acquire));
}

void speed_sync_rpm_limits_from_settings() {
  float mx = MAX_RPM;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  mx = g_settings.max_rpm;
  xSemaphoreGive(g_settings_mutex);
  if (mx < MIN_RPM) mx = MIN_RPM;
  if (mx > MAX_RPM) mx = MAX_RPM;
  rpmMaxUi.store(mx, std::memory_order_release);

  float cap = rpmMaxUi.load(std::memory_order_relaxed);
  float sr = sliderRPM.load(std::memory_order_relaxed);
  if (sr > cap) sliderRPM.store(cap, std::memory_order_relaxed);
  float ct = cachedTargetRpm.load(std::memory_order_relaxed);
  if (ct > cap) cachedTargetRpm.store(cap, std::memory_order_relaxed);
}

float speed_get_rpm_max() {
  return rpmMaxUi.load(std::memory_order_acquire);
}

void speed_update_adc() {
  bool pedalSettingsTick = pedalApplyPending.exchange(false, std::memory_order_acq_rel);
  if (pedalSettingsTick) {
    buttonsActive.store(false, std::memory_order_release);
  }

  float raw = (float)analogRead(PIN_POT);
  float prev = adcFiltered;
  adcFiltered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * adcFiltered;
  if (fabsf(adcFiltered - prev) > POT_WAKE_THRESHOLD) {
    g_wakePending.store(true, std::memory_order_release);
  }

#if ENABLE_ADS1115_PEDAL
  if (pedalEnabled.load(std::memory_order_acquire) && ads1115Connected) {
    int16_t adsVal = 0;
    ads_poll_and_start(&adsVal);  // non-blocking; returns last known value
    float pedalAdc = (float)adsVal * ADS1115_TO_ADC_SCALE;
    if (pedalSettingsTick) {
      pedalFiltered = pedalAdc;
      lastPotAdc.store(adcFiltered, std::memory_order_release);
    } else {
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
  }
}

void speed_slider_set(float rpm) {
  float cap = rpmMaxUi.load(std::memory_order_relaxed);
  float r = constrain(rpm, MIN_RPM, cap);
  sliderRPM.store(r, std::memory_order_release);
  lastSliderMs.store(millis(), std::memory_order_release);
  buttonsActive.store(true, std::memory_order_release);
  lastPotAdc.store(adcFiltered, std::memory_order_release);
  // Immediate: controlTask may call step_execute before next speed_apply tick.
  cachedTargetRpm.store(r, std::memory_order_release);
}

void speed_set_slider_priority(bool on) {
  sliderPriorityOverride.store(on, std::memory_order_release);
}

float speed_get_target_rpm() {
  return cachedTargetRpm.load(std::memory_order_acquire);
}

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

void speed_apply() {
  // Cache motor_is_running() once — each call takes g_stepperMutex.
  const bool motorRunning = motor_is_running();

  bool pedOn = pedalEnabled.load(std::memory_order_acquire);
  bool usePedal = pedOn && pedalFiltered > 100.0f && pedalFiltered < 3900.0f;
  float activeAdc = usePedal ? pedalFiltered : adcFiltered;
  float adc = constrain(activeAdc, 0.0f, 4095.0f);
  float normalized = (3315.0f - adc) / 3315.0f;
  normalized = constrain(normalized, 0.0f, 1.0f);
  float snapMax = motorRunning ? (float)POT_ADC_SNAP_MAX_RPM_RUNNING
                               : (float)POT_ADC_SNAP_MAX_RPM;
  if (snapMax > 0.0f && adc <= snapMax) {
    normalized = 1.0f;
  }
  float cap = rpmMaxUi.load(std::memory_order_relaxed);
  float pot_rpm = MIN_RPM + normalized * (cap - MIN_RPM);

  bool active = buttonsActive.load(std::memory_order_acquire);
  float srpm = sliderRPM.load(std::memory_order_relaxed);

  if (sliderPriorityOverride.load(std::memory_order_acquire)) {
    cachedTargetRpm.store(srpm, std::memory_order_relaxed);
  } else if (active) {
    float lastAdc = lastPotAdc.load(std::memory_order_relaxed);
    float adcDelta = fabsf(activeAdc - lastAdc);
    float rpmDelta = fabsf(pot_rpm - srpm);
    if (adcDelta > 200.0f || rpmDelta > POT_SLIDER_OVERRIDE_RPM_DELTA) {
      buttonsActive.store(false, std::memory_order_release);
      sliderRPM.store(pot_rpm, std::memory_order_relaxed);
      cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
    } else {
      cachedTargetRpm.store(srpm, std::memory_order_relaxed);
    }
  } else {
    cachedTargetRpm.store(pot_rpm, std::memory_order_relaxed);
  }

  if (!motorRunning) return;
  SystemState state = control_get_state();
  if (state == STATE_JOG || state == STATE_STEP || state == STATE_STOPPING || state == STATE_ESTOP) return;

  uint32_t mhz = motor_milli_hz_for_rpm_calibrated(
      cachedTargetRpm.load(std::memory_order_relaxed));
  motor_set_target_milli_hz(mhz);
}

void speed_request_update() {
}

Direction speed_get_direction() {
  Direction dir;
  if (g_dir_switch_cache.load(std::memory_order_acquire)) {
    dir = digitalRead(PIN_DIR_SWITCH) ? DIR_CW : DIR_CCW;
  } else {
    dir = (Direction)currentDir.load(std::memory_order_acquire);
  }
  bool invert = false;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  invert = g_settings.invert_direction;
  xSemaphoreGive(g_settings_mutex);
  if (invert) {
    dir = (dir == DIR_CW) ? DIR_CCW : DIR_CW;
  }
  return dir;
}

void speed_set_direction(Direction dir) {
  currentDir.store((uint8_t)dir, std::memory_order_release);
}

void speed_set_pedal_enabled(bool enabled) {
#if ENABLE_ADS1115_PEDAL
  if (enabled && !ads1115Connected) return;
#else
  if (enabled) return;
#endif
  pedalEnabled.store(enabled, std::memory_order_release);
  pedalApplyPending.store(true, std::memory_order_release);
}

bool speed_get_pedal_enabled() {
  return pedalEnabled.load(std::memory_order_acquire);
}

bool speed_pedal_connected() {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected && pedalEnabled.load(std::memory_order_acquire);
#else
  return false;
#endif
}

bool speed_ads1115_pedal_present(void) {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected;
#else
  return false;
#endif
}
