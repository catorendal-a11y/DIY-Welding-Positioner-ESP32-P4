// TIG Rotator Controller - Jog Mode Screen
// Matching ui_all_screens.svg: Single large JOG button + CW/CCW selectors

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* jogBtn = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  control_stop_jog();
  screens_show(SCREEN_MENU);
}

static void jog_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    if (speed_get_direction() == DIR_CW) {
      control_start_jog_cw();
    } else {
      control_start_jog_ccw();
    }
    lv_obj_set_style_bg_color(jogBtn, COL_AMBER, 0);
    lv_obj_set_style_bg_opa(jogBtn, LV_OPA_30, 0);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    lv_obj_set_style_bg_color(jogBtn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_bg_opa(jogBtn, LV_OPA_COVER, 0);
  }
}

static void cw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CW);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
  lv_obj_set_style_border_width(cwBtn, 2, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER_SPD, 0);
  lv_obj_set_style_border_width(ccwBtn, 1, 0);
}

static void ccw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CCW);
  lv_obj_set_style_border_color(ccwBtn, COL_ACCENT, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_border_color(cwBtn, COL_BORDER_SPD, 0);
  lv_obj_set_style_border_width(cwBtn, 1, 0);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Large hold button + CW/CCW direction toggle
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Back button (top-left) ──
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 80, 40);
  lv_obj_set_pos(backBtn, 16, 12);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "JOG MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_AMBER, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // Instruction
  lv_obj_t* instrLabel = lv_label_create(screen);
  lv_label_set_text(instrLabel, "HOLD TO ROTATE");
  lv_obj_set_style_text_font(instrLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(instrLabel, COL_AMBER, 0);
  lv_obj_align(instrLabel, LV_ALIGN_TOP_MID, 0, 42);

  // ── Large JOG hold button (SVG: orange border, transparent fill) ──
  jogBtn = lv_btn_create(screen);
  lv_obj_set_size(jogBtn, 600, 200);
  lv_obj_align(jogBtn, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_bg_color(jogBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(jogBtn, 16, 0);
  lv_obj_set_style_border_width(jogBtn, 3, 0);
  lv_obj_set_style_border_color(jogBtn, COL_AMBER, 0);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  lv_obj_t* jogSymbol = lv_label_create(jogBtn);
  lv_label_set_text(jogSymbol, LV_SYMBOL_LEFT " JOG " LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(jogSymbol, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(jogSymbol, COL_AMBER, 0);
  lv_obj_align(jogSymbol, LV_ALIGN_CENTER, 0, -15);

  lv_obj_t* jogHint = lv_label_create(jogBtn);
  lv_label_set_text(jogHint, "Touch & hold to run");
  lv_obj_set_style_text_font(jogHint, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(jogHint, COL_TEXT_DIM, 0);
  lv_obj_align(jogHint, LV_ALIGN_CENTER, 0, 25);

  // ── CW / CCW direction buttons (bottom) ──
  cwBtn = lv_btn_create(screen);
  lv_obj_set_size(cwBtn, 180, 65);
  lv_obj_set_pos(cwBtn, 100, 375);
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(cwBtn, 8, 0);
  lv_obj_set_style_border_width(cwBtn, 2, 0);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cwLabel = lv_label_create(cwBtn);
  lv_label_set_text(cwLabel, "CW " LV_SYMBOL_UP);
  lv_obj_set_style_text_font(cwLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(cwLabel, COL_ACCENT, 0);
  lv_obj_center(cwLabel);

  ccwBtn = lv_btn_create(screen);
  lv_obj_set_size(ccwBtn, 180, 65);
  lv_obj_set_pos(ccwBtn, 520, 375);
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(ccwBtn, 8, 0);
  lv_obj_set_style_border_width(ccwBtn, 1, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER_SPD, 0);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* ccwLabel = lv_label_create(ccwBtn);
  lv_label_set_text(ccwLabel, "CCW " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_font(ccwLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(ccwLabel, COL_TEXT_DIM, 0);
  lv_obj_center(ccwLabel);

  LOG_I("Screen jog: SVG-matched layout created");
}
