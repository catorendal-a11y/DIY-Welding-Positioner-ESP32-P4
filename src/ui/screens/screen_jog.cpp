// TIG Rotator Controller - Jog Mode Screen
// Brutalist v2.0 design — RPM row, two large CW/CCW hold buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* cwHoldBtn = nullptr;
static lv_obj_t* ccwHoldBtn = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmBar = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  control_stop_jog();
  screens_show(SCREEN_MAIN);
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  float currentRpm = control_get_jog_speed();
  if (delta > 0) currentRpm += 0.1f;
  else if (currentRpm > 0.1f) currentRpm -= 0.1f;
  float mx = speed_get_rpm_max();
  if (currentRpm < MIN_RPM) currentRpm = MIN_RPM;
  if (currentRpm > mx) currentRpm = mx;
  control_set_jog_speed(currentRpm);
  lv_label_set_text_fmt(rpmLabel, "%.1f", currentRpm);
  if (rpmBar) {
    float span = mx - MIN_RPM;
    if (span < 1e-6f) span = 1e-6f;
    int pct = (int)((currentRpm - MIN_RPM) * 100.0f / span + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(rpmBar, pct, LV_ANIM_OFF);
  }
}

static void cw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    speed_set_direction(DIR_CW);
    control_start_jog_cw();
    lv_obj_set_style_bg_color(cwHoldBtn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(cwHoldBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(cwHoldBtn, 3, 0);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    lv_obj_set_style_bg_color(cwHoldBtn, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(cwHoldBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(cwHoldBtn, 1, 0);
  }
}

static void ccw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    speed_set_direction(DIR_CCW);
    control_start_jog_ccw();
    lv_obj_set_style_bg_color(ccwHoldBtn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(ccwHoldBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(ccwHoldBtn, 3, 0);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    lv_obj_set_style_bg_color(ccwHoldBtn, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(ccwHoldBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(ccwHoldBtn, 1, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  lv_obj_t* header = ui_create_header(screen, "JOG MODE");
  lv_obj_remove_flag(header, LV_OBJ_FLAG_CLICKABLE);

  // ── RPM row (theme JOG_RPM_*) ──
  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, JOG_RPM_TITLE_X, JOG_RPM_ROW_Y);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", control_get_jog_speed());
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_set_pos(rpmLabel, JOG_RPM_VAL_X, JOG_RPM_ROW_Y);

  rpmBar = lv_bar_create(screen);
  lv_obj_set_size(rpmBar, JOG_RPM_BAR_W, JOG_RPM_BAR_H);
  lv_obj_set_pos(rpmBar, JOG_RPM_BAR_X, JOG_RPM_ROW_Y + JOG_RPM_BAR_Y_OFF);
  lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_color(rpmBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_range(rpmBar, 0, 100);
  {
    float mx = speed_get_rpm_max();
    float span = mx - MIN_RPM;
    if (span < 1e-6f) span = 1e-6f;
    int rpmPct = (int)((control_get_jog_speed() - MIN_RPM) * 100.0f / span + 0.5f);
    if (rpmPct < 0) rpmPct = 0;
    if (rpmPct > 100) rpmPct = 100;
    lv_bar_set_value(rpmBar, rpmPct, LV_ANIM_OFF);
  }

  ui_create_btn(screen, JOG_RPM_BTN_MINUS_X, JOG_RPM_BTN_Y, JOG_RPM_BTN_W, JOG_RPM_BTN_H, "-",
                 FONT_BTN, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)-1);
  ui_create_btn(screen, JOG_RPM_BTN_PLUS_X, JOG_RPM_BTN_Y, JOG_RPM_BTN_W, JOG_RPM_BTN_H, "+",
                 FONT_BTN, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)1);

  // ── Direction label (y=120) ──
  lv_obj_t* dirHint = lv_label_create(screen);
  lv_label_set_text(dirHint, "DIRECTION");
  lv_obj_set_style_text_font(dirHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirHint, COL_TEXT_DIM, 0);
  lv_obj_set_pos(dirHint, JOG_RPM_TITLE_X, 120);

  lv_obj_t* dirVal = lv_label_create(screen);
  lv_label_set_text(dirVal, "Hold button to rotate");
  lv_obj_set_style_text_font(dirVal, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirVal, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(dirVal, JOG_RPM_VAL_X, 120);

  // ── Two large CW / CCW hold buttons ──
  const int holdY = 160;
  const int holdH = 220;
  const int holdGap = 16;
  const int holdW = (SCREEN_W - PAD_X * 2 - holdGap) / 2;

  // CW button (left)
  cwHoldBtn = lv_button_create(screen);
  lv_obj_set_size(cwHoldBtn, holdW, holdH);
  lv_obj_set_pos(cwHoldBtn, PAD_X, holdY);
  lv_obj_set_style_bg_color(cwHoldBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(cwHoldBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(cwHoldBtn, 1, 0);
  lv_obj_set_style_border_color(cwHoldBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(cwHoldBtn, 0, 0);
  lv_obj_set_style_pad_all(cwHoldBtn, 0, 0);
  lv_obj_add_event_cb(cwHoldBtn, cw_hold_event_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(cwHoldBtn, cw_hold_event_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(cwHoldBtn, cw_hold_event_cb, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* cwArrow = lv_label_create(cwHoldBtn);
  lv_label_set_text(cwArrow, "<  CW");
  lv_obj_set_style_text_font(cwArrow, FONT_XXL, 0);
  lv_obj_set_style_text_color(cwArrow, COL_TEXT_DIM, 0);
  lv_obj_align(cwArrow, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t* cwHint = lv_label_create(cwHoldBtn);
  lv_label_set_text(cwHint, "Hold to run");
  lv_obj_set_style_text_font(cwHint, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(cwHint, COL_TEXT_VDIM, 0);
  lv_obj_align(cwHint, LV_ALIGN_CENTER, 0, 30);

  // CCW button (right)
  ccwHoldBtn = lv_button_create(screen);
  lv_obj_set_size(ccwHoldBtn, holdW, holdH);
  lv_obj_set_pos(ccwHoldBtn, PAD_X + holdW + holdGap, holdY);
  lv_obj_set_style_bg_color(ccwHoldBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(ccwHoldBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(ccwHoldBtn, 1, 0);
  lv_obj_set_style_border_color(ccwHoldBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(ccwHoldBtn, 0, 0);
  lv_obj_set_style_pad_all(ccwHoldBtn, 0, 0);
  lv_obj_add_event_cb(ccwHoldBtn, ccw_hold_event_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(ccwHoldBtn, ccw_hold_event_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(ccwHoldBtn, ccw_hold_event_cb, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* ccwArrow = lv_label_create(ccwHoldBtn);
  lv_label_set_text(ccwArrow, "CCW  >");
  lv_obj_set_style_text_font(ccwArrow, FONT_XXL, 0);
  lv_obj_set_style_text_color(ccwArrow, COL_TEXT_DIM, 0);
  lv_obj_align(ccwArrow, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t* ccwHint = lv_label_create(ccwHoldBtn);
  lv_label_set_text(ccwHint, "Hold to run");
  lv_obj_set_style_text_font(ccwHint, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(ccwHint, COL_TEXT_VDIM, 0);
  lv_obj_align(ccwHint, LV_ALIGN_CENTER, 0, 30);

  // ── Bottom bar: BACK button (same position as other screens) ──
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
  lv_obj_set_style_text_font(backLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  LOG_I("Screen jog: v2.0 layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_jog_invalidate_widgets() {
  cwHoldBtn = nullptr;
  ccwHoldBtn = nullptr;
  rpmLabel = nullptr;
  rpmBar = nullptr;
}

void screen_jog_update() {
  if (!screens_is_active(SCREEN_JOG)) return;

  SystemState state = control_get_state();
  float mx = speed_get_rpm_max();
  float span = mx - MIN_RPM;
  if (span < 1e-6f) span = 1e-6f;
  if (state == STATE_JOG) {
    float actual_rpm = speed_get_actual_rpm();
    lv_label_set_text_fmt(rpmLabel, "%.1f", actual_rpm);
    if (rpmBar) {
      int pct = (int)((actual_rpm - MIN_RPM) * 100.0f / span + 0.5f);
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      lv_bar_set_value(rpmBar, pct, LV_ANIM_OFF);
    }
  } else {
    float jogRpm = control_get_jog_speed();
    lv_label_set_text_fmt(rpmLabel, "%.1f", jogRpm);
    if (rpmBar) {
      int pct = (int)((jogRpm - MIN_RPM) * 100.0f / span + 0.5f);
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      lv_bar_set_value(rpmBar, pct, LV_ANIM_OFF);
    }
  }
}
