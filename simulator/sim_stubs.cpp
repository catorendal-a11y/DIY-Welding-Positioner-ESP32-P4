// PC Simulator - Hardware/control/storage stubs for LVGL host build

#include "stubs/Arduino.h"

#include "../src/app_state.h"
#include "../src/config.h"
#include "../src/control/control.h"
#include "../src/control/modes.h"
#include <FastAccelStepper.h>
#include "../src/control/motion_policy.h"
#include "../src/storage/save_request.h"
#include "../src/control/program_executor.h"
#include "../src/event_log.h"
#include "../src/motor/acceleration.h"
#include "../src/motor/calibration.h"
#include "../src/motor/microstep.h"
#include "../src/motor/motor.h"
#include "../src/motor/speed.h"
#include "../src/onchip_temp.h"
#include "../src/safety/safety.h"
#include "../src/storage/storage.h"
#include "../src/ui/display.h"
#include "../src/ui/lvgl_hal.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

SimSerial Serial;
SimEsp ESP;

TaskHandle_t lvglHandle = nullptr;

std::vector<Preset> g_presets;
SemaphoreHandle_t g_presets_mutex = nullptr;
SystemSettings g_settings = {};
SemaphoreHandle_t g_settings_mutex = nullptr;
SemaphoreHandle_t g_nvs_mutex = nullptr;


esp_lcd_panel_handle_t display_panel = nullptr;
esp_lcd_touch_handle_t display_touch = nullptr;
void* display_framebuffer = nullptr;

static Direction s_direction = DIR_CW;
static bool s_directionOverride = false;
static Direction s_directionOverrideValue = DIR_CW;
static float s_targetRpm = 0.50f;
static float s_workpieceDiameterMm = 0.0f;
static bool s_sliderPriority = false;
static bool s_pedalEnabled = true;
static uint32_t s_eventVersion = 0;
static std::vector<EventLogEntry> s_events;
static bool simFault = false, simAlarm = false;
static bool simControlStalled = false;
static bool simStaleInput = false, simRejectMotion = false, simNvsFailure = false;
static FaultReason simReason = FAULT_NONE;
static SaveRequest simSettingsSave{200}, simPresetsSave{100};


static float sim_rpm_cap() {
  float cap = g_settings.max_rpm;
  if (cap < MIN_RPM) cap = MIN_RPM;
  if (cap > MAX_RPM) cap = MAX_RPM;
  return cap;
}

static Preset make_preset(uint8_t id, const char* name, SystemState mode, float rpm) {
  Preset p = {};
  p.id = id;
  strlcpy(p.name, name, sizeof(p.name));
  p.mode = mode;
  p.mode_mask = PRESET_MASK_ALL;
  p.rpm = rpm;
  p.pulse_on_ms = 1200;
  p.pulse_off_ms = 800;
  p.step_angle = 90.0f;
  p.workpiece_diameter_mm = 300.0f;
  p.timer_ms = 0;
  p.direction = DIR_CW;
  p.pulse_cycles = 0;
  p.step_repeats = 4;
  p.step_dwell_sec = 0.5f;
  p.timer_auto_stop = 1;
  p.cont_soft_start = 1;
  return p;
}

void simulator_init_state() {
  g_presets_mutex = xSemaphoreCreateRecursiveMutex();
  g_settings_mutex = xSemaphoreCreateRecursiveMutex();
  g_nvs_mutex = xSemaphoreCreateRecursiveMutex();


  g_settings.acceleration = 5000;
  g_settings.microstep = MICROSTEP_16;
  g_settings.max_rpm = MAX_RPM;
  g_settings.calibration_factor = 1.0f;
  g_settings.brightness = 180;
  g_settings.dim_timeout = 0;
  g_settings.dir_switch_enabled = true;
  g_settings.invert_direction = false;
  g_settings.accent_color = 0;
  g_settings.color_scheme = 0;
  g_settings.countdown_seconds = 3;
  g_settings.stepper_driver = STEPPER_DRIVER_DM542T;
  g_settings.pedal_enabled = true;
  g_settings.settings_version = 1;
  g_settings.setup_completed = true;

  g_presets.clear();
  g_presets.push_back(make_preset(1, "ROOT PASS", STATE_RUNNING, 0.35f));
  g_presets.push_back(make_preset(2, "PULSE TACK", STATE_PULSE, 0.20f));
  g_presets.push_back(make_preset(3, "INDEX 90", STATE_STEP, 0.15f));

  motor_init(); control_init(); control_run_cycle();
  event_log_add("SIMULATOR START");
}

