// TIG Rotator Controller - Settings Screen
// Matching ui_all_screens.svg: Simple settings list with values on right

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

// Helper: Create a settings row (full-width, label left, value right)
static lv_obj_t* create_settings_row(lv_obj_t* parent, int y,
                                      const char* name, const char* value,
                                      lv_color_t valueColor) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 700, 60);
  lv_obj_set_pos(row, 50, y);
  lv_obj_set_style_bg_color(row, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(row, 6, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_border_color(row, COL_BORDER_SPD, 0);
  lv_obj_set_style_pad_all(row, 0, 0);

  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, name);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label, COL_TEXT, 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 20, 0);

  lv_obj_t* val = lv_label_create(row);
  lv_label_set_text(val, value);
  lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(val, valueColor, 0);
  lv_obj_align(val, LV_ALIGN_RIGHT_MID, -20, 0);

  return row;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Simple list with values on right side
// ───────────────────────────────────────────────────────────────────────────────
void screen_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "SETTINGS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEXT_DIM, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  // SVG settings rows
  create_settings_row(screen, 55, "Max RPM Limit", "3.0", COL_ACCENT);
  create_settings_row(screen, 125, "Acceleration", "5000", COL_TEXT_DIM);
  create_settings_row(screen, 195, "Screen Brightness", "100%", COL_TEXT_DIM);
  create_settings_row(screen, 265, "Gear Ratio", "60:1", COL_TEXT_DIM);
  create_settings_row(screen, 335, "Microstep", "1/8", COL_TEXT_DIM);

  // Back button
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 160, 50);
  lv_obj_set_pos(backBtn, 50, 415);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_TEXT_DIM, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  LOG_I("Screen settings: SVG-matched layout created");
}
