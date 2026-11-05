// TIG Rotator Controller - Pulse Mode Screen
// Rotate / pause / speed cards, cycle preview with a live marker, one Start or Stop action

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../../control/control.h"
#include "../../control/modes.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../safety/safety.h"
#include "../../config.h"
#include "../../control/motion_policy.h"
#include "../../storage/storage.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t pulseOnMs = 500;
static uint32_t pulseOffMs = 500;
static float targetRpm = 0.5f;
static lv_obj_t* onTimeLabel = nullptr;
static lv_obj_t* offTimeLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* startBtn = nullptr;
static lv_obj_t* stopBtn = nullptr;
static lv_obj_t* infoDutyLabel = nullptr;
static lv_obj_t* infoCycleLabel = nullptr;
static lv_obj_t* waveMarker = nullptr;

// Waveform line point storage (3 cycles, 5 points each)
#define WAVE_CYCLES 3
static lv_point_precise_t wavePts[WAVE_CYCLES][5];
static lv_obj_t* waveLines[WAVE_CYCLES] = {nullptr};
static constexpr int kWaveW = 516, kWaveH = 134, kWaveMargin = 20, kWaveHighY = 30, kWaveLowY = 104, kMarkerSize = 14;
static bool markerOnPhase = false;
static lv_obj_t* previewCaption = nullptr;
static int adjustEditable = -1;
static uint32_t markerPhaseMs = 0;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_computed_info() {
  float onSec = pulseOnMs / 1000.0f;
  float offSec = pulseOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0.0f) ? (onSec / cycleSec * 100.0f) : 0.0f;

  if (infoDutyLabel) lv_label_set_text_fmt(infoDutyLabel, "%d %%", (int)(duty + 0.5f));
  if (infoCycleLabel) lv_label_set_text_fmt(infoCycleLabel, "%.1f s", cycleSec);
}

static void show_rpm() { ui_set_rpm_unit(rpmLabel, targetRpm); }