void simulator_fast_motion(bool fast) { simStepper.motionScale = fast ? 1000.0 : 1.0; }
void simulator_tick() {
  storage_flush();
  simStepper.commandResult = simRejectMotion ? MoveResultCode::ErrorSpeedIsUndefined : MoveResultCode::OK;
  control_check_stop_deadline(millis());
  if (!simControlStalled) control_run_cycle();
}

// Display / LVGL HAL
void display_init() {}
i2c_master_bus_handle_t display_touch_i2c_bus_handle() { return nullptr; }
static uint8_t simulatedBrightness = 180;
void display_set_brightness(uint8_t value) { simulatedBrightness = value; }
uint8_t simulator_display_brightness() { return simulatedBrightness; }
void display_fill_black() {}
void display_fill_black_sync() {}
extern "C" bool display_lvgl_vsync_callback(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void*) {
  return false;
}
void display_register_lvgl_vsync(void*) {}
void lvgl_hal_init() {}
void lvgl_alloc_buffers() {}
void lvgl_flush_cb(lv_display_t* disp, const lv_area_t*, uint8_t*) { lv_display_flush_ready(disp); }
void lvgl_touchpad_read_cb(lv_indev_t*, lv_indev_data_t* data) { data->state = LV_INDEV_STATE_RELEASED; }

// Speed
float speed_steps_per_gear_output_rev(void) { return microstep_get_steps_per_rev() * GEAR_RATIO; }
float rpmToStepHz(float rpmWorkpiece) {
  float rollerScale = (s_workpieceDiameterMm > 0 ? s_workpieceDiameterMm / 1000.0f : D_EMNE) / D_RULLE;
  return rpmWorkpiece * speed_steps_per_gear_output_rev() * rollerScale / 60.0f;
}
float rpmToStepHzCalibrated(float rpmCommand) { return rpmToStepHz(rpmCommand * calibration_get_factor()); }
long angleToSteps(float degrees) { return angleToStepsForDiameter(degrees, s_workpieceDiameterMm); }
long angleToStepsForDiameter(float degrees, float mmOd) {
  float odM = (mmOd > 0.0f) ? (mmOd / 1000.0f) : D_EMNE;
  float steps = speed_steps_per_gear_output_rev() * (odM / D_RULLE) * (degrees / 360.0f);
  int32_t checked = 0; motion_checked_steps(double(steps) * calibration_get_factor(), checked); return checked;
}
void speed_set_workpiece_diameter_mm(float mmOd) { s_workpieceDiameterMm = mmOd < 0.0f ? 0.0f : mmOd; }
float speed_get_workpiece_diameter_mm(void) { return s_workpieceDiameterMm; }
void speed_init() {}
void speed_update_adc() {}
void speed_sync_rpm_limits_from_settings() {}
float speed_get_rpm_min_for_diameter(float mm) {
  const float hz = speed_steps_per_gear_output_rev() * (mm > 0 ? mm/1000.0f : D_EMNE) / D_RULLE * calibration_get_factor() / 60.0f;
  return std::max(MIN_RPM, std::ceil(START_SPEED / hz * 1000.0f) / 1000.0f);
}
float speed_get_rpm_min() { return speed_get_rpm_min_for_diameter(s_workpieceDiameterMm); }
float speed_clamp_rpm(float rpm) { return std::isfinite(rpm) && speed_get_rpm_min() <= sim_rpm_cap() ? constrain(rpm, speed_get_rpm_min(), sim_rpm_cap()) : 0; }
float speed_get_rpm_max() { return sim_rpm_cap(); }
void speed_slider_set(float rpm) { s_targetRpm = speed_clamp_rpm(rpm); }
void speed_set_slider_priority(bool on) { s_sliderPriority = on; }
float speed_get_target_rpm() { return s_targetRpm; }
float speed_get_actual_rpm() {
  const float hzPerRpm = rpmToStepHzCalibrated(1.0f);
  return hzPerRpm > 0 ? motor_get_step_frequency_hz() / hzPerRpm : 0;
}
bool speed_using_slider() { return s_sliderPriority; }
void speed_apply() {
  if (simStaleInput && control_get_state() != STATE_IDLE && control_get_state() != STATE_ESTOP) {
    safety_report_motor_fault(FAULT_PEDAL_INPUT); return;
  }
  if ((control_get_state() == STATE_RUNNING || control_get_state() == STATE_PULSE) && motor_is_running())
    motor_set_target_milli_hz(motor_milli_hz_for_rpm_calibrated(s_targetRpm));
}
Direction speed_get_requested_direction() { return s_directionOverride ? s_directionOverrideValue : s_direction; }
Direction speed_get_direction() { return speed_resolve_direction(s_directionOverride ? s_directionOverrideValue : s_direction); }
void speed_set_direction(Direction dir) { s_direction = dir; }
void speed_set_program_direction_override(Direction dir) {
  s_directionOverride = true;
  s_directionOverrideValue = dir;
}
void speed_clear_program_direction_override() { s_directionOverride = false; }
bool speed_program_direction_override_active() { return s_directionOverride; }
void speed_set_pedal_enabled(bool enabled) {
  s_pedalEnabled = enabled;
  g_settings.pedal_enabled = enabled;
}
bool speed_get_pedal_enabled() { return s_pedalEnabled; }
bool speed_pedal_switch_enabled() { return s_pedalEnabled; }
bool speed_pedal_analog_available() { return false; }
bool speed_pedal_connected() { return s_pedalEnabled; }
bool speed_ads1115_pedal_present(void) { return false; }

