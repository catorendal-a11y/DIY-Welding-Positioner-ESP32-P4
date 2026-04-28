// Jog Mode Screen - POST-style hold-to-run control

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static lv_obj_t* cwHoldBtn = nullptr;
static lv_obj_t* ccwHoldBtn = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmBar = nullptr;
static lv_obj_t* jogHdrRight = nullptr;

static void back_event_cb(lv_event_t* e) {
  control_stop_jog();
  screens_show(SCREEN_MAIN);
}

static void stop_event_cb(lv_event_t* e) {
  control_stop_jog();
  control_stop();
}

static int rpm_to_pct(float rpm) {
  float mx = speed_get_rpm_max();
  float span = mx - MIN_RPM;
  if (span < 1e-6f) span = 1e-6f;
  int pct = (int)((rpm - MIN_RPM) * 100.0f / span + 0.5f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

static void set_jog_rpm(float rpm) {
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);
  if (rpmBar) lv_bar_set_value(rpmBar, rpm_to_pct(rpm), LV_ANIM_OFF);
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)lv_event_get_user_data(e);
  float currentRpm = control_get_jog_speed();
  if (delta > 0) currentRpm += 0.1f;
  else if (currentRpm > 0.1f) currentRpm -= 0.1f;
  float mx = speed_get_rpm_max();
  if (currentRpm < MIN_RPM) currentRpm = MIN_RPM;
  if (currentRpm > mx) currentRpm = mx;
  control_set_jog_speed(currentRpm);
  set_jog_rpm(currentRpm);
}

static void style_hold_btn(lv_obj_t* btn, bool active) {
  if (!btn) return;
  ui_btn_style_post(btn, active ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  lv_obj_set_style_radius(btn, RADIUS_CARD, 0);
  lv_obj_t* title = lv_obj_get_child(btn, 0);
  if (title) lv_obj_set_style_text_color(title, ui_btn_label_color_post(active ? UI_BTN_ACCENT : UI_BTN_NORMAL), 0);
}

static void cw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    speed_set_direction(DIR_CW);
    control_start_jog_cw();
    style_hold_btn(cwHoldBtn, true);
    style_hold_btn(ccwHoldBtn, false);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    style_hold_btn(cwHoldBtn, true);
  }
}

static void ccw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    speed_set_direction(DIR_CCW);
    control_start_jog_ccw();
    style_hold_btn(cwHoldBtn, false);
    style_hold_btn(ccwHoldBtn, true);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    style_hold_btn(cwHoldBtn, true);
    style_hold_btn(ccwHoldBtn, false);
  }
}

