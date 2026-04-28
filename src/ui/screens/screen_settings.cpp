// TIG Rotator Controller - Settings Menu Screen
// Navigation to motor config, display, pedal, diagnostics hub row
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void nav_click_cb(lv_event_t* e) {
  ScreenId dest = (ScreenId)(size_t)lv_event_get_user_data(e);
  screens_show(dest);
}

static void create_nav_item(lv_obj_t* parent, int y, int rowH, const char* label, ScreenId dest, bool accentRow) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 776, rowH);
  lv_obj_set_pos(row, 12, y);
  lv_obj_set_style_bg_color(row, accentRow ? COL_BG_ACTIVE : COL_BG_ROW, 0);
  lv_obj_set_style_border_color(row, accentRow ? COL_ACCENT : COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(row, accentRow ? 2 : 1, 0);
  lv_obj_set_style_radius(row, RADIUS_ROW, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, nav_click_cb, LV_EVENT_CLICKED, (void*)(size_t)dest);

  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_font(lbl, SET_VAL_FONT, 0);
  lv_obj_set_style_text_color(lbl, accentRow ? COL_ACCENT : COL_TEXT, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* chevron = lv_label_create(row);
  lv_label_set_text(chevron, ">");
  lv_obj_set_style_text_font(chevron, FONT_BTN, 0);
  lv_obj_set_style_text_color(chevron, accentRow ? COL_ACCENT : SET_CHEVRON_COL, 0);
  lv_obj_align(chevron, LV_ALIGN_RIGHT_MID, -16, 0);
}

void screen_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_settings_header(screen, "SETTINGS", "SYSTEM CONFIG", COL_TEXT_DIM);

  const int rowH = 44;
  const int rowY0 = SET_HEADER_H + 10;
  const int rowGap = 5;

  create_nav_item(screen, rowY0 + (rowH + rowGap) * 0, rowH, "Motor Configuration", SCREEN_MOTOR_CONFIG, false);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 1, rowH, "Calibration", SCREEN_CALIBRATION, false);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 2, rowH, "Display Settings", SCREEN_DISPLAY, true);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 3, rowH, "Pedal Settings", SCREEN_PEDAL_SETTINGS, false);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 4, rowH, "Diagnostics", SCREEN_DIAGNOSTICS, false);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 5, rowH, "System Info", SCREEN_SYSINFO, false);
  create_nav_item(screen, rowY0 + (rowH + rowGap) * 6, rowH, "About", SCREEN_ABOUT, false);

  ui_create_btn(screen, 20, SET_FOOTER_Y, 200, SET_FOOTER_H, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_event_cb,
                nullptr);

  lv_obj_t* versionLbl = lv_label_create(screen);
  lv_label_set_text(versionLbl, FW_VERSION);
  lv_obj_set_style_text_font(versionLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(versionLbl, COL_TEXT_VDIM, 0);
  lv_obj_set_style_text_align(versionLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(versionLbl, 240);
  lv_obj_align(versionLbl, LV_ALIGN_BOTTOM_MID, 0, -14);
}
