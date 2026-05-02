// PC Simulator - Hardware/control/storage stubs for LVGL host build

#include "stubs/Arduino.h"

#include "../src/app_state.h"
#include "../src/config.h"
#include "../src/control/control.h"
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
SemaphoreHandle_t g_stepperMutex = nullptr;

esp_lcd_panel_handle_t display_panel = nullptr;
esp_lcd_touch_handle_t display_touch = nullptr;
void* display_framebuffer = nullptr;

static SystemState s_state = STATE_IDLE;
static Direction s_direction = DIR_CW;
static bool s_directionOverride = false;
static Direction s_directionOverrideValue = DIR_CW;
static float s_targetRpm = 0.50f;
static float s_jogRpm = 0.20f;
static float s_workpieceDiameterMm = 0.0f;
static float s_calibrationFactor = 1.0f;
static uint32_t s_stepCount = 0;
static float s_stepAccumulated = 0.0f;
static bool s_sliderPriority = false;
static bool s_pedalEnabled = true;
static uint32_t s_eventVersion = 0;
static std::vector<EventLogEntry> s_events;

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
  g_stepperMutex = xSemaphoreCreateRecursiveMutex();

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

  g_presets.clear();
  g_presets.push_back(make_preset(1, "ROOT PASS", STATE_RUNNING, 0.35f));
  g_presets.push_back(make_preset(2, "PULSE TACK", STATE_PULSE, 0.20f));
  g_presets.push_back(make_preset(3, "INDEX 90", STATE_STEP, 0.15f));

  event_log_add("SIMULATOR START");
}

void simulator_tick() {
  if (s_state == STATE_RUNNING || s_state == STATE_PULSE || s_state == STATE_JOG) {
    // UI-only simulation: keep status alive without producing motion commands.
  }
}

// Display / LVGL HAL
void display_init() {}
i2c_master_bus_handle_t display_touch_i2c_bus_handle() { return nullptr; }
void display_set_brightness(uint8_t) {}
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
void dim_reset_activity() {}
void dim_update() {}

// Control
void control_init() { s_state = STATE_IDLE; }
bool control_transition_to(SystemState state) { s_state = state; return true; }
SystemState control_get_state() { return s_state; }
const char* control_state_name(SystemState state) {
  switch (state) {
    case STATE_IDLE: return "IDLE";
    case STATE_RUNNING: return "RUNNING";
    case STATE_PULSE: return "PULSE";
    case STATE_STEP: return "STEP";
    case STATE_JOG: return "JOG";
    case STATE_STOPPING: return "STOPPING";
    case STATE_ESTOP: return "ESTOP";
    default: return "UNKNOWN";
  }
}
const char* control_get_state_string() { return control_state_name(s_state); }
bool control_start_continuous(bool, uint32_t) { s_state = STATE_RUNNING; event_log_add("SIM RUN"); return true; }
bool control_stop() { s_state = STATE_IDLE; event_log_add("SIM STOP"); return true; }
bool control_start_pulse(uint32_t, uint32_t, uint16_t) { s_state = STATE_PULSE; event_log_add("SIM PULSE"); return true; }
bool control_start_step(float angleDeg) {
  s_state = STATE_STEP;
  s_stepAccumulated += angleDeg;
  s_stepCount++;
  event_log_add("SIM STEP");
  return true;
}
bool control_start_step_sequence(float angleDeg, uint16_t repeats, float) {
  s_state = STATE_STEP;
  s_stepAccumulated += angleDeg * repeats;
  s_stepCount += repeats;
  event_log_add("SIM STEP SEQ");
  return true;
}
void control_reset_step_accumulator() { s_stepAccumulated = 0.0f; s_stepCount = 0; }
float control_get_step_accumulated() { return s_stepAccumulated; }
long control_get_step_count() { return (long)s_stepCount; }
bool control_start_jog_cw() { s_direction = DIR_CW; s_state = STATE_JOG; return true; }
bool control_start_jog_ccw() { s_direction = DIR_CCW; s_state = STATE_JOG; return true; }
bool control_stop_jog() { if (s_state == STATE_JOG) s_state = STATE_IDLE; return true; }
void control_set_jog_speed(float rpm) { s_jogRpm = constrain(rpm, MIN_RPM, sim_rpm_cap()); }
float control_get_jog_speed() { return s_jogRpm; }
void controlTask(void*) {}

