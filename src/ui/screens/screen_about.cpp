// TIG Rotator Controller - About Screen
// Credits, version info, legal notices
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include <cstdio>

static void back_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }

static lv_obj_t* make_info_row(lv_obj_t* parent, int x, int y, int w, int h, const char* key,
                               const char* value) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, w, h);
  lv_obj_set_pos(row, x, y);
  ui_style_post_row(row);
  lv_obj_set_clickable(row, false);

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
  ui_create_header(screen, "About this controller", "", nullptr);
  ui_create_text(screen, 24, 102, 752, "TIG / ROTATOR", FONT_HUGE, COL_TEXT);
  ui_create_text(screen, 24, 158, 752, "DIY welding positioner", FONT_XL, COL_TEXT_DIM);
  char motion[40];  // The gear ratio is a setting, not a constant.
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  snprintf(motion, sizeof(motion), "NEMA 23 / %.4g:1 reduction", (double)g_settings.gear_ratio);
  xSemaphoreGive(g_settings_mutex);
  const char* names[] = {"Firmware", "Controller", "Display", "Motion", "Software"};
  const char* values[] = {FW_VERSION " (" FW_BUILD_ID ")", "ESP32-P4 + ESP32-C6", "4.3 inch / 800 x 480",
                          motion, "LVGL 9 / FastAccelStepper"};
  for (int i = 0; i < 5; ++i) make_info_row(screen, 24, 208 + i * 34, 752, 32, names[i], values[i]);
  ui_create_btn(screen, 24, 408, 152, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_cb, nullptr);
}

void screen_about_update() {}
