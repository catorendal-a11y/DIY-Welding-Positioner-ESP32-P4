// TIG Rotator Controller - Jog Mode Screen
// Matching tig-rotator-ui.svg: RPM panel, direction indicator, large JOG button

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
static lv_obj_t* rpmLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  control_stop_jog();
  screens_show(SCREEN_MENU);
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  float currentRpm = control_get_jog_speed();
  if (delta > 0) currentRpm += 0.1f;
  else if (currentRpm > 0.1f) currentRpm -= 0.1f;
  if (currentRpm < MIN_RPM) currentRpm = MIN_RPM;
  if (currentRpm > MAX_RPM) currentRpm = MAX_RPM;
  control_set_jog_speed(currentRpm);
  lv_label_set_text_fmt(rpmLabel, "%.1f", currentRpm);
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
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
  lv_obj_set_style_border_width(ccwBtn, 1, 0);

  // Update direction label
  lv_obj_t* dirLabel = lv_obj_get_child(ccwBtn, 0);
  lv_label_set_text(dirLabel, "CCW " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_color(dirLabel, COL_TEXT_DIM, 0);
}

static void ccw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CCW);
  lv_obj_set_style_border_color(ccwBtn, COL_ACCENT, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_border_color(cwBtn, COL_BORDER, 0);
  lv_obj_set_style_border_width(cwBtn, 1, 0);

  // Update direction label
  lv_obj_t* dirLabel = lv_obj_get_child(cwBtn, 0);
  lv_label_set_text(dirLabel, "CW " LV_SYMBOL_UP);
  lv_obj_set_style_text_color(dirLabel, COL_TEXT_DIM, 0);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Screen 5, scaled from 360x250 to 800x480
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // SVG 800x480: exact pixel values from new SVG

  // ── Header bar (SVG: h=61.4) ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  // ── [ESC] button at right of header (removed - BACK is at bottom) ──

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "JOG MODE");
  lv_obj_set_style_text_font(title, FONT_MED, 0);
  lv_obj_set_style_text_color(title, COL_AMBER, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // ── RPM panel (SVG: x=35.6, y=76.8, w=355.6, h=107.5) ──
  lv_obj_t* rpmPanel = lv_obj_create(screen);
  lv_obj_set_size(rpmPanel, 356, 108);
  lv_obj_set_pos(rpmPanel, 36, 77);
  lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(rpmPanel, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmPanel, 2, 0);
  lv_obj_set_style_border_color(rpmPanel, COL_ACCENT, 0);
  lv_obj_set_style_pad_all(rpmPanel, 0, 0);
  lv_obj_remove_flag(rpmPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rpmTitle = lv_label_create(rpmPanel);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_ACCENT, 0);
  lv_obj_align(rpmTitle, LV_ALIGN_TOP_MID, 0, 8);

  rpmLabel = lv_label_create(rpmPanel);
  float jogRpm = control_get_jog_speed();
  lv_label_set_text_fmt(rpmLabel, "%.1f", jogRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_BOTTOM_MID, 0, -8);

  // ── Direction indicator (SVG: x=408.9, y=76.8, w=355.6, h=107.5) ──
  lv_obj_t* dirPanel = lv_obj_create(screen);
  lv_obj_set_size(dirPanel, 356, 108);
  lv_obj_set_pos(dirPanel, 409, 77);
  lv_obj_set_style_bg_color(dirPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(dirPanel, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(dirPanel, 2, 0);
  lv_obj_set_style_border_color(dirPanel, COL_BORDER, 0);
  lv_obj_set_style_pad_all(dirPanel, 0, 0);
  lv_obj_remove_flag(dirPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* dirTitle = lv_label_create(dirPanel);
  lv_label_set_text(dirTitle, "DIRECTION");
  lv_obj_set_style_text_font(dirTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirTitle, COL_TEXT_DIM, 0);
  lv_obj_align(dirTitle, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t* dirLabel = lv_label_create(dirPanel);
  lv_label_set_text(dirLabel, "CW " LV_SYMBOL_LOOP);
  lv_obj_set_style_text_font(dirLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(dirLabel, COL_TEXT, 0);
  lv_obj_align(dirLabel, LV_ALIGN_BOTTOM_MID, 0, -8);

  // ── Large JOG button (SVG: x=35.6, y=199.7, w=728.9, h=107.5) ──
  jogBtn = lv_button_create(screen);
  lv_obj_set_size(jogBtn, 729, 108);
  lv_obj_set_pos(jogBtn, 36, 200);
  lv_obj_set_style_bg_color(jogBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(jogBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(jogBtn, 3, 0);
  lv_obj_set_style_border_color(jogBtn, COL_AMBER, 0);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* jogSymbol = lv_label_create(jogBtn);
  lv_label_set_text(jogSymbol, LV_SYMBOL_LEFT " JOG " LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(jogSymbol, FONT_LARGE, 0);
  lv_obj_set_style_text_color(jogSymbol, COL_AMBER, 0);
  lv_obj_align(jogSymbol, LV_ALIGN_CENTER, 0, -8);

  lv_obj_t* jogHint = lv_label_create(jogBtn);
  lv_label_set_text(jogHint, "Touch & hold to rotate");
  lv_obj_set_style_text_font(jogHint, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(jogHint, COL_TEXT_DIM, 0);
  lv_obj_align(jogHint, LV_ALIGN_CENTER, 0, 20);

  // ── Separator (SVG: y=322.6, h=1.9) ──
  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SCREEN_W, 2);
  lv_obj_set_pos(sep, 0, 323);
  lv_obj_set_style_bg_color(sep, COL_PANEL_BG, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  // ── RPM adjustment buttons (SVG: y=337.9, x=35.6/222.2, w=168.9, gap=17.7) ──
  const int rpmAdjY = 338;
  const int rpmAdjW = 169;  // SVG: 168.9
  const int rpmAdjH = 61;   // SVG: 61.4
  const int rpmAdjGap = 18; // SVG: 222.2-35.6-168.9=17.7
  const int rpmAdjStartX = 36;  // SVG: 35.6

  lv_obj_t* rpmMinusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmMinusBtn, rpmAdjW, rpmAdjH);
  lv_obj_set_pos(rpmMinusBtn, rpmAdjStartX, rpmAdjY);
  lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmMinusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmMinusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

  lv_obj_t* rpmMinusLabel = lv_label_create(rpmMinusBtn);
  lv_label_set_text(rpmMinusLabel, "RPM " LV_SYMBOL_MINUS);
  lv_obj_set_style_text_font(rpmMinusLabel, FONT_MED, 0);
  lv_obj_set_style_text_color(rpmMinusLabel, COL_TEXT, 0);
  lv_obj_center(rpmMinusLabel);

  lv_obj_t* rpmPlusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmPlusBtn, rpmAdjW, rpmAdjH);
  lv_obj_set_pos(rpmPlusBtn, rpmAdjStartX + rpmAdjW + rpmAdjGap, rpmAdjY);
  lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmPlusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmPlusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

  lv_obj_t* rpmPlusLabel = lv_label_create(rpmPlusBtn);
  lv_label_set_text(rpmPlusLabel, "RPM " LV_SYMBOL_PLUS);
  lv_obj_set_style_text_font(rpmPlusLabel, FONT_MED, 0);
  lv_obj_set_style_text_color(rpmPlusLabel, COL_TEXT, 0);
  lv_obj_center(rpmPlusLabel);

  // ── Direction buttons (SVG: y=337.9, x=408.9/595.6, w=168.9, gap=17.8) ──
  cwBtn = lv_button_create(screen);
  lv_obj_set_size(cwBtn, rpmAdjW, rpmAdjH);
  lv_obj_set_pos(cwBtn, rpmAdjStartX + (rpmAdjW + rpmAdjGap) * 2, rpmAdjY);
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(cwBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(cwBtn, 2, 0);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cwLabel = lv_label_create(cwBtn);
  lv_label_set_text(cwLabel, "CW " LV_SYMBOL_UP);
  lv_obj_set_style_text_font(cwLabel, FONT_MED, 0);
  lv_obj_set_style_text_color(cwLabel, COL_ACCENT, 0);
  lv_obj_center(cwLabel);

  ccwBtn = lv_button_create(screen);
  lv_obj_set_size(ccwBtn, rpmAdjW, rpmAdjH);
  lv_obj_set_pos(ccwBtn, rpmAdjStartX + (rpmAdjW + rpmAdjGap) * 3, rpmAdjY);
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(ccwBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(ccwBtn, 1, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* ccwLabel = lv_label_create(ccwBtn);
  lv_label_set_text(ccwLabel, "CCW " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_font(ccwLabel, FONT_MED, 0);
  lv_obj_set_style_text_color(ccwLabel, COL_TEXT_DIM, 0);
  lv_obj_center(ccwLabel);

  // ── Bottom bar: BACK button (matching menu/settings/programs position) ──
  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, 376, 48);
  lv_obj_set_pos(backBtn, PAD_X, 428);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_set_style_pad_all(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  LOG_I("Screen jog: SVG-matched layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_update() {
  if (!screens_is_active(SCREEN_JOG)) return;

  SystemState state = control_get_state();
  if (state == STATE_JOG) {
    float actual_rpm = speed_get_actual_rpm();
    lv_label_set_text_fmt(rpmLabel, "%.1f", actual_rpm);
  } else {
    float jogRpm = control_get_jog_speed();
    lv_label_set_text_fmt(rpmLabel, "%.1f", jogRpm);
  }
}