// Motor settings
void microstep_init() {}
MicrostepSetting microstep_get() { return (MicrostepSetting)g_settings.microstep; }
void microstep_set(MicrostepSetting setting) { g_settings.microstep = setting; }
const char* microstep_get_string() {
  switch (g_settings.microstep) {
    case MICROSTEP_4:
      return "1/4";
    case MICROSTEP_8:
      return "1/8";
    case MICROSTEP_32:
      return "1/32";
    case MICROSTEP_16:
    default:
      return "1/16";
  }
}
uint32_t microstep_get_steps_per_rev() { return (uint32_t)g_settings.microstep * 200u; }
void microstep_save() { storage_save_settings(); }
void acceleration_init() {}
void acceleration_set(uint32_t accel) { g_settings.acceleration = constrain(accel, 1000u, 30000u); }
uint32_t acceleration_get() { return (uint32_t)g_settings.acceleration; }
void acceleration_save() { storage_save_settings(); }
bool acceleration_has_pending_apply() { return false; }
void acceleration_clear_pending() {}

// Safety
void safety_init() {}
void safety_cache_stepper() {}
void safety_attach_estop() {}
void simulator_set_estop_input(bool active) { digitalWrite(PIN_ESTOP, active ? LOW : HIGH); simFault = active; if (active) safety_report_motor_fault(FAULT_ESTOP_PRESSED); }
bool safety_is_estop_active() { return simFault; }
bool safety_is_driver_alarm_latched() { return simAlarm; }
bool safety_inhibit_motion() { return simFault || simAlarm || simStaleInput || control_get_state() == STATE_ESTOP; }
bool safety_can_reset_from_overlay() { return !simFault && !simAlarm && !simStaleInput; }
bool safety_is_estop_locked() { return control_get_state() == STATE_ESTOP; }
FaultReason safety_get_fault_reason() { return simReason; }
void safety_reset_estop() {
  if (safety_can_reset_from_overlay()) { simReason = FAULT_NONE; control_transition_to(STATE_IDLE); }
}
bool safety_check_ui_reset() {
  if (!g_uiResetPending.exchange(false) || !safety_can_reset_from_overlay()) return false;
  simReason = FAULT_NONE; return true;
}
void safety_init_watchdog() {}
void safety_feed_watchdog() {}
void safetyTask(void*) {}

// Storage
void preset_clamp_mode_to_mask(Preset* p) {
  if (!p) return;
  p->mode_mask &= PRESET_MASK_ALL;
  if (!p->mode_mask) p->mode_mask = preset_mode_to_mask(p->mode);
  if (!(p->mode_mask & preset_mode_to_mask(p->mode))) {
    p->mode = preset_first_in_mask(p->mode_mask);
  }
}
void storage_init() {}
bool storage_load_presets() { return true; }
uint32_t storage_request_presets_save() { return simPresetsSave.request(); }
StorageStatus storage_presets_save_status(uint32_t ticket) {
  if (simPresetsSave.saved(ticket)) return STORAGE_SAVED;
  return simPresetsSave.failed() ? STORAGE_ERROR : STORAGE_PENDING;
}
bool storage_save_presets() { storage_request_presets_save(); return true; }
bool storage_load_settings() { return true; }
void storage_save_settings() { simSettingsSave.request(); }
void storage_flush() {
  if (simSettingsSave.begin(millis())) simSettingsSave.complete(!simNvsFailure);
  if (simPresetsSave.begin(millis())) simPresetsSave.complete(!simNvsFailure);
}
bool storage_get_preset(uint8_t id, Preset* out) {
  for (const Preset& p : g_presets) {
    if (p.id == id) {
      if (out) *out = p;
      return true;
    }
  }
  return false;
}
bool storage_delete_preset(uint8_t id) {
  auto oldSize = g_presets.size();
  g_presets.erase(
      std::remove_if(g_presets.begin(), g_presets.end(), [id](const Preset& p) { return p.id == id; }),
      g_presets.end());
  return g_presets.size() != oldSize;
}
void storage_get_usage(size_t* used, size_t* total) {
  if (used) *used = 64u * 1024u;
  if (total) *total = 512u * 1024u;
}
bool storage_format() {
  g_presets.clear();
  event_log_add("SIM STORAGE FORMAT");
  return true;
}
void storageTask(void*) {}

