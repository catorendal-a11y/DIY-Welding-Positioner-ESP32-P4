#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static float pulseOnSec = 0.5f;
static float pulseOffSec = 0.5f;
static lv_obj_t* onLabel = nullptr;
static lv_obj_t* offLabel = nullptr;
static lv_obj_t* rpmGaugeLabel = nullptr;
static lv_obj_t* startBtn = nullptr;

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void on_minus_cb(lv_event_t* e) {
  if (pulseOnSec > 0.1f) pulseOnSec -= 0.1f;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fs", pulseOnSec);
  lv_label_set_text(onLabel, buf);
}

static void on_plus_cb(lv_event_t* e) {
  if (pulseOnSec < 5.0f) pulseOnSec += 0.1f;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fs", pulseOnSec);
  lv_label_set_text(onLabel, buf);
}

static void off_minus_cb(lv_event_t* e) {
  if (pulseOffSec > 0.1f) pulseOffSec -= 0.1f;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fs", pulseOffSec);
  lv_label_set_text(offLabel, buf);
}

static void off_plus_cb(lv_event_t* e) {
  if (pulseOffSec < 5.0f) pulseOffSec += 0.1f;
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fs", pulseOffSec);
  lv_label_set_text(offLabel, buf);
}

static void rpm_minus_cb(lv_event_t* e) {
  float rpm = speed_get_target_rpm();
  if (rpm > MIN_RPM) {
    speed_slider_set(rpm - 0.1f);
  }
}

static void rpm_plus_cb(lv_event_t* e) {
  float rpm = speed_get_target_rpm();
  if (rpm < MAX_RPM) {
    speed_slider_set(rpm + 0.1f);
  }
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE) {
    uint32_t onMs = (uint32_t)(pulseOnSec * 1000);
    uint32_t offMs = (uint32_t)(pulseOffSec * 1000);
    control_start_pulse(onMs, offMs);
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "\xE2\x96\xA0 STOP");
    lv_obj_set_style_bg_color(startBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(startBtn, COL_RED, 0);
    lv_obj_set_style_text_color(label, COL_RED, 0);
  } else if (state == STATE_PULSE) {
    control_stop();
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "\xE2\x96\xB6 START");
    lv_obj_set_style_bg_color(startBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
  }
}

static void make_btn(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, lv_event_cb_t cb) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(btn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_center(lbl);
}

