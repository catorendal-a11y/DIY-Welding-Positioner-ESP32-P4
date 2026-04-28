// Menu Screen - POST-style navigation hub (no duplicate PROGRAMS / SETTINGS actions)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }
static void run_modes_event_cb(lv_event_t* e) { screens_show(SCREEN_RUN_MODES); }
static void setup_event_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }
static void programs_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }
static void diagnostics_event_cb(lv_event_t* e) { screens_show(SCREEN_DIAGNOSTICS); }

static lv_obj_t* make_nav_card(lv_obj_t* parent, int16_t x, int16_t y, int16_t w, int16_t h,
                               const char* title, const char* detail,
                               lv_event_cb_t cb, bool active) {
  lv_obj_t* card = lv_button_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  ui_nav_card_btn_style(card, active);
  lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* titleLbl = lv_label_create(card);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_LARGE, 0);
  lv_obj_set_style_text_color(titleLbl, active ? COL_ACCENT : COL_TEXT, 0);
  lv_obj_set_pos(titleLbl, 24, 24);

  lv_obj_t* detailLbl = lv_label_create(card);
  lv_label_set_text(detailLbl, detail);
  lv_obj_set_style_text_font(detailLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(detailLbl, COL_TEXT_DIM, 0);
  lv_obj_set_width(detailLbl, w - 64);
  lv_label_set_long_mode(detailLbl, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(detailLbl, 24, 64);

  lv_obj_t* chevron = lv_label_create(card);
  lv_label_set_text(chevron, ">");
  lv_obj_set_style_text_font(chevron, FONT_BTN, 0);
  lv_obj_set_style_text_color(chevron, active ? COL_ACCENT : COL_TEXT_DIM, 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -20, 0);
  return card;
}

void screen_menu_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MENU];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_header(screen, "MENU", "NAVIGATION HUB", nullptr);

  const int cardW = 374;
  const int cardH = 142;
  const int padX = 18;
  const int midGap = 16;
  const int row1Y = HEADER_H + 10;
  const int rowGap = 20;
  const int row2Y = row1Y + cardH + rowGap;
  const int leftX = padX;
  const int rightX = padX + cardW + midGap;
  const int footerY = 402;
  const int footerH = 62;

  make_nav_card(screen, leftX, row1Y, cardW, cardH, "RUN MODES", "Pulse / Step / Jog / Timer", run_modes_event_cb, false);
  make_nav_card(screen, rightX, row1Y, cardW, cardH, "SETUP", "Motor / display / pedal / calibration", setup_event_cb, false);
  make_nav_card(screen, leftX, row2Y, cardW, cardH, "PROGRAMS", "16 saved welding sequences", programs_event_cb, true);
  make_nav_card(screen, rightX, row2Y, cardW, cardH, "DIAGNOSTICS", "GPIO / faults / runtime state", diagnostics_event_cb, false);

  ui_create_btn(screen, padX, footerY, SCREEN_W - 2 * padX, footerH, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_event_cb,
                nullptr);

  LOG_I("Screen menu: POST-style navigation hub created");
}
