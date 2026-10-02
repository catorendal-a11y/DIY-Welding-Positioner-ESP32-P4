// TIG Rotator Controller - Pulse Mode Screen
// Brutalist v2.0 design — ON TIME / OFF TIME / RPM rows, computed info,
// waveform preview, START/STOP buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

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
static lv_obj_t* onBar = nullptr;
static lv_obj_t* offBar = nullptr;
static lv_obj_t* rpmBar = nullptr;
static lv_obj_t* infoDutyLabel = nullptr;
static lv_obj_t* infoCycleLabel = nullptr;
static lv_obj_t* infoFreqLabel = nullptr;
static lv_obj_t* infoStepsLabel = nullptr;

// Waveform line point storage (3 cycles, 5 points each)
#define WAVE_CYCLES 3
static lv_point_precise_t wavePts[WAVE_CYCLES][5];
static lv_obj_t* waveLines[WAVE_CYCLES] = {nullptr};

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_computed_info() {
  float onSec = pulseOnMs / 1000.0f;
  float offSec = pulseOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0.0f) ? (onSec / cycleSec * 100.0f) : 0.0f;
  float freq = (cycleSec > 0.0f) ? (1.0f / cycleSec) : 0.0f;
  // Step pulse rate matches motorTask (roller + gear + microstep + calibration)
  float stepsPerSec = rpmToStepHzCalibrated(targetRpm);

  if (infoDutyLabel) lv_label_set_text_fmt(infoDutyLabel, "DUTY %d%%", (int)(duty + 0.5f));
  if (infoCycleLabel) lv_label_set_text_fmt(infoCycleLabel, "CYCLE %.1fs", cycleSec);
  if (infoFreqLabel) lv_label_set_text_fmt(infoFreqLabel, "FREQ %.1fHz", freq);
  if (infoStepsLabel) lv_label_set_text_fmt(infoStepsLabel, "STEPS/S %d", (int)(stepsPerSec + 0.5f));

  // Update progress bars (range 0-100)
  const uint32_t pulseSpan = PULSE_MS_MAX - PULSE_MS_MIN;
  // ON/OFF bars: shared pulse range mapped to 0..100
  if (onBar) {
    int onPct = (int)((pulseOnMs - PULSE_MS_MIN) * 100 / pulseSpan);
    if (onPct < 0) onPct = 0;
    if (onPct > 100) onPct = 100;
    lv_bar_set_value(onBar, onPct, LV_ANIM_OFF);
  }
  // OFF bar
  if (offBar) {
    int offPct = (int)((pulseOffMs - PULSE_MS_MIN) * 100 / pulseSpan);
    if (offPct < 0) offPct = 0;
    if (offPct > 100) offPct = 100;
    lv_bar_set_value(offBar, offPct, LV_ANIM_OFF);
  }
  // RPM bar: 0.1..3.0 mapped to 0..100
  if (rpmBar) {
    float mx = speed_get_rpm_max();
    float span = mx - MIN_RPM;
    if (span < 1e-6f) span = 1e-6f;
    int rpmPct = (int)((targetRpm - MIN_RPM) * 100.0f / span + 0.5f);
    if (rpmPct < 0) rpmPct = 0;
    if (rpmPct > 100) rpmPct = 100;
    lv_bar_set_value(rpmBar, rpmPct, LV_ANIM_OFF);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
static void update_waveform() {
  float onSec = pulseOnMs / 1000.0f;
  float offSec = pulseOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0) ? (onSec / cycleSec) : 0.5f;

  const int waveW = 500;
  const int waveH = 88;
  const int margin = 20;
  const int usableW = waveW - margin * 2;
  const int cycleW = usableW / WAVE_CYCLES;
  const int onW = (int)(cycleW * duty);
  const int highY = 25;
  const int lowY = waveH - 25;

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
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

static void on_time_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    pulseOnMs += 100;
  else if (pulseOnMs > PULSE_MS_MIN)
    pulseOnMs -= 100;
  if (pulseOnMs > PULSE_MS_MAX) pulseOnMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", pulseOnMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void off_time_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    pulseOffMs += 100;
  else if (pulseOffMs > PULSE_MS_MIN)
    pulseOffMs -= 100;
  if (pulseOffMs > PULSE_MS_MAX) pulseOffMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", pulseOffMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    targetRpm += 0.1f;
  else if (targetRpm > MIN_RPM)
    targetRpm -= 0.1f;
  float mx = speed_get_rpm_max();
  if (targetRpm < MIN_RPM) targetRpm = MIN_RPM;
  if (targetRpm > mx) targetRpm = mx;
  ui_set_rpm(rpmLabel, targetRpm);
  update_computed_info();
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = ui_control_state();
  if (state == STATE_IDLE) {
    speed_slider_set(targetRpm);
    control_start_pulse(pulseOnMs, pulseOffMs);
  } else {
    control_stop();
  }
}

