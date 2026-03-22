// TIG Rotator Controller - Pulse Mode Screen
// Matching ui_all_screens.svg: DEGREES + PAUSE panels, small RPM gauge, START

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static float pulseDegrees = 10.0f;
static float pauseSeconds = 2.0f;
static lv_obj_t* degLabel = nullptr;
static lv_obj_t* pauseLabel = nullptr;
static lv_obj_t* rpmGaugeLabel = nullptr;
static lv_obj_t* startBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void deg_minus_cb(lv_event_t* e) {
  if (pulseDegrees > 5.0f) pulseDegrees -= 5.0f;
  lv_label_set_text_fmt(degLabel, "%.0f°", pulseDegrees);
}

static void deg_plus_cb(lv_event_t* e) {
  if (pulseDegrees < 360.0f) pulseDegrees += 5.0f;
  lv_label_set_text_fmt(degLabel, "%.0f°", pulseDegrees);
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE) {
    uint32_t onMs = (uint32_t)(pulseDegrees * 100);  // Simplified
    uint32_t offMs = (uint32_t)(pauseSeconds * 1000);
    control_start_pulse(onMs, offMs);
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "■ STOP");
    lv_obj_set_style_bg_color(startBtn, COL_RED, 0);
    lv_obj_set_style_border_color(startBtn, COL_RED, 0);
  } else if (state == STATE_PULSE) {
    control_stop();
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "▶ START");
    lv_obj_set_style_bg_color(startBtn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_border_color(startBtn, COL_GREEN, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Two parameter boxes, small gauge, start button
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "PULSE MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_PURPLE, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // State badge
  lv_obj_t* badge = lv_label_create(screen);
  lv_label_set_text(badge, "PULSE ACTIVE");
  lv_obj_set_style_text_font(badge, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(badge, COL_PURPLE, 0);
  lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 40);

  // ── DEGREES panel (left) ──
  lv_obj_t* degPanel = lv_obj_create(screen);
  lv_obj_set_size(degPanel, 340, 130);
  lv_obj_set_pos(degPanel, 30, 65);
  lv_obj_set_style_bg_color(degPanel, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(degPanel, 8, 0);
  lv_obj_set_style_border_width(degPanel, 1, 0);
  lv_obj_set_style_border_color(degPanel, COL_BORDER_SPD, 0);

  lv_obj_t* degTitle = lv_label_create(degPanel);
  lv_label_set_text(degTitle, "DEGREES");
  lv_obj_set_style_text_color(degTitle, COL_TEXT_DIM, 0);
  lv_obj_align(degTitle, LV_ALIGN_TOP_MID, 0, 8);

  degLabel = lv_label_create(degPanel);
  lv_label_set_text(degLabel, "10°");
  lv_obj_set_style_text_font(degLabel, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(degLabel, COL_TEXT, 0);
  lv_obj_align(degLabel, LV_ALIGN_CENTER, 0, 8);

  // ── PAUSE panel (right) ──
  lv_obj_t* pausePanel = lv_obj_create(screen);
  lv_obj_set_size(pausePanel, 340, 130);
  lv_obj_set_pos(pausePanel, 430, 65);
  lv_obj_set_style_bg_color(pausePanel, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(pausePanel, 8, 0);
  lv_obj_set_style_border_width(pausePanel, 1, 0);
  lv_obj_set_style_border_color(pausePanel, COL_BORDER_SPD, 0);

  lv_obj_t* pauseTitle = lv_label_create(pausePanel);
  lv_label_set_text(pauseTitle, "PAUSE");
  lv_obj_set_style_text_color(pauseTitle, COL_TEXT_DIM, 0);
  lv_obj_align(pauseTitle, LV_ALIGN_TOP_MID, 0, 8);

  pauseLabel = lv_label_create(pausePanel);
  lv_label_set_text(pauseLabel, "2.0s");
  lv_obj_set_style_text_font(pauseLabel, &lv_font_montserrat_40, 0);
  lv_obj_set_style_text_color(pauseLabel, COL_TEXT, 0);
  lv_obj_align(pauseLabel, LV_ALIGN_CENTER, 0, 8);

  // ── Small RPM gauge (bottom-left) ──
  lv_obj_t* rpmCircle = lv_obj_create(screen);
  lv_obj_set_size(rpmCircle, 120, 120);
  lv_obj_set_pos(rpmCircle, 30, 215);
  lv_obj_set_style_bg_color(rpmCircle, COL_BG, 0);
  lv_obj_set_style_radius(rpmCircle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(rpmCircle, 3, 0);
  lv_obj_set_style_border_color(rpmCircle, COL_GAUGE_TICK, 0);

  rpmGaugeLabel = lv_label_create(rpmCircle);
  lv_label_set_text(rpmGaugeLabel, "1.2");
  lv_obj_set_style_text_font(rpmGaugeLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(rpmGaugeLabel, COL_TEXT, 0);
  lv_obj_center(rpmGaugeLabel);

  // ── START button (right of gauge) ──
  startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, 340, 75);
  lv_obj_set_pos(startBtn, 430, 215);
  lv_obj_set_style_bg_color(startBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(startBtn, 8, 0);
  lv_obj_set_style_border_width(startBtn, 2, 0);
  lv_obj_set_style_border_color(startBtn, COL_GREEN, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "▶ START");
  lv_obj_set_style_text_font(startLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(startLabel, COL_GREEN, 0);
  lv_obj_center(startLabel);

  // ── Adjustment buttons (bottom) ──
  lv_obj_t* minusBtn = lv_btn_create(screen);
  lv_obj_set_size(minusBtn, 165, 60);
  lv_obj_set_pos(minusBtn, 430, 305);
  lv_obj_set_style_bg_color(minusBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(minusBtn, 6, 0);
  lv_obj_set_style_border_width(minusBtn, 1, 0);
  lv_obj_set_style_border_color(minusBtn, COL_BORDER_SPD, 0);
  lv_obj_add_event_cb(minusBtn, deg_minus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* minusLabel = lv_label_create(minusBtn);
  lv_label_set_text(minusLabel, "-10°");
  lv_obj_set_style_text_color(minusLabel, COL_TEXT_DIM, 0);
  lv_obj_center(minusLabel);

  lv_obj_t* plusBtn = lv_btn_create(screen);
  lv_obj_set_size(plusBtn, 165, 60);
  lv_obj_set_pos(plusBtn, 605, 305);
  lv_obj_set_style_bg_color(plusBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(plusBtn, 6, 0);
  lv_obj_set_style_border_width(plusBtn, 1, 0);
  lv_obj_set_style_border_color(plusBtn, COL_BORDER_SPD, 0);
  lv_obj_add_event_cb(plusBtn, deg_plus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* plusLabel = lv_label_create(plusBtn);
  lv_label_set_text(plusLabel, "+10°");
  lv_obj_set_style_text_color(plusLabel, COL_TEXT_DIM, 0);
  lv_obj_center(plusLabel);

  // Back button
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 130, 50);
  lv_obj_set_pos(backBtn, 30, 355);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  LOG_I("Screen pulse: SVG-matched layout created");
}

void screen_pulse_update() {
  if (!screens_is_active(SCREEN_PULSE)) return;
  float rpm = speed_get_actual_rpm();
  lv_label_set_text_fmt(rpmGaugeLabel, "%.1f", rpm);
}
