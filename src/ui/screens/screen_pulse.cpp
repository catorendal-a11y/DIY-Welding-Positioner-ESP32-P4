// TIG Rotator Controller - Pulse Mode Screen
// Brutalist v2.0 design — ON TIME / OFF TIME / RPM rows, computed info,
// waveform preview, START/STOP buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include "../../motor/microstep.h"

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
  // steps/s = RPM * gear_ratio * steps_per_rev / 60
  float stepsPerSec = targetRpm * GEAR_RATIO * microstep_get_steps_per_rev() / 60.0f;

  if (infoDutyLabel)
    lv_label_set_text_fmt(infoDutyLabel, "DUTY %d%%", (int)(duty + 0.5f));
  if (infoCycleLabel)
    lv_label_set_text_fmt(infoCycleLabel, "CYCLE %.1fs", cycleSec);
  if (infoFreqLabel)
    lv_label_set_text_fmt(infoFreqLabel, "FREQ %.1fHz", freq);
  if (infoStepsLabel)
    lv_label_set_text_fmt(infoStepsLabel, "STEPS/S %d", (int)(stepsPerSec + 0.5f));

  // Update progress bars (range 0-100)
  // ON bar: 0.1s..10s mapped to 0..100
  if (onBar) {
    int onPct = (int)((pulseOnMs - 100) * 100 / 9900);
    if (onPct < 0) onPct = 0;
    if (onPct > 100) onPct = 100;
    lv_bar_set_value(onBar, onPct, LV_ANIM_OFF);
  }
  // OFF bar
  if (offBar) {
    int offPct = (int)((pulseOffMs - 100) * 100 / 9900);
    if (offPct < 0) offPct = 0;
    if (offPct > 100) offPct = 100;
    lv_bar_set_value(offBar, offPct, LV_ANIM_OFF);
  }
  // RPM bar: 0.1..3.0 mapped to 0..100
  if (rpmBar) {
    int rpmPct = (int)((targetRpm - MIN_RPM) * 100 / (MAX_RPM - MIN_RPM));
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
    wavePts[i][0] = { (lv_value_precise_t)(cx), (lv_value_precise_t)(lowY) };
    wavePts[i][1] = { (lv_value_precise_t)(cx), (lv_value_precise_t)(highY) };
    wavePts[i][2] = { (lv_value_precise_t)(cx + onW), (lv_value_precise_t)(highY) };
    wavePts[i][3] = { (lv_value_precise_t)(cx + onW), (lv_value_precise_t)(lowY) };
    wavePts[i][4] = { (lv_value_precise_t)(cx + cycleW), (lv_value_precise_t)(lowY) };
    lv_line_set_points(waveLines[i], wavePts[i], 5);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

static void on_time_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  if (delta > 0) pulseOnMs += 100;
  else if (pulseOnMs > 100) pulseOnMs -= 100;
  if (pulseOnMs > 10000) pulseOnMs = 10000;
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", pulseOnMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void off_time_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  if (delta > 0) pulseOffMs += 100;
  else if (pulseOffMs > 100) pulseOffMs -= 100;
  if (pulseOffMs > 10000) pulseOffMs = 10000;
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", pulseOffMs / 1000.0f);
  update_computed_info();
  update_waveform();
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  if (delta > 0) targetRpm += 0.1f;
  else if (targetRpm > MIN_RPM) targetRpm -= 0.1f;
  if (targetRpm < MIN_RPM) targetRpm = MIN_RPM;
  if (targetRpm > MAX_RPM) targetRpm = MAX_RPM;
  lv_label_set_text_fmt(rpmLabel, "%.1f", targetRpm);
  update_computed_info();
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE) {
    speed_slider_set(targetRpm);
    control_start_pulse(pulseOnMs, pulseOffMs);
  } else {
    control_stop();
  }
}

static void stop_event_cb(lv_event_t* e) {
  control_stop();
}