// Event log
void event_log_init() {
  s_events.clear();
  s_eventVersion++;
}
void event_log_add(const char* text) {
  EventLogEntry entry = {};
  entry.ms = millis();
  strlcpy(entry.text, text ? text : "", sizeof(entry.text));
  if (s_events.size() >= EVENT_LOG_CAPACITY) {
    s_events.erase(s_events.begin());
  }
  s_events.push_back(entry);
  s_eventVersion++;
}
void event_log_addf(const char* fmt, ...) {
  char buf[EVENT_LOG_TEXT_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  event_log_add(buf);
}
size_t event_log_snapshot(EventLogEntry* out, size_t maxEntries) {
  size_t n = std::min(maxEntries, s_events.size());
  for (size_t i = 0; i < n; i++) {
    out[i] = s_events[s_events.size() - n + i];
  }
  return n;
}
uint32_t event_log_version() { return s_eventVersion; }
void event_log_clear() {
  s_events.clear();
  s_eventVersion++;
}

// Temperature
void onchip_temp_init() {}
bool onchip_temp_get_celsius(float* outC) {
  if (outC) *outC = 42.0f;
  return true;
}

SpeedInputSource speed_get_input_source() {
  return speed_using_slider() ? SPEED_SOURCE_UI : SPEED_SOURCE_POT;
}

bool speed_pedal_input_healthy() { return !simStaleInput; }
StorageStatus storage_status() {
  if (simSettingsSave.failed() || simPresetsSave.failed()) return STORAGE_ERROR;
  return simSettingsSave.pending() || simPresetsSave.pending() ? STORAGE_PENDING : STORAGE_SAVED;
}

Direction speed_resolve_direction(Direction requested) {
  return g_settings.invert_direction ? (requested == DIR_CW ? DIR_CCW : DIR_CW) : requested;
}
uint32_t storage_request_settings_save() { return simSettingsSave.request(); }
StorageStatus storage_settings_save_status(uint32_t ticket) {
  if (simSettingsSave.saved(ticket)) return STORAGE_SAVED;
  return simSettingsSave.failed() ? STORAGE_ERROR : STORAGE_PENDING;
}
bool storage_get_nvs_stats(size_t* used, size_t* total) {
  if (!used || !total) return false;
  *used = 64; *total = 630; return true;
}
bool event_log_try_snapshot(EventLogEntry* out, size_t max, size_t* count, uint32_t* version) {
  if (!out || !count || !version) return false;
  *count = event_log_snapshot(out, max); *version = s_eventVersion; return true;
}
uint32_t event_log_dropped() { return 0; }
void safety_report_motor_fault(FaultReason reason) {
  simReason = reason; digitalWrite(PIN_ENA, HIGH); control_transition_to(STATE_ESTOP);
}
void safety_register_watchdog() {}
void safety_task_ready(uint32_t) {}

bool simulator_set_scenario(const char* scenario) {
  simControlStalled = false;
  simFault = simAlarm = simStaleInput = simRejectMotion = simNvsFailure = false;
  if (std::strcmp(scenario, "none") == 0) return true;
  if (std::strcmp(scenario, "stalled-control") == 0) { simControlStalled = true; return true; }
  if (std::strcmp(scenario, "estop") == 0) { simFault = true; simReason = FAULT_ESTOP_PRESSED; }
  else if (std::strcmp(scenario, "driver-alarm") == 0) { simAlarm = true; simReason = FAULT_DRIVER_ALARM; }
  else if (std::strcmp(scenario, "stale-adc") == 0 || std::strcmp(scenario, "i2c-failure") == 0) {
    simStaleInput = true; simReason = FAULT_PEDAL_INPUT;
  }
  else if (std::strcmp(scenario, "rejected-motion") == 0) { simRejectMotion = true; return true; }
  else if (std::strcmp(scenario, "nvs-failure") == 0) { simNvsFailure = true; storage_save_settings(); return true; }
  else return false;
  control_transition_to(STATE_ESTOP);
  return true;
}
