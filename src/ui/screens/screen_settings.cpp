// TIG Rotator Controller - Settings Menu Screen
// Navigation to motor config, display, calibration, about
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void nav_click_cb(lv_event_t* e) {
  ScreenId dest = (ScreenId)(size_t)lv_event_get_user_data(e);
  screens_show(dest);
}

static void create_nav_item(lv_obj_t* parent, int y, const char* label, ScreenId dest) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 776, SET_ROW_H);
  lv_obj_set_pos(row, 12, y);
  lv_obj_set_style_bg_color(row, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(row, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, nav_click_cb, LV_EVENT_CLICKED, (void*)(size_t)dest);

  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* chevron = lv_label_create(row);
  lv_label_set_text(chevron, ">");
  lv_obj_set_style_text_font(chevron, FONT_BTN, 0);
  lv_obj_set_style_text_color(chevron, SET_CHEVRON_COL, 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -16, 0);
}

void screen_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_settings_header(screen, "SETTINGS");

  const int itemH = SET_ROW_H;
  const int gap = 5;
  const int startY = SET_HEADER_H + 8;

  create_nav_item(screen, startY, "Motor Configuration", SCREEN_MOTOR_CONFIG);
  create_nav_item(screen, startY + (itemH + gap) * 1, "Calibration", SCREEN_CALIBRATION);
  create_nav_item(screen, startY + (itemH + gap) * 2, "Display Settings", SCREEN_DISPLAY);
  create_nav_item(screen, startY + (itemH + gap) * 3, "System Info", SCREEN_SYSINFO);
  create_nav_item(screen, startY + (itemH + gap) * 4, "About", SCREEN_ABOUT);

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = 160;

  ui_create_btn(screen, 12, footerY, btnW, footerH, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_event_cb, nullptr);

  lv_obj_t* versionLbl = lv_label_create(screen);
  lv_label_set_text(versionLbl, FW_VERSION);
  lv_obj_set_style_text_font(versionLbl, FONT_BODY, 0);
  lv_obj_set_style_text_color(versionLbl, COL_TEXT_VDIM, 0);
  lv_obj_align(versionLbl, LV_ALIGN_BOTTOM_MID, 0, -8);
}
