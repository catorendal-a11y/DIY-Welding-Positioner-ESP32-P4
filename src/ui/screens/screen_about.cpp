// TIG Rotator Controller - About Screen
// Credits, version info, legal notices
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

static lv_obj_t* make_info_row(lv_obj_t* parent, int x, int y, int w, int h,
                                const char* key, const char* value) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, w, h);
  lv_obj_set_pos(row, x, y);
  ui_style_post_row(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* keyLbl = lv_label_create(row);
  lv_label_set_text(keyLbl, key);
  lv_obj_set_style_text_font(keyLbl, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(keyLbl, COL_TEXT_DIM, 0);
  lv_obj_align(keyLbl, LV_ALIGN_LEFT_MID, 12, 0);

  lv_obj_t* valLbl = lv_label_create(row);
  lv_label_set_text(valLbl, value);
  lv_obj_set_style_text_font(valLbl, SET_VAL_FONT, 0);
  lv_obj_set_style_text_color(valLbl, COL_TEXT, 0);
  lv_obj_align(valLbl, LV_ALIGN_LEFT_MID, 160, 0);

  return row;
}

void screen_about_create() {
  lv_obj_t* screen = screenRoots[SCREEN_ABOUT];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;
  const int CW = SCREEN_W - 2 * PX;

  ui_create_settings_header(screen, "ABOUT", "", COL_TEXT_DIM);
  ui_create_post_card(screen, PX, 46, CW, 66);

  int y = 50;

  lv_obj_t* nameLbl = lv_label_create(screen);
  lv_label_set_text(nameLbl, "TIG Rotator Controller");
  lv_obj_set_style_text_font(nameLbl, FONT_LARGE, 0);
  lv_obj_set_style_text_color(nameLbl, COL_ACCENT, 0);
  lv_obj_set_pos(nameLbl, PX, y);

  y += 32;

  lv_obj_t* verLbl = lv_label_create(screen);
  lv_label_set_text(verLbl, FW_VERSION);
  lv_obj_set_style_text_font(verLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(verLbl, COL_TEXT, 0);
  lv_obj_set_pos(verLbl, PX, y);

  y += 36;

  ui_create_separator_line(screen, PX, y, CW, COL_SEPARATOR);
  y += 12;

  make_info_row(screen, PX, y, CW, 32, "BOARD", "GUITION JC4880P433 ESP32-P4");
  y += 36;

  make_info_row(screen, PX, y, CW, 32, "DISPLAY", "4.3 inch MIPI-DSI 800x480");
  y += 36;

  make_info_row(screen, PX, y, CW, 32, "MCU", "ESP32-P4 RISC-V 360MHz");
  y += 36;

  make_info_row(screen, PX, y, CW, 32, "CO-PROCESSOR", "ESP32-C6");
  y += 36;

  ui_create_separator_line(screen, PX, y, CW, COL_SEPARATOR);
  y += 12;

  make_info_row(screen, PX, y, CW, 32, "FRAMEWORK", "LVGL 9.x + FastAccelStepper");
  y += 36;

  make_info_row(screen, PX, y, CW, 32, "MOTOR", "NEMA 23, NMRV030 + spur (1:108)");
  y += 36;

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = 160;

  ui_create_btn(screen, PX, footerY, btnW, footerH, "BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb, nullptr);

  LOG_I("Screen about: created");
}

void screen_about_update() {
}