static lv_obj_t* create_hold_btn(lv_obj_t* parent, int x, int y, int w, int h,
                                 const char* title, bool active, lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  ui_btn_style_post(btn, active ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  lv_obj_set_style_radius(btn, RADIUS_CARD, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* titleLbl = lv_label_create(btn);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(titleLbl, ui_btn_label_color_post(active ? UI_BTN_ACCENT : UI_BTN_NORMAL), 0);
  lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -18);

  lv_obj_t* hintLbl = lv_label_create(btn);
  lv_label_set_text(hintLbl, "Hold to run");
  lv_obj_set_style_text_font(hintLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(hintLbl, COL_TEXT_VDIM, 0);
  lv_obj_align(hintLbl, LV_ALIGN_CENTER, 0, 28);
  return btn;
}

void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  jogHdrRight = nullptr;
  ui_create_header(screen, "JOG MODE", "IDLE", &jogHdrRight);
  if (jogHdrRight) lv_obj_set_style_text_color(jogHdrRight, COL_GREEN, 0);

  lv_obj_t* speedCard = lv_obj_create(screen);
  lv_obj_set_size(speedCard, 760, 74);
  lv_obj_set_pos(speedCard, 20, HEADER_H + 14);
  lv_obj_set_style_bg_color(speedCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(speedCard, COL_BORDER, 0);
  lv_obj_set_style_border_width(speedCard, 1, 0);
  lv_obj_set_style_radius(speedCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(speedCard, 0, 0);
  lv_obj_remove_flag(speedCard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rpmTitle = lv_label_create(speedCard);
  lv_label_set_text(rpmTitle, "JOG SPEED");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, 22, 12);

  rpmLabel = lv_label_create(speedCard);
  lv_obj_set_style_text_font(rpmLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_set_pos(rpmLabel, 150, 18);

  lv_obj_t* unit = lv_label_create(speedCard);
  lv_label_set_text(unit, "RPM");
  lv_obj_set_style_text_font(unit, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(unit, COL_TEXT_DIM, 0);
  lv_obj_set_pos(unit, 232, 32);

  rpmBar = lv_bar_create(speedCard);
  lv_obj_set_size(rpmBar, 320, 6);
  lv_obj_set_pos(rpmBar, 310, 38);
  lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_radius(rpmBar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(rpmBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(rpmBar, 0, LV_PART_INDICATOR);
  lv_bar_set_range(rpmBar, 0, 100);
  set_jog_rpm(control_get_jog_speed());

  ui_create_btn(speedCard, 530, 11, 82, 40, "-", FONT_NORMAL, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)-1);
  ui_create_btn(speedCard, 628, 11, 82, 40, "+", FONT_NORMAL, UI_BTN_ACCENT, rpm_adj_cb, (void*)(intptr_t)1);

  const int holdY = HEADER_H + 110;
  const int holdH = 92;
  const int holdGap = 16;
  const int holdW = (SCREEN_W - PAD_X * 2 - holdGap) / 2;
  cwHoldBtn = create_hold_btn(screen, PAD_X, holdY, holdW, holdH, "HOLD CW", true, cw_hold_event_cb);
  ccwHoldBtn = create_hold_btn(screen, PAD_X + holdW + holdGap, holdY, holdW, holdH, "HOLD CCW", false,
                               ccw_hold_event_cb);

  lv_obj_t* liveCard = lv_obj_create(screen);
  lv_obj_set_size(liveCard, 760, 92);
  lv_obj_set_pos(liveCard, 20, holdY + holdH + 16);
  lv_obj_set_style_bg_color(liveCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(liveCard, COL_BORDER, 0);
  lv_obj_set_style_border_width(liveCard, 1, 0);
  lv_obj_set_style_radius(liveCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(liveCard, 0, 0);
  lv_obj_remove_flag(liveCard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* liveTitle = lv_label_create(liveCard);
  lv_label_set_text(liveTitle, "LIVE");
  lv_obj_set_style_text_font(liveTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(liveTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(liveTitle, 22, 16);

  lv_obj_t* liveVals = lv_label_create(liveCard);
  lv_label_set_text(liveVals, "DIR CW     ENA HIGH     ESTOP CLEAR");
  lv_obj_set_style_text_font(liveVals, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(liveVals, COL_TEXT, 0);
  lv_obj_set_pos(liveVals, 120, 36);

  ui_create_btn(screen, PAD_X, 414, 376, 50, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_event_cb, nullptr);
  ui_create_btn(screen, 408, 414, 376, 50, "X STOP", FONT_SUBTITLE, UI_BTN_DANGER, stop_event_cb, nullptr);

  LOG_I("Screen jog: POST-style layout created");
}

void screen_jog_invalidate_widgets() {
  cwHoldBtn = nullptr;
  ccwHoldBtn = nullptr;
  rpmLabel = nullptr;
  rpmBar = nullptr;
  jogHdrRight = nullptr;
}

void screen_jog_update() {
  if (!screens_is_active(SCREEN_JOG)) return;

  SystemState state = control_get_state();
  if (jogHdrRight) {
    if (state == STATE_JOG) {
      lv_label_set_text(jogHdrRight, "RUN");
      lv_obj_set_style_text_color(jogHdrRight, COL_ACCENT, 0);
    } else {
      lv_label_set_text(jogHdrRight, "IDLE");
      lv_obj_set_style_text_color(jogHdrRight, COL_GREEN, 0);
    }
  }
  if (state == STATE_JOG) {
    float actualRpm = speed_get_actual_rpm();
    set_jog_rpm(actualRpm);
  } else {
    set_jog_rpm(control_get_jog_speed());
  }
}
