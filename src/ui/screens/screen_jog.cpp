// TIG Rotator Controller - Jog Mode Screen (T-25)
// Motor runs only while button held - uses PRESSED/RELEASED events

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* jogCwBtn = nullptr;
static lv_obj_t* jogCcwBtn = nullptr;
static lv_obj_t* speedSlider = nullptr;
static lv_obj_t* speedLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// JOG EVENT HANDLER - Handles PRESSED, RELEASED, PRESS_LOST
// ───────────────────────────────────────────────────────────────────────────────
static void jog_event_cb(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);

  // Determine direction from user data
  bool isCw = (lv_obj_get_user_data(btn) == (void*)1);

  if (code == LV_EVENT_PRESSED) {
    // Button pressed - start jog
    if (isCw) {
      control_start_jog_cw();
    } else {
      control_start_jog_ccw();
    }
    // Visual feedback
    lv_obj_set_style_bg_color(btn, COL_AMBER, 0);
  }
  else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    // Button released or finger slid off - stop jog immediately
    control_stop_jog();
    // Reset visual
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SPEED SLIDER EVENT
// ───────────────────────────────────────────────────────────────────────────────
static void speed_slider_event_cb(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  float rpm = 0.1f + (value / 100.0f) * 1.9f;  // 0.1 - 2.0 RPM
  control_set_jog_speed(rpm);
  lv_label_set_text_fmt(speedLabel, "%.1f RPM", rpm);
}

// ───────────────────────────────────────────────────────────────────────────────
// BACK EVENT
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  control_stop_jog();
  screens_show(SCREEN_MENU);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];

  // Header
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
  lv_label_set_text(title, "◎ JOG MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_AMBER, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // Jog speed slider
  lv_obj_t* speedTitle = lv_label_create(screen);
  lv_label_set_text(speedTitle, "JOG SPEED");
  lv_obj_set_style_text_color(speedTitle, COL_TEXT_DIM, 0);
  lv_obj_align(speedTitle, LV_ALIGN_TOP_MID, 0, 44);

  speedSlider = lv_slider_create(screen);
  lv_obj_set_size(speedSlider, 300, 20);
  lv_obj_align(speedSlider, LV_ALIGN_TOP_MID, 0, 66);
  lv_slider_set_range(speedSlider, 0, 100);
  lv_slider_set_value(speedSlider, 21, LV_ANIM_OFF);  // ~0.5 RPM
  lv_obj_add_event_cb(speedSlider, speed_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  speedLabel = lv_label_create(screen);
  lv_label_set_text(speedLabel, "0.5 RPM");
  lv_obj_align(speedLabel, LV_ALIGN_TOP_MID, 120, 66);

  // Hold to jog label
  lv_obj_t* holdLabel = lv_label_create(screen);
  lv_label_set_text(holdLabel, "HOLD BUTTON TO JOG — RELEASES WHEN FINGER LIFTS");
  lv_obj_set_style_text_color(holdLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(holdLabel, 12, 106);

  // CCW Jog button (left)
  jogCcwBtn = lv_btn_create(screen);
  lv_obj_set_size(jogCcwBtn, 220, 130);
  lv_obj_set_pos(jogCcwBtn, 12, 134);
  lv_obj_set_style_bg_color(jogCcwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(jogCcwBtn, RADIUS_BTN, 0);
  lv_obj_set_user_data(jogCcwBtn, (void*)0);  // 0 = CCW
  lv_obj_add_event_cb(jogCcwBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  lv_obj_t* ccwLabel = lv_label_create(jogCcwBtn);
  lv_label_set_text(ccwLabel, "◄◄◄  JOG  CCW");
  lv_obj_set_style_text_font(ccwLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(ccwLabel);

  // CW Jog button (right)
  jogCwBtn = lv_btn_create(screen);
  lv_obj_set_size(jogCwBtn, 220, 130);
  lv_obj_set_pos(jogCwBtn, 248, 134);
  lv_obj_set_style_bg_color(jogCwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(jogCwBtn, RADIUS_BTN, 0);
  lv_obj_set_user_data(jogCwBtn, (void*)1);  // 1 = CW
  lv_obj_add_event_cb(jogCwBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  lv_obj_t* cwLabel = lv_label_create(jogCwBtn);
  lv_label_set_text(cwLabel, "JOG  CW  ►►►");
  lv_obj_set_style_text_font(cwLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(cwLabel);

  // Back to menu button
  lv_obj_t* menuBtn = lv_btn_create(screen);
  lv_obj_set_size(menuBtn, 456, 44);
  lv_obj_set_pos(menuBtn, 12, 270);
  lv_obj_set_style_bg_color(menuBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(menuBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(menuBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* menuLabel = lv_label_create(menuBtn);
  lv_label_set_text(menuLabel, "◄ BACK TO MENU");
  lv_obj_center(menuLabel);

  LOG_I("Screen jog: created");
}
