#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }
static void pulse_event_cb(lv_event_t* e) { screens_show(SCREEN_PULSE); }
static void step_event_cb(lv_event_t* e) { screens_show(SCREEN_STEP); }
static void jog_event_cb(lv_event_t* e) { screens_show(SCREEN_JOG); }
static void timer_event_cb(lv_event_t* e) { screens_show(SCREEN_TIMER); }
static void settings_event_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }
static void programs_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

static lv_obj_t* create_btn(lv_obj_t* parent, int16_t x, int16_t y,
                            int16_t w, int16_t h, const char* text, bool active) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_radius(btn, RADIUS_SM, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);

  if (active) {
    lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
  } else {
    lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
  }

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(label, COL_TEXT, 0);
  if (active) {
    lv_obj_set_style_text_font(label, FONT_NORMAL, 0);
  }
  lv_obj_center(label);

  return btn;
}

void screen_menu_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MENU];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  lv_obj_t* subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "MODE SELECTION MENU");
  lv_obj_set_style_text_font(subtitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(subtitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(subtitle, SX(180), SY(18));
  lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(subtitle, SW(360));

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "02 — SELECT MODE");
  lv_obj_set_style_text_font(title, FONT_LARGE, 0);
  lv_obj_set_style_text_color(title, COL_TEXT, 0);
  lv_obj_set_pos(title, SX(180), SY(38));
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title, SW(360));

  lv_obj_t* pulseBtn = create_btn(screen, SX(27), SY(50), SW(144), SH(34), "PULSE", false);
  lv_obj_add_event_cb(pulseBtn, pulse_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stepBtn = create_btn(screen, SX(189), SY(50), SW(144), SH(34), "STEP", false);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* jogBtn = create_btn(screen, SX(27), SY(90), SW(144), SH(34), "JOG", false);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* timerBtn = create_btn(screen, SX(189), SY(90), SW(144), SH(34), "TIMER", false);
  lv_obj_add_event_cb(timerBtn, timer_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* programsBtn = create_btn(screen, SX(27), SY(132), SW(306), SH(28), "PROGRAMS", false);
  lv_obj_add_event_cb(programsBtn, programs_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SW(360), SH(2));
  lv_obj_set_pos(sep, 0, SY(168));
  lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);

  lv_obj_t* backBtn = create_btn(screen, SX(27), SY(178), SW(144), SH(28), LV_SYMBOL_LEFT "  BACK", false);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsBtn = create_btn(screen, SX(189), SY(178), SW(144), SH(28), "SETTINGS " LV_SYMBOL_SETTINGS, true);
  lv_obj_add_event_cb(settingsBtn, settings_event_cb, LV_EVENT_CLICKED, nullptr);

  LOG_I("Screen menu: SVG-matched layout created");
}