// Program executor
ProgramExecutorResult program_executor_start_preset(const Preset* preset) {
  if (!preset) return PROGRAM_EXEC_INVALID_PRESET;
  s_targetRpm = constrain(preset->rpm, MIN_RPM, sim_rpm_cap());
  s_direction = (preset->direction == DIR_CCW) ? DIR_CCW : DIR_CW;
  if (preset->mode == STATE_PULSE) s_state = STATE_PULSE;
  else if (preset->mode == STATE_STEP) s_state = STATE_STEP;
  else s_state = STATE_RUNNING;
  event_log_add("SIM PROGRAM START");
  return PROGRAM_EXEC_OK;
}
const char* program_executor_result_name(ProgramExecutorResult result) {
  switch (result) {
    case PROGRAM_EXEC_OK: return "OK";
    case PROGRAM_EXEC_INVALID_PRESET: return "INVALID_PRESET";
    case PROGRAM_EXEC_BLOCKED_SAFETY: return "BLOCKED_SAFETY";
    case PROGRAM_EXEC_BLOCKED_STATE: return "BLOCKED_STATE";
    case PROGRAM_EXEC_INVALID_MODE: return "INVALID_MODE";
    case PROGRAM_EXEC_REQUEST_FAILED: return "REQUEST_FAILED";
    default: return "UNKNOWN";
  }
}

// Speed
float speed_steps_per_gear_output_rev(void) { return microstep_get_steps_per_rev() * GEAR_RATIO; }
float rpmToStepHz(float rpmWorkpiece) {
  float rollerScale = D_EMNE / D_RULLE;
  return rpmWorkpiece * speed_steps_per_gear_output_rev() * rollerScale / 60.0f;
}
float rpmToStepHzCalibrated(float rpmCommand) { return rpmToStepHz(rpmCommand * s_calibrationFactor); }
long angleToSteps(float degrees) { return angleToStepsForDiameter(degrees, s_workpieceDiameterMm); }
long angleToStepsForDiameter(float degrees, float mmOd) {
  float odM = (mmOd > 0.0f) ? (mmOd / 1000.0f) : D_EMNE;
  float steps = speed_steps_per_gear_output_rev() * (odM / D_RULLE) * (degrees / 360.0f);
  return (long)(steps * s_calibrationFactor + 0.5f);
}
void speed_set_workpiece_diameter_mm(float mmOd) { s_workpieceDiameterMm = mmOd < 0.0f ? 0.0f : mmOd; }
float speed_get_workpiece_diameter_mm(void) { return s_workpieceDiameterMm; }
void speed_init() {}
void speed_update_adc() {}
void speed_sync_rpm_limits_from_settings() {}
float speed_get_rpm_max() { return sim_rpm_cap(); }
void speed_slider_set(float rpm) { s_targetRpm = constrain(rpm, MIN_RPM, sim_rpm_cap()); }
void speed_set_slider_priority(bool on) { s_sliderPriority = on; }
float speed_get_target_rpm() { return s_targetRpm; }
float speed_get_actual_rpm() {
  return (s_state == STATE_RUNNING || s_state == STATE_PULSE || s_state == STATE_JOG) ? s_targetRpm : 0.0f;
}
bool speed_using_slider() { return s_sliderPriority; }
void speed_apply() {}
Direction speed_get_direction() { return s_directionOverride ? s_directionOverrideValue : s_direction; }
void speed_set_direction(Direction dir) { s_direction = dir; }
void speed_set_program_direction_override(Direction dir) { s_directionOverride = true; s_directionOverrideValue = dir; }
void speed_clear_program_direction_override() { s_directionOverride = false; }
bool speed_program_direction_override_active() { return s_directionOverride; }
void speed_set_pedal_enabled(bool enabled) { s_pedalEnabled = enabled; g_settings.pedal_enabled = enabled; }
bool speed_get_pedal_enabled() { return s_pedalEnabled; }
bool speed_pedal_switch_enabled() { return s_pedalEnabled; }
bool speed_pedal_analog_available() { return false; }
bool speed_pedal_connected() { return s_pedalEnabled; }
bool speed_ads1115_pedal_present(void) { return false; }

