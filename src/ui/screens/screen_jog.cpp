#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static lv_obj_t* jogBtn = nullptr;
static lv_obj_t* rpmValLabel = nullptr;
static lv_obj_t* dirValLabel = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;
static lv_obj_t* statusLabel = nullptr;

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
    lv_label_set_text(statusLabel, "\xE2\x97\x8F RUNNING");
    lv_obj_set_style_text_color(statusLabel, COL_GREEN, 0);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    lv_obj_set_style_bg_color(jogBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_bg_opa(jogBtn, LV_OPA_COVER, 0);
    lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
    lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
  }
}

static void rpm_minus_cb(lv_event_t* e) {
  float rpm = control_get_jog_speed();
  if (rpm > MIN_RPM) {
    rpm -= 0.1f;
    if (rpm < MIN_RPM) rpm = MIN_RPM;
    control_set_jog_speed(rpm);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", rpm);
    lv_label_set_text(rpmValLabel, buf);
  }
}

static void rpm_plus_cb(lv_event_t* e) {
  float rpm = control_get_jog_speed();
  if (rpm < MAX_RPM) {
    rpm += 0.1f;
    if (rpm > MAX_RPM) rpm = MAX_RPM;
    control_set_jog_speed(rpm);
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", rpm);
    lv_label_set_text(rpmValLabel, buf);
  }
}

static void cw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CW);
  lv_label_set_text(dirValLabel, "CW \xE2\x86\xBB");
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
  lv_obj_set_style_border_width(cwBtn, 3, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(cwBtn, 0), COL_ACCENT, 0);
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(ccwBtn, 0), COL_TEXT, 0);
}

static void ccw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CCW);
  lv_label_set_text(dirValLabel, "CCW \xE2\x86\xBA");
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_ACCENT, 0);
  lv_obj_set_style_border_width(ccwBtn, 3, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(ccwBtn, 0), COL_ACCENT, 0);
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_border_color(cwBtn, COL_BORDER, 0);
  lv_obj_set_style_border_width(cwBtn, 2, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(cwBtn, 0), COL_TEXT, 0);
}