// ───────────────────────────────────────────────────────────────────────────────
// Helper: create a section row with label, value, progress bar, -/+ buttons
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_pm_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                int16_t w, int16_t h, const char* text,
                                lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER_SM, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_XL, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — matching new_ui.svg: header, 3 parameter rows, info line,
// waveform preview, START/STOP buttons
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Header bar (y=0, h=30) ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "PULSE MODE");
  lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 8);

  // ── Parameter row layout constants ──
  const int secLabelX = 20;
  const int valX = 160;
  const int barX = 240;
  const int barW = 340;
  const int barH = 3;
  const int btnW = BTN_W_PM;   // 44
  const int btnH = BTN_H_PM;   // 30
  const int btnMinusX = 600;
  const int btnPlusX = 652;

  // ════════════════════════════════════════════════════════════════════════════════
  // ON TIME section (y=56)
  // ════════════════════════════════════════════════════════════════════════════════
  const int onY = 56;

  lv_obj_t* onTitle = lv_label_create(screen);
  lv_label_set_text(onTitle, "ON TIME");
  lv_obj_set_style_text_font(onTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(onTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(onTitle, secLabelX, onY);

  onTimeLabel = lv_label_create(screen);
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", pulseOnMs / 1000.0f);
  lv_obj_set_style_text_font(onTimeLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(onTimeLabel, COL_ACCENT, 0);
  lv_obj_set_pos(onTimeLabel, valX, onY);

  // Progress bar
  onBar = lv_bar_create(screen);
  lv_obj_set_size(onBar, barW, barH);
  lv_obj_set_pos(onBar, barX, onY + 8);
  lv_obj_set_style_bg_color(onBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(onBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(onBar, 0, 0);
  lv_obj_set_style_radius(onBar, 1, 0);
  lv_bar_set_range(onBar, 0, 100);
  // Indicator style
  lv_obj_set_style_bg_color(onBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(onBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, btnMinusX, onY - 2, btnW, btnH,
                "-", on_time_adj_cb, (void*)(intptr_t)-1);
  create_pm_btn(screen, btnPlusX, onY - 2, btnW, btnH,
                "+", on_time_adj_cb, (void*)(intptr_t)1);

  // Range hint
  lv_obj_t* onHint = lv_label_create(screen);
  lv_label_set_text(onHint, "0.1-10.0s");
  lv_obj_set_style_text_font(onHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(onHint, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(onHint, 710, onY);

  // ════════════════════════════════════════════════════════════════════════════════
  // OFF TIME section (y=136)
  // ════════════════════════════════════════════════════════════════════════════════
  const int offY = 136;

  lv_obj_t* offTitle = lv_label_create(screen);
  lv_label_set_text(offTitle, "OFF TIME");
  lv_obj_set_style_text_font(offTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(offTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(offTitle, secLabelX, offY);

  offTimeLabel = lv_label_create(screen);
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", pulseOffMs / 1000.0f);
  lv_obj_set_style_text_font(offTimeLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(offTimeLabel, COL_TEXT, 0);
  lv_obj_set_pos(offTimeLabel, valX, offY);

  // Progress bar
  offBar = lv_bar_create(screen);
  lv_obj_set_size(offBar, barW, barH);
  lv_obj_set_pos(offBar, barX, offY + 8);
  lv_obj_set_style_bg_color(offBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(offBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(offBar, 0, 0);
  lv_obj_set_style_radius(offBar, 1, 0);
  lv_bar_set_range(offBar, 0, 100);
  lv_obj_set_style_bg_color(offBar, COL_TEXT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(offBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, btnMinusX, offY - 2, btnW, btnH,
                "-", off_time_adj_cb, (void*)(intptr_t)-1);
  create_pm_btn(screen, btnPlusX, offY - 2, btnW, btnH,
                "+", off_time_adj_cb, (void*)(intptr_t)1);

  // Range hint
  lv_obj_t* offHint = lv_label_create(screen);
  lv_label_set_text(offHint, "0.1-10.0s");
  lv_obj_set_style_text_font(offHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(offHint, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(offHint, 710, offY);

  // ════════════════════════════════════════════════════════════════════════════════
  // RPM section (y=216)
  // ════════════════════════════════════════════════════════════════════════════════
  const int rpmY = 216;

  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, secLabelX, rpmY);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", targetRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_set_pos(rpmLabel, valX, rpmY);

  // Progress bar
  rpmBar = lv_bar_create(screen);
  lv_obj_set_size(rpmBar, barW, barH);
  lv_obj_set_pos(rpmBar, barX, rpmY + 8);
  lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(rpmBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(rpmBar, 0, 0);
  lv_obj_set_style_radius(rpmBar, 1, 0);
  lv_bar_set_range(rpmBar, 0, 100);
  lv_obj_set_style_bg_color(rpmBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(rpmBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, btnMinusX, rpmY - 2, btnW, btnH,
                "-", rpm_adj_cb, (void*)(intptr_t)-1);
  create_pm_btn(screen, btnPlusX, rpmY - 2, btnW, btnH,
                "+", rpm_adj_cb, (void*)(intptr_t)1);

  // Range hint
  lv_obj_t* rpmHint = lv_label_create(screen);
  lv_label_set_text(rpmHint, "0.02-1.0");
  lv_obj_set_style_text_font(rpmHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmHint, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(rpmHint, 710, rpmY);

  // ════════════════════════════════════════════════════════════════════════════════
  // Computed info line (y=284)
  // ════════════════════════════════════════════════════════════════════════════════
  const int infoY = 284;

  infoDutyLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoDutyLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoDutyLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoDutyLabel, 20, infoY);

  infoCycleLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoCycleLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoCycleLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoCycleLabel, 170, infoY);

  infoFreqLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoFreqLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoFreqLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoFreqLabel, 340, infoY);

  infoStepsLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoStepsLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoStepsLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoStepsLabel, 510, infoY);

  update_computed_info();

  // ════════════════════════════════════════════════════════════════════════════════
  // Waveform preview (y=330..456, dark box with square wave lines)
  // ════════════════════════════════════════════════════════════════════════════════
  const int waveY = 330;
  const int waveH = 88;
  const int waveW = 500;

  lv_obj_t* waveBox = lv_obj_create(screen);
  lv_obj_set_size(waveBox, waveW, waveH);
  lv_obj_set_pos(waveBox, 10, waveY);
  lv_obj_set_style_bg_color(waveBox, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_border_width(waveBox, 1, 0);
  lv_obj_set_style_border_color(waveBox, COL_BORDER_ROW, 0);
  lv_obj_set_style_radius(waveBox, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(waveBox, 0, 0);
  lv_obj_remove_flag(waveBox, LV_OBJ_FLAG_SCROLLABLE);

  // Draw simple square wave using line objects
  // Wave represents on/off pattern — 3 cycles across the box
  float onSec = pulseOnMs / 1000.0f;
  float offSec = pulseOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0) ? (onSec / cycleSec) : 0.5f;

  const int margin = 20;
  const int usableW = waveW - margin * 2;
  const int cycleW = usableW / WAVE_CYCLES;
  const int onW = (int)(cycleW * duty);
  const int highY = 25;
  const int lowY = waveH - 25;

  for (int i = 0; i < WAVE_CYCLES; i++) {
    int cx = margin + i * cycleW;

    // Populate static point array for this cycle
    wavePts[i][0] = { (lv_value_precise_t)(cx), (lv_value_precise_t)(lowY) };
    wavePts[i][1] = { (lv_value_precise_t)(cx), (lv_value_precise_t)(highY) };
    wavePts[i][2] = { (lv_value_precise_t)(cx + onW), (lv_value_precise_t)(highY) };
    wavePts[i][3] = { (lv_value_precise_t)(cx + onW), (lv_value_precise_t)(lowY) };
    wavePts[i][4] = { (lv_value_precise_t)(cx + cycleW), (lv_value_precise_t)(lowY) };

    lv_obj_t* cycleLine = lv_line_create(waveBox);
    lv_line_set_points(cycleLine, wavePts[i], 5);
    lv_obj_set_style_line_color(cycleLine, COL_ACCENT, 0);
    lv_obj_set_style_line_width(cycleLine, 2, 0);
    lv_obj_remove_flag(cycleLine, LV_OBJ_FLAG_CLICKABLE);
    waveLines[i] = cycleLine;  // Store for later update
  }

  // ON/OFF labels inside waveform box
  lv_obj_t* onTag = lv_label_create(waveBox);
  lv_label_set_text(onTag, "ON");
  lv_obj_set_style_text_font(onTag, FONT_SMALL, 0);
  lv_obj_set_style_text_color(onTag, COL_ACCENT, 0);
  lv_obj_set_pos(onTag, 5, highY - 12);

  lv_obj_t* offTag = lv_label_create(waveBox);
  lv_label_set_text(offTag, "OFF");
  lv_obj_set_style_text_font(offTag, FONT_SMALL, 0);
  lv_obj_set_style_text_color(offTag, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(offTag, 5, lowY - 2);

  // ════════════════════════════════════════════════════════════════════════════════
  // Bottom bar: BACK + START + STOP (y=428)
  // ════════════════════════════════════════════════════════════════════════════════
  const int botY = 428;
  const int botH = 48;
  const int botBtnW = 256;
  const int botGap = 8;
  const int botX = 8;

  // < BACK button
  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, botBtnW, botH);
  lv_obj_set_pos(backBtn, botX, botY);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_set_style_pad_all(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  // > START button
  startBtn = lv_button_create(screen);
  lv_obj_set_size(startBtn, botBtnW, botH);
  lv_obj_set_pos(startBtn, botX + botBtnW + botGap, botY);
  lv_obj_set_style_bg_color(startBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(startBtn, 2, 0);
  lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(startBtn, 0, 0);
  lv_obj_set_style_pad_all(startBtn, 0, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "> START");
  lv_obj_set_style_text_font(startLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(startLabel, COL_ACCENT, 0);
  lv_obj_center(startLabel);

  // [] STOP button
  stopBtn = lv_button_create(screen);
  lv_obj_set_size(stopBtn, botBtnW, botH);
  lv_obj_set_pos(stopBtn, botX + (botBtnW + botGap) * 2, botY);
  lv_obj_set_style_bg_color(stopBtn, COL_BG_DANGER, 0);
  lv_obj_set_style_radius(stopBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(stopBtn, 2, 0);
  lv_obj_set_style_border_color(stopBtn, COL_RED, 0);
  lv_obj_set_style_shadow_width(stopBtn, 0, 0);
  lv_obj_set_style_pad_all(stopBtn, 0, 0);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "[] STOP");
  lv_obj_set_style_text_font(stopLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(stopLabel, COL_RED, 0);
  lv_obj_center(stopLabel);

  LOG_I("Screen pulse: v2.0 layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE — refresh computed values and button states
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_update() {
  if (!screens_is_active(SCREEN_PULSE)) return;
  if (!startBtn) return;

  SystemState state = control_get_state();

  // Update START button appearance
  lv_obj_t* startLbl = lv_obj_get_child(startBtn, 0);
  if (!startLbl) return;
  if (state == STATE_PULSE) {
    lv_label_set_text(startLbl, "[] STOP");
    lv_obj_set_style_text_color(startLbl, COL_ACCENT, 0);
  } else {
    lv_label_set_text(startLbl, "> START");
    lv_obj_set_style_text_color(startLbl, COL_ACCENT, 0);
  }

  // Refresh computed info
  update_computed_info();
}