static void stop_event_cb(lv_event_t* e) { control_stop(); }

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — matching new_ui.svg: header, 3 parameter rows, info line,
// waveform preview, START/STOP buttons
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];
  lv_obj_clean(screen);
  ui_create_header(screen, "Pulse rotation", "CYCLE SETUP", nullptr);
  onTimeLabel = ui_create_adjust_card(screen, 24, 94, 240, "ROTATE FOR / s", on_time_adj_cb);
  ui_highlight_value_card(onTimeLabel);
  offTimeLabel = ui_create_adjust_card(screen, 280, 94, 240, "PAUSE FOR / s", off_time_adj_cb);
  rpmLabel = ui_create_adjust_card(screen, 536, 94, 240, "TARGET SPEED / RPM", rpm_adj_cb);
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", pulseOnMs / 1000.0f);
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", pulseOffMs / 1000.0f);
  ui_set_rpm(rpmLabel, targetRpm);
  onBar = offBar = rpmBar = nullptr;
  ui_create_text(screen, 24, 238, 500, "CYCLE PREVIEW / continuous repeat", FONT_SUBTITLE, COL_TEXT_DIM);
  lv_obj_t* wave = ui_create_post_card(screen, 24, 270, 500, 88);
  for (int i = 0; i < WAVE_CYCLES; ++i) {
    waveLines[i] = lv_line_create(wave);
    lv_obj_set_style_line_color(waveLines[i], COL_ACCENT, 0);
    lv_obj_set_style_line_width(waveLines[i], 3, 0);
  }
  infoDutyLabel = ui_create_text(screen, 548, 272, 228, "", FONT_SUBTITLE, COL_TEXT);
  infoCycleLabel = ui_create_text(screen, 548, 302, 228, "", FONT_SUBTITLE, COL_TEXT_DIM);
  infoFreqLabel = ui_create_text(screen, 548, 332, 228, "", FONT_SUBTITLE, COL_TEXT_DIM);
  infoStepsLabel = nullptr;
  ui_create_btn(screen, 24, 408, 152, 56, "<  BACK", FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  startBtn =
      ui_create_btn(screen, 192, 408, 272, 56, "> START", FONT_BTN, UI_BTN_ACCENT, start_event_cb, nullptr);
  stopBtn =
      ui_create_btn(screen, 480, 408, 296, 56, "[] STOP", FONT_BTN, UI_BTN_DANGER, stop_event_cb, nullptr);
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
  onBar = nullptr;
  offBar = nullptr;
  rpmBar = nullptr;
  infoDutyLabel = nullptr;
  infoCycleLabel = nullptr;
  infoFreqLabel = nullptr;
  infoStepsLabel = nullptr;
  for (int i = 0; i < WAVE_CYCLES; i++) waveLines[i] = nullptr;
}

void screen_pulse_update() {
  if (!screens_is_active(SCREEN_PULSE)) return;
  if (!startBtn) return;

  SystemState state = ui_control_state();

  // Update START button appearance
  lv_obj_t* startLbl = lv_obj_get_child(startBtn, 0);
  if (!startLbl) return;
  if (state == STATE_PULSE) {
    lv_label_set_text(startLbl, "[] STOP");
    lv_obj_set_style_text_color(startLbl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  } else {
    lv_label_set_text(startLbl, "> START");
    lv_obj_set_style_text_color(startLbl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  }

  // Refresh computed info
  update_computed_info();
}
