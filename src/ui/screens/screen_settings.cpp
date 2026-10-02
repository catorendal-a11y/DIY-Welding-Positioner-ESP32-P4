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

static void create_nav_item(lv_obj_t* parent, int y, int rowH, const char* label, ScreenId dest,
                            bool accentRow) {
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
  ui_create_header(screen, "Settings", "SYSTEM CONFIG", nullptr);
  const char* names[] = {"Motor Configuration", "Calibration", "Pedal Settings",
                         "Display Settings",    "Diagnostics", "System Info"};
  const char* details[] = {"Drive and motion",      "Angle accuracy",    "Input and control",
                           "Screen and USB mirror", "Inputs and faults", "Health and firmware"};
  const ScreenId targets[] = {SCREEN_MOTOR_CONFIG, SCREEN_CALIBRATION, SCREEN_PEDAL_SETTINGS,
                              SCREEN_DISPLAY,      SCREEN_DIAGNOSTICS, SCREEN_SYSINFO};
  for (int i = 0; i < 6; ++i) {
    lv_obj_t* card = lv_button_create(screen);
    lv_obj_set_pos(card, 24 + (i % 2) * 384, 94 + (i / 2) * 92);
    lv_obj_set_size(card, 368, 76);
    ui_nav_card_btn_style(card, false);
    lv_obj_add_event_cb(card, nav_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)targets[i]);
    ui_create_text(card, 18, 12, 330, names[i], FONT_LARGE, COL_TEXT);
    ui_create_text(card, 18, 44, 330, details[i], FONT_SUBTITLE, COL_TEXT_DIM);
  }
  ui_create_btn(screen, 24, 408, 152, 56, "<  BACK", FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "About", FONT_BTN, UI_BTN_NORMAL, nav_click_cb,
                (void*)(intptr_t)SCREEN_ABOUT);
}