// Motor
void motor_gpio_init() {}
void motor_init() {}
bool motor_run_cw() { s_direction = DIR_CW; s_state = STATE_RUNNING; return true; }
bool motor_run_ccw() { s_direction = DIR_CCW; s_state = STATE_RUNNING; return true; }
void motor_stop() { s_state = STATE_IDLE; }
void motor_halt() { s_state = STATE_IDLE; }
void motor_disable() {}
bool motor_is_running() { return s_state == STATE_RUNNING || s_state == STATE_PULSE || s_state == STATE_JOG; }
uint32_t motor_get_current_hz() { return (uint32_t)(motor_get_step_frequency_hz() + 0.5f); }
float motor_get_step_frequency_hz() { return motor_is_running() ? rpmToStepHzCalibrated(s_targetRpm) : 0.0f; }
void motor_refresh_hz_cache(void) {}
uint32_t motor_milli_hz_for_rpm_calibrated(float rpmWorkpieceCommand) {
  float hz = rpmToStepHzCalibrated(rpmWorkpieceCommand);
  if (hz < START_SPEED) hz = START_SPEED;
  return (uint32_t)(hz * 1000.0f + 0.5f);
}
void motor_apply_speed_for_rpm_locked(float rpmWorkpieceCommand) { s_targetRpm = constrain(rpmWorkpieceCommand, MIN_RPM, sim_rpm_cap()); }
void motor_set_target_milli_hz(uint32_t) {}
FastAccelStepper* motor_get_stepper() { return nullptr; }
void motor_apply_settings() {}
void motor_apply_soft_start_acceleration() {}
void motor_restore_configured_acceleration() {}
void motorTask(void*) {}

// Motor settings
void calibration_init() {}
void calibration_set_factor(float factor) {
  s_calibrationFactor = constrain(factor, 0.5f, 1.5f);
  g_settings.calibration_factor = s_calibrationFactor;
}
float calibration_get_factor() { return s_calibrationFactor; }
long calibration_apply_steps(long steps) { return (long)(steps * s_calibrationFactor + 0.5f); }
float calibration_apply_angle(float angle) { return angle / s_calibrationFactor; }
void calibration_save() { storage_save_settings(); }
bool calibration_validate() { return s_calibrationFactor >= 0.5f && s_calibrationFactor <= 1.5f; }
void microstep_init() {}
MicrostepSetting microstep_get() { return (MicrostepSetting)g_settings.microstep; }
void microstep_set(MicrostepSetting setting) { g_settings.microstep = setting; }
const char* microstep_get_string() {
  switch (g_settings.microstep) {
    case MICROSTEP_4: return "1/4";
    case MICROSTEP_8: return "1/8";
    case MICROSTEP_32: return "1/32";
    case MICROSTEP_16:
    default: return "1/16";
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
bool safety_is_estop_active() { return false; }
bool safety_is_driver_alarm_latched() { return false; }
bool safety_inhibit_motion() { return false; }
bool safety_can_reset_from_overlay() { return true; }
bool safety_is_estop_locked() { return s_state == STATE_ESTOP; }
FaultReason safety_get_fault_reason() { return FAULT_NONE; }
const char* safety_fault_reason_name(FaultReason reason) {
  return reason == FAULT_NONE ? "NONE" : "FAULT";
}
const char* safety_fault_reason_message(FaultReason reason) {
  return reason == FAULT_NONE ? "No active fault." : "Simulated fault.";
}
void safety_reset_estop() { if (s_state == STATE_ESTOP) s_state = STATE_IDLE; }
bool safety_check_ui_reset() { return false; }
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
bool storage_save_presets() { event_log_add("SIM PRESETS SAVED"); return true; }
bool storage_load_settings() { return true; }
void storage_save_settings() { event_log_add("SIM SETTINGS SAVED"); }
void storage_flush() {}
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
  g_presets.erase(std::remove_if(g_presets.begin(), g_presets.end(),
                  [id](const Preset& p) { return p.id == id; }), g_presets.end());
  return g_presets.size() != oldSize;
}
void storage_get_usage(size_t* used, size_t* total) {
  if (used) *used = 64u * 1024u;
  if (total) *total = 512u * 1024u;
}
void storage_format() {
  g_presets.clear();
  event_log_add("SIM STORAGE FORMAT");
}
void storageTask(void*) {}

// Event log
void event_log_init() { s_events.clear(); s_eventVersion++; }
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
void event_log_clear() { s_events.clear(); s_eventVersion++; }

// Temperature
void onchip_temp_init() {}
bool onchip_temp_get_celsius(float* outC) {
  if (outC) *outC = 42.0f;
  return true;
}
