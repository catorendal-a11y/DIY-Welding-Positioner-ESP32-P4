// TIG Rotator Controller - Pulse Mode Screen (T-24)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* pulseOnLabel = nullptr;
static lv_obj_t* pulseOffLabel = nullptr;
static uint32_t pulseOnMs = 500;
static uint32_t pulseOffMs = 500;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE) {
    control_start_pulse(pulseOnMs, pulseOffMs);
    lv_obj_t* label = lv_obj_get_child(lv_event_get_target(e), 0);
    lv_label_set_text(label, "■ STOP PULSE");
    lv_obj_set_style_bg_color(lv_event_get_target(e), COL_RED, 0);
  } else if (state == STATE_PULSE) {
    control_stop();
    lv_obj_t* label = lv_obj_get_child(lv_event_get_target(e), 0);
    lv_label_set_text(label, "▶ START PULSE");
    lv_obj_set_style_bg_color(lv_event_get_target(e), COL_GREEN_DARK, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];

  // Header with back button
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, STATUS_BAR_H);
  lv_obj_set_style_bg_color(backBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_set_style_text_color(backLabel, COL_ACCENT, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "◉ PULSE MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_PURPLE, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // PULSE ON panel (left)
  lv_obj_t* onPanel = lv_obj_create(screen);
  lv_obj_set_size(onPanel, 222, 120);
  lv_obj_set_pos(onPanel, 12, 44);
  lv_obj_set_style_bg_color(onPanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(onPanel, RADIUS_CARD, 0);

  lv_obj_t* onTitle = lv_label_create(onPanel);
  lv_label_set_text(onTitle, "PULSE ON");
  lv_obj_set_style_text_color(onTitle, COL_TEXT_LABEL, 0);
  lv_obj_align(onTitle, LV_ALIGN_TOP_MID, 0, 8);

  pulseOnLabel = lv_label_create(onPanel);
  lv_label_set_text(pulseOnLabel, "500");
  lv_obj_set_style_text_font(pulseOnLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(pulseOnLabel, LV_ALIGN_CENTER, 0, 0);

  // PULSE OFF panel (right)
  lv_obj_t* offPanel = lv_obj_create(screen);
  lv_obj_set_size(offPanel, 222, 120);
  lv_obj_set_pos(offPanel, 246, 44);
  lv_obj_set_style_bg_color(offPanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(offPanel, RADIUS_CARD, 0);

  lv_obj_t* offTitle = lv_label_create(offPanel);
  lv_label_set_text(offTitle, "PULSE OFF");
  lv_obj_set_style_text_color(offTitle, COL_TEXT_LABEL, 0);
  lv_obj_align(offTitle, LV_ALIGN_TOP_MID, 0, 8);

  pulseOffLabel = lv_label_create(offPanel);
  lv_label_set_text(pulseOffLabel, "500");
  lv_obj_set_style_text_font(pulseOffLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(pulseOffLabel, LV_ALIGN_CENTER, 0, 0);

  // Speed slider
  lv_obj_t* speedLabel = lv_label_create(screen);
  lv_label_set_text(speedLabel, "SPEED");
  lv_obj_set_style_text_color(speedLabel, COL_TEXT_DIM, 0);
  lv_obj_align(speedLabel, LV_ALIGN_TOP_MID, -200, 180);

  lv_obj_t* slider = lv_slider_create(screen);
  lv_obj_set_size(slider, 200, 20);
  lv_obj_align(slider, LV_ALIGN_TOP_MID, -80, 178);

  // START/STOP button
  lv_obj_t* startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, 220, 56);
  lv_obj_set_pos(startBtn, 12, 264);
  lv_obj_set_style_bg_color(startBtn, COL_GREEN_DARK, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "▶ START PULSE");
  lv_obj_center(startLabel);

  // BACK button
  lv_obj_t* backBtn2 = lv_btn_create(screen);
  lv_obj_set_size(backBtn2, 224, 56);
  lv_obj_set_pos(backBtn2, 244, 264);
  lv_obj_set_style_bg_color(backBtn2, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn2, RADIUS_BTN, 0);
  lv_obj_add_event_cb(backBtn2, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel2 = lv_label_create(backBtn2);
  lv_label_set_text(backLabel2, "◄ BACK");
  lv_obj_center(backLabel2);

  LOG_I("Screen pulse: created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_pulse_update() {
  // Update labels based on current state
  // TODO: Implement full update logic
}