void screen_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PULSE];
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
  lv_obj_set_style_text_font(backLbl, FONT_NORMAL, 0);
  lv_obj_center(backLbl);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "PULSE MODE");
  lv_obj_set_style_text_font(title, FONT_BODY, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, SX(180), SY(21));
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t* activeLbl = lv_label_create(header);
  lv_label_set_text(activeLbl, LV_SYMBOL_BULLET " ACTIVE");
  lv_obj_set_style_text_font(activeLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(activeLbl, COL_ACCENT, 0);
  lv_obj_set_pos(activeLbl, SX(344), SY(21));
  lv_obj_set_style_text_align(activeLbl, LV_TEXT_ALIGN_RIGHT, 0);

  lv_obj_t* sep1 = lv_obj_create(screen);
  lv_obj_set_size(sep1, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep1, 0, HEADER_H);
  lv_obj_set_style_bg_color(sep1, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep1, 0, 0);
  lv_obj_set_style_pad_all(sep1, 0, 0);
  lv_obj_set_style_radius(sep1, 0, 0);

  lv_obj_t* onPanel = lv_obj_create(screen);
  lv_obj_set_size(onPanel, SW(160), SH(50));
  lv_obj_set_pos(onPanel, SX(16), SY(40));
  lv_obj_set_style_bg_color(onPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(onPanel, RADIUS_SM, 0);
  lv_obj_set_style_border_width(onPanel, 2, 0);
  lv_obj_set_style_border_color(onPanel, COL_ACCENT, 0);

  lv_obj_t* onTitle = lv_label_create(onPanel);
  lv_label_set_text(onTitle, "ON TIME");
  lv_obj_set_style_text_font(onTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(onTitle, COL_ACCENT, 0);
  lv_obj_set_style_text_letter_space(onTitle, 1, 0);
  lv_obj_align(onTitle, LV_ALIGN_TOP_MID, 0, 8);

  onLabel = lv_label_create(onPanel);
  lv_label_set_text(onLabel, "0.5s");
  lv_obj_set_style_text_font(onLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(onLabel, COL_ACCENT, 0);
  lv_obj_align(onLabel, LV_ALIGN_CENTER, 0, 10);

  lv_obj_t* offPanel = lv_obj_create(screen);
  lv_obj_set_size(offPanel, SW(160), SH(50));
  lv_obj_set_pos(offPanel, SX(184), SY(40));
  lv_obj_set_style_bg_color(offPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(offPanel, RADIUS_SM, 0);
  lv_obj_set_style_border_width(offPanel, 2, 0);
  lv_obj_set_style_border_color(offPanel, COL_BORDER, 0);

  lv_obj_t* offTitle = lv_label_create(offPanel);
  lv_label_set_text(offTitle, "OFF TIME");
  lv_obj_set_style_text_font(offTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(offTitle, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_letter_space(offTitle, 1, 0);
  lv_obj_align(offTitle, LV_ALIGN_TOP_MID, 0, 8);

  offLabel = lv_label_create(offPanel);
  lv_label_set_text(offLabel, "0.5s");
  lv_obj_set_style_text_font(offLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(offLabel, COL_TEXT, 0);
  lv_obj_align(offLabel, LV_ALIGN_CENTER, 0, 10);

  make_btn(screen, LV_SYMBOL_LEFT " RPM", SX(16), SY(98), SW(76), SH(36), rpm_minus_cb);

  lv_obj_t* rpmPanel = lv_obj_create(screen);
  lv_obj_set_size(rpmPanel, SW(160), SH(36));
  lv_obj_set_pos(rpmPanel, SX(100), SY(98));
  lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(rpmPanel, RADIUS_SM, 0);
  lv_obj_set_style_border_width(rpmPanel, 2, 0);
  lv_obj_set_style_border_color(rpmPanel, COL_ACCENT, 0);

  rpmGaugeLabel = lv_label_create(rpmPanel);
  char rpmBuf[16];
  snprintf(rpmBuf, sizeof(rpmBuf), "%.1f RPM", speed_get_target_rpm());
  lv_label_set_text(rpmGaugeLabel, rpmBuf);
  lv_obj_set_style_text_font(rpmGaugeLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(rpmGaugeLabel, COL_ACCENT, 0);
  lv_obj_center(rpmGaugeLabel);

  make_btn(screen, "RPM " LV_SYMBOL_RIGHT, SX(268), SY(98), SW(76), SH(36), rpm_plus_cb);

  lv_obj_t* sep2 = lv_obj_create(screen);
  lv_obj_set_size(sep2, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep2, 0, SY(144));
  lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep2, 0, 0);
  lv_obj_set_style_pad_all(sep2, 0, 0);
  lv_obj_set_style_radius(sep2, 0, 0);

  startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, SW(328), SH(40));
  lv_obj_set_pos(startBtn, SX(16), SY(152));
  lv_obj_set_style_bg_color(startBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_MD, 0);
  lv_obj_set_style_border_width(startBtn, 2, 0);
  lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLbl = lv_label_create(startBtn);
  lv_label_set_text(startLbl, LV_SYMBOL_PLAY " START");
  lv_obj_set_style_text_font(startLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(startLbl, COL_TEXT, 0);
  lv_obj_center(startLbl);

  lv_obj_t* sep3 = lv_obj_create(screen);
  lv_obj_set_size(sep3, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep3, 0, SY(200));
  lv_obj_set_style_bg_color(sep3, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep3, 0, 0);
  lv_obj_set_style_pad_all(sep3, 0, 0);
  lv_obj_set_style_radius(sep3, 0, 0);

  make_btn(screen, LV_SYMBOL_LEFT " ON", SX(16), SY(208), SW(76), SH(30), on_minus_cb);
  make_btn(screen, "ON " LV_SYMBOL_RIGHT, SX(100), SY(208), SW(76), SH(30), on_plus_cb);
  make_btn(screen, LV_SYMBOL_LEFT " OFF", SX(184), SY(208), SW(76), SH(30), off_minus_cb);
  make_btn(screen, "OFF " LV_SYMBOL_RIGHT, SX(268), SY(208), SW(76), SH(30), off_plus_cb);

  LOG_I("Screen pulse: SVG-matched layout created");
}

void screen_pulse_update() {
  if (!screens_is_active(SCREEN_PULSE)) return;

  float rpm = speed_get_actual_rpm();
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f RPM", rpm);
  lv_label_set_text(rpmGaugeLabel, buf);

  SystemState state = control_get_state();
  if (startBtn) {
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    if (state == STATE_PULSE) {
      if (label) lv_label_set_text(label, "\xE2\x96\xA0 STOP");
      lv_obj_set_style_bg_color(startBtn, COL_BTN_FILL, 0);
      lv_obj_set_style_border_color(startBtn, COL_RED, 0);
      if (label) lv_obj_set_style_text_color(label, COL_RED, 0);
    } else {
      if (label) lv_label_set_text(label, "\xE2\x96\xB6 START");
      lv_obj_set_style_bg_color(startBtn, COL_BTN_ACTIVE, 0);
      lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
      if (label) lv_obj_set_style_text_color(label, COL_TEXT, 0);
    }
  }
}