// ───────────────────────────────────────────────────────────────────────────────
static void update_waveform() {
  float onSec = pulseOnMs / 1000.0f;
  float offSec = pulseOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0) ? (onSec / cycleSec) : 0.5f;

  const int usableW = kWaveW - kWaveMargin * 2;
  const int cycleW = usableW / WAVE_CYCLES;
  const int onW = (int)(cycleW * duty);
  const int margin = kWaveMargin;
  const int highY = kWaveHighY;
  const int lowY = kWaveLowY;

  for (int i = 0; i < WAVE_CYCLES; i++) {
    if (!waveLines[i]) continue;
    int cx = margin + i * cycleW;
    wavePts[i][0] = {(lv_value_precise_t)(cx), (lv_value_precise_t)(lowY)};
    wavePts[i][1] = {(lv_value_precise_t)(cx), (lv_value_precise_t)(highY)};
    wavePts[i][2] = {(lv_value_precise_t)(cx + onW), (lv_value_precise_t)(highY)};
    wavePts[i][3] = {(lv_value_precise_t)(cx + onW), (lv_value_precise_t)(lowY)};
    wavePts[i][4] = {(lv_value_precise_t)(cx + cycleW), (lv_value_precise_t)(lowY)};
    lv_line_set_points(waveLines[i], wavePts[i], 5);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void remember_values();
static void back_event_cb(lv_event_t* e) {
  remember_values();
  screens_show(SCREEN_MAIN);
}

static void on_time_adj_cb(lv_event_t* e) {
  if (ui_control_state() != STATE_IDLE) return;
  int delta = (intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    pulseOnMs += 100;
  else if (pulseOnMs > PULSE_MS_MIN)
    pulseOnMs -= 100;
  if (pulseOnMs > PULSE_MS_MAX) pulseOnMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(onTimeLabel, "%.1f s", pulseOnMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void off_time_adj_cb(lv_event_t* e) {
  if (ui_control_state() != STATE_IDLE) return;
  int delta = (intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    pulseOffMs += 100;
  else if (pulseOffMs > PULSE_MS_MIN)
    pulseOffMs -= 100;
  if (pulseOffMs > PULSE_MS_MAX) pulseOffMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(offTimeLabel, "%.1f s", pulseOffMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void rpm_adj_cb(lv_event_t* e) {
  if (ui_control_state() != STATE_IDLE) return;
  const float step = ui_rpm_increment(targetRpm);
  targetRpm = speed_clamp_rpm(targetRpm + ((intptr_t)lv_event_get_user_data(e) > 0 ? step : -step));
  show_rpm();
  update_computed_info();
}

static void remember_values() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const bool changed = g_settings.pulse_on_ms != pulseOnMs || g_settings.pulse_off_ms != pulseOffMs;
  g_settings.pulse_on_ms = (uint16_t)pulseOnMs;
  g_settings.pulse_off_ms = (uint16_t)pulseOffMs;
  xSemaphoreGive(g_settings_mutex);
  if (changed) storage_save_settings();
}

static void start_event_cb(lv_event_t* e) {
  if (ui_control_state() != STATE_IDLE) return;
  if (!motor_milli_hz_for_rpm_calibrated(targetRpm)) return;
  remember_values();
  speed_slider_set(targetRpm);
  control_start_pulse(pulseOnMs, pulseOffMs);
}

static void stop_event_cb(lv_event_t* e) { control_stop(); }

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];
  lv_obj_clean(screen);
  static bool savedValuesLoaded = false;
  if (!savedValuesLoaded) {
    savedValuesLoaded = true;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    pulseOnMs = motion_pulse_clamp_ms(g_settings.pulse_on_ms);
    pulseOffMs = motion_pulse_clamp_ms(g_settings.pulse_off_ms);
    xSemaphoreGive(g_settings_mutex);
  }
  adjustEditable = -1;
  ui_create_header(screen, "Pulse rotation", "", nullptr);
  onTimeLabel = ui_create_adjust_card(screen, 24, 94, 240, "Rotate for", on_time_adj_cb);
  offTimeLabel = ui_create_adjust_card(screen, 280, 94, 240, "Pause for", off_time_adj_cb);
  rpmLabel = ui_create_adjust_card(screen, 536, 94, 240, "Target speed", rpm_adj_cb);
  lv_label_set_text_fmt(onTimeLabel, "%.1f s", pulseOnMs / 1000.0f);
  lv_label_set_text_fmt(offTimeLabel, "%.1f s", pulseOffMs / 1000.0f);
  targetRpm = speed_clamp_rpm(targetRpm);
  show_rpm();
  previewCaption = ui_create_text(screen, 24, 228, 516, "Cycle preview", FONT_SUBTITLE, COL_TEXT_DIM);
  lv_obj_t* wave = ui_create_post_card(screen, 24, 252, kWaveW, kWaveH);
  for (int i = 0; i < WAVE_CYCLES; ++i) {
    waveLines[i] = lv_line_create(wave);
    lv_obj_set_style_line_color(waveLines[i], COL_TEXT_DIM, 0);
    lv_obj_set_style_line_width(waveLines[i], 3, 0);
  }
  waveMarker = lv_obj_create(wave);
  lv_obj_remove_style_all(waveMarker);
  lv_obj_set_size(waveMarker, kMarkerSize, kMarkerSize);
  lv_obj_set_style_radius(waveMarker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(waveMarker, COL_ACCENT, 0);
  lv_obj_set_style_bg_opa(waveMarker, LV_OPA_COVER, 0);
  lv_obj_set_pos(waveMarker, kWaveMargin - kMarkerSize / 2, kWaveHighY - kMarkerSize / 2);
  ui_create_text(screen, 556, 266, 100, "Duty", FONT_SUBTITLE, COL_TEXT_DIM);
  infoDutyLabel = ui_create_text(screen, 656, 264, 120, "", FONT_LARGE, COL_TEXT);
  lv_obj_set_style_text_align(infoDutyLabel, LV_TEXT_ALIGN_RIGHT, 0);
  ui_create_separator_line(screen, 556, 302, 220, COL_BORDER);
  ui_create_text(screen, 556, 324, 100, "Cycle", FONT_SUBTITLE, COL_TEXT_DIM);
  infoCycleLabel = ui_create_text(screen, 656, 322, 120, "", FONT_LARGE, COL_TEXT);
  lv_obj_set_style_text_align(infoCycleLabel, LV_TEXT_ALIGN_RIGHT, 0);
  ui_create_btn(screen, 24, 408, 152, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  startBtn = ui_create_btn(screen, 192, 408, 584, 56, UI_LABEL_START, FONT_LARGE, UI_BTN_ACCENT, start_event_cb, nullptr);
  stopBtn = ui_create_btn(screen, 192, 408, 584, 56, UI_LABEL_STOP, FONT_LARGE, UI_BTN_DANGER, stop_event_cb, nullptr);
  lv_obj_set_user_data(startBtn, (void*)(uintptr_t)UI_ACTION_START);
  lv_obj_set_user_data(stopBtn, (void*)(uintptr_t)UI_ACTION_STOP);
  lv_obj_set_hidden(stopBtn, true);
  update_computed_info();
  update_waveform();
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE — refresh computed values and button states
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_invalidate_widgets() {
  onTimeLabel = nullptr;
  offTimeLabel = nullptr;
  rpmLabel = nullptr;
  startBtn = nullptr;
  stopBtn = nullptr;
  infoDutyLabel = nullptr;
  infoCycleLabel = nullptr;
  waveMarker = nullptr;
  previewCaption = nullptr;
  for (int i = 0; i < WAVE_CYCLES; i++) waveLines[i] = nullptr;
}

// Places the marker from the published phase, so it shows where the running cycle really is.
static void update_marker(SystemState state) {
  if (!waveMarker) return;
  int x = kWaveMargin, y = kWaveHighY;
  if (state == STATE_PULSE) {
    const bool on = pulse_is_on_phase();
    const uint32_t now = millis();
    if (on != markerOnPhase) {
      markerOnPhase = on;
      markerPhaseMs = now;
    }
    const uint32_t onMs = pulse_get_on_ms(), offMs = pulse_get_off_ms(), phaseMs = on ? onMs : offMs;
    float through = phaseMs ? (now - markerPhaseMs) / (float)phaseMs : 1.0f;
    if (through > 1.0f) through = 1.0f;
    const int cycleW = (kWaveW - kWaveMargin * 2) / WAVE_CYCLES;
    const int onW = (int)(cycleW * ((float)onMs / (float)(onMs + offMs)));
    const int offW = cycleW - onW;
    // The cycle counter increments when ON ends, so the pause belongs to the previous cycle.
    const uint32_t completed = ui_control_view().pulse_cycles;
    const uint32_t cycle = on || completed == 0 ? completed : completed - 1;
    x = kWaveMargin + (int)(cycle % WAVE_CYCLES) * cycleW + (on ? (int)(onW * through) : onW + (int)(offW * through));
    y = on ? kWaveHighY : kWaveLowY;
  }
  lv_obj_set_pos(waveMarker, x - kMarkerSize / 2, y - kMarkerSize / 2);
}

void screen_pulse_update() {
  ui_mark_motion_callback(screenRoots[SCREEN_PULSE], start_event_cb);
  if (!screens_is_active(SCREEN_PULSE)) return;
  if (!startBtn) return;

  const SystemState state = ui_control_state();
  const bool moving = state != STATE_IDLE && state != STATE_ESTOP;
  const bool showStop = moving || !ui_control_fresh();
  const bool inhibited = !ui_control_fresh() || safety_inhibit_motion() || safety_is_estop_locked() || state == STATE_ESTOP;
  const bool blocked = inhibited || !motor_milli_hz_for_rpm_calibrated(targetRpm);
  lv_obj_set_hidden(startBtn, showStop);
  lv_obj_set_hidden(stopBtn, !showStop);
  lv_obj_t* startLbl = lv_obj_get_child(startBtn, 0);
  const char* startText = blocked ? "Start blocked" : UI_LABEL_START;
  if (strcmp(lv_label_get_text(startLbl), startText) != 0) lv_label_set_text(startLbl, startText);
  lv_obj_set_disabled(startBtn, blocked);
  update_marker(state);
  if (state == STATE_PULSE) {  // The pot can take over during the run.
    targetRpm = ui_control_view().target_rpm;
    show_rpm();
  }
  const char* caption = ui_direction_switch_differs() ? "Direction switch changed; applies at next start" : "Cycle preview";
  if (strcmp(lv_label_get_text(previewCaption), caption) != 0) lv_label_set_text(previewCaption, caption);
  const int editable = state == STATE_IDLE ? 1 : 0;
  if (editable != adjustEditable) {
    adjustEditable = editable;
    for (lv_obj_t* value : {onTimeLabel, offTimeLabel, rpmLabel}) {
      lv_obj_t* card = lv_obj_get_parent(value);
      for (uint32_t i = 0; i < lv_obj_get_child_count(card); ++i) {
        lv_obj_t* child = lv_obj_get_child(card, i);
        if (lv_obj_check_type(child, &lv_button_class)) lv_obj_set_disabled(child, !editable);
      }
    }
  }
}