void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, SW(50), SH(22));
  lv_obj_set_pos(backBtn, SX(16), SY(21));
  lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_set_style_radius(backBtn, 0, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLbl = lv_label_create(backBtn);
  lv_label_set_text(backLbl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_font(backLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(backLbl, COL_TEXT_DIM, 0);
  lv_obj_center(backLbl);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "JOG MODE");
  lv_obj_set_style_text_font(title, FONT_BODY, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SY(21));

  statusLabel = lv_label_create(header);
  lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
  lv_obj_set_style_text_font(statusLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_RIGHT, -SX(16), SY(21));

  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep, 0, HEADER_H);
  lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);

  lv_obj_t* rpmPanel = lv_obj_create(screen);
  lv_obj_set_size(rpmPanel, SW(160), SH(56));
  lv_obj_set_pos(rpmPanel, SX(16), SY(40));
  lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_border_width(rpmPanel, 2, 0);
  lv_obj_set_style_border_color(rpmPanel, COL_ACCENT, 0);
  lv_obj_set_style_radius(rpmPanel, RADIUS_MD, 0);
  lv_obj_set_style_pad_all(rpmPanel, 0, 0);

  lv_obj_t* rpmTitle = lv_label_create(rpmPanel);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_ACCENT, 0);
  lv_obj_align(rpmTitle, LV_ALIGN_TOP_MID, 0, SH(6));

  float initRpm = control_get_jog_speed();
  rpmValLabel = lv_label_create(rpmPanel);
  char rpmBuf[16];
  snprintf(rpmBuf, sizeof(rpmBuf), "%.1f", initRpm);
  lv_label_set_text(rpmValLabel, rpmBuf);
  lv_obj_set_style_text_font(rpmValLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmValLabel, COL_ACCENT, 0);
  lv_obj_align(rpmValLabel, LV_ALIGN_CENTER, 0, SH(6));

  lv_obj_t* dirPanel = lv_obj_create(screen);
  lv_obj_set_size(dirPanel, SW(160), SH(56));
  lv_obj_set_pos(dirPanel, SX(184), SY(40));
  lv_obj_set_style_bg_color(dirPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_border_width(dirPanel, 2, 0);
  lv_obj_set_style_border_color(dirPanel, COL_BORDER, 0);
  lv_obj_set_style_radius(dirPanel, RADIUS_MD, 0);
  lv_obj_set_style_pad_all(dirPanel, 0, 0);

  lv_obj_t* dirTitle = lv_label_create(dirPanel);
  lv_label_set_text(dirTitle, "DIRECTION");
  lv_obj_set_style_text_font(dirTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirTitle, COL_TEXT_DIM, 0);
  lv_obj_align(dirTitle, LV_ALIGN_TOP_MID, 0, SH(6));

  dirValLabel = lv_label_create(dirPanel);
  lv_label_set_text(dirValLabel, "CW \xE2\x86\xBB");
  lv_obj_set_style_text_font(dirValLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(dirValLabel, COL_TEXT, 0);
  lv_obj_align(dirValLabel, LV_ALIGN_CENTER, 0, SH(6));

  jogBtn = lv_btn_create(screen);
  lv_obj_set_size(jogBtn, SW(328), SH(56));
  lv_obj_set_pos(jogBtn, SX(16), SY(104));
  lv_obj_set_style_bg_color(jogBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(jogBtn, RADIUS_MD, 0);
  lv_obj_set_style_border_width(jogBtn, 2, 0);
  lv_obj_set_style_border_color(jogBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  lv_obj_t* jogLabel = lv_label_create(jogBtn);
  lv_label_set_text(jogLabel, LV_SYMBOL_LEFT " JOG " LV_SYMBOL_RIGHT);
  lv_obj_set_style_text_font(jogLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(jogLabel, COL_TEXT, 0);
  lv_obj_align(jogLabel, LV_ALIGN_CENTER, 0, -SH(6));

  lv_obj_t* jogHint = lv_label_create(jogBtn);
  lv_label_set_text(jogHint, "Touch & hold to rotate");
  lv_obj_set_style_text_font(jogHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(jogHint, COL_TEXT_DIM, 0);
  lv_obj_align(jogHint, LV_ALIGN_CENTER, 0, SH(10));

  lv_obj_t* sep2 = lv_obj_create(screen);
  lv_obj_set_size(sep2, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep2, 0, SY(168));
  lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep2, 0, 0);
  lv_obj_set_style_pad_all(sep2, 0, 0);
  lv_obj_set_style_radius(sep2, 0, 0);

  lv_obj_t* rpmMinusBtn = lv_btn_create(screen);
  lv_obj_set_size(rpmMinusBtn, SW(76), SH(32));
  lv_obj_set_pos(rpmMinusBtn, SX(16), SY(176));
  lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(rpmMinusBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(rpmMinusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_minus_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rpmMinusLbl = lv_label_create(rpmMinusBtn);
  lv_label_set_text(rpmMinusLbl, "RPM " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_color(rpmMinusLbl, COL_TEXT, 0);
  lv_obj_center(rpmMinusLbl);

  lv_obj_t* rpmPlusBtn = lv_btn_create(screen);
  lv_obj_set_size(rpmPlusBtn, SW(76), SH(32));
  lv_obj_set_pos(rpmPlusBtn, SX(100), SY(176));
  lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(rpmPlusBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(rpmPlusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_plus_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rpmPlusLbl = lv_label_create(rpmPlusBtn);
  lv_label_set_text(rpmPlusLbl, "RPM " LV_SYMBOL_UP);
  lv_obj_set_style_text_color(rpmPlusLbl, COL_TEXT, 0);
  lv_obj_center(rpmPlusLbl);

  cwBtn = lv_btn_create(screen);
  lv_obj_set_size(cwBtn, SW(76), SH(32));
  lv_obj_set_pos(cwBtn, SX(184), SY(176));
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(cwBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(cwBtn, 3, 0);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* cwLbl = lv_label_create(cwBtn);
  lv_label_set_text(cwLbl, "CW " LV_SYMBOL_UP);
  lv_obj_set_style_text_color(cwLbl, COL_ACCENT, 0);
  lv_obj_center(cwLbl);

  ccwBtn = lv_btn_create(screen);
  lv_obj_set_size(ccwBtn, SW(76), SH(32));
  lv_obj_set_pos(ccwBtn, SX(268), SY(176));
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(ccwBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* ccwLbl = lv_label_create(ccwBtn);
  lv_label_set_text(ccwLbl, "CCW " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_color(ccwLbl, COL_TEXT, 0);
  lv_obj_center(ccwLbl);

  LOG_I("Screen jog: SVG-matched layout created");
}

void screen_jog_update() {
  if (!screens_is_active(SCREEN_JOG)) return;

  SystemState state = control_get_state();
  if (state != STATE_JOG && state != STATE_IDLE) {
    lv_obj_set_style_bg_color(jogBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_bg_opa(jogBtn, LV_OPA_COVER, 0);
    if (statusLabel) {
      lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
      lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
    }
  }
}
