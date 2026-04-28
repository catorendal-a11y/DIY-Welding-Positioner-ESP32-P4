// Run Modes picker - Pulse / Step / Jog / Timer (from MENU), same card scale as MENU

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_event_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_MENU);
}

static void mode_pick_cb(lv_event_t* e) {
  ScreenId dest = (ScreenId)(size_t)lv_event_get_user_data(e);
  screens_show(dest);
}

static lv_obj_t* make_mode_card(lv_obj_t* parent, int16_t x, int16_t y, int16_t w, int16_t h,
                                const char* title, const char* detail, ScreenId dest, bool accent) {
  lv_obj_t* card = lv_button_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  ui_nav_card_btn_style(card, accent);
  lv_obj_add_event_cb(card, mode_pick_cb, LV_EVENT_CLICKED, (void*)(size_t)dest);

  lv_obj_t* titleLbl = lv_label_create(card);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_LARGE, 0);
  lv_obj_set_style_text_color(titleLbl, accent ? COL_ACCENT : COL_TEXT, 0);
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
  lv_obj_set_style_text_color(chevron, accent ? COL_ACCENT : COL_TEXT_DIM, 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -20, 0);
  return card;
}

void screen_run_modes_create() {
  lv_obj_t* screen = screenRoots[SCREEN_RUN_MODES];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_header(screen, "RUN MODES", "SELECT MODE", nullptr);

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

  make_mode_card(screen, leftX, row1Y, cardW, cardH, "PULSE", "Timed on/off rotation cycles", SCREEN_PULSE, false);
  make_mode_card(screen, rightX, row1Y, cardW, cardH, "STEP", "Move by angle, then stop", SCREEN_STEP, false);
  make_mode_card(screen, leftX, row2Y, cardW, cardH, "JOG", "Hold to run, release to stop", SCREEN_JOG, false);
  make_mode_card(screen, rightX, row2Y, cardW, cardH, "3-2-1", "Countdown then start continuous", SCREEN_TIMER, false);

  ui_create_btn(screen, padX, footerY, SCREEN_W - 2 * padX, footerH, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_event_cb,
                nullptr);

  LOG_I("Screen run modes: POST picker created");
}
