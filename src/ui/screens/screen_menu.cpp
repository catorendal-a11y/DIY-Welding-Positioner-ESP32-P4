// TIG Rotator Controller - Menu Screen
// Vertical list navigation matching ui_all_screens.svg blueprint

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }
static void pulse_event_cb(lv_event_t* e) { screens_show(SCREEN_PULSE); }
static void step_event_cb(lv_event_t* e) { screens_show(SCREEN_STEP); }
static void timer_event_cb(lv_event_t* e) { screens_show(SCREEN_TIMER); }
static void settings_event_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }
static void programs_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

// Helper: Create a menu row (full-width button with title left, subtitle right)
static lv_obj_t* create_menu_row(lv_obj_t* parent, int y,
                                  const char* title, const char* subtitle,
                                  lv_color_t titleColor, bool active) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 700, 80);
  lv_obj_set_pos(btn, 50, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(btn, 8, 0);

  if (active) {
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
  } else {
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER_SPD, 0);
  }

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text_fmt(label, LV_SYMBOL_RIGHT " %s", title);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(label, titleColor, 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* sub = lv_label_create(btn);
  lv_label_set_text(sub, subtitle);
  lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(sub, COL_TEXT_DIM, 0);
  lv_obj_align(sub, LV_ALIGN_RIGHT_MID, -16, 0);

  return btn;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — Vertical list menu (SVG: screen 2)
// ───────────────────────────────────────────────────────────────────────────────
void screen_menu_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MENU];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "MODE SELECTION MENU");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEXT_DIM, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

  // Menu rows — SVG: vertical stacked full-width buttons
  lv_obj_t* pulseBtn = create_menu_row(screen, 55,
    "PULSE MODE", "Tack weld", COL_ACCENT, true);
  lv_obj_add_event_cb(pulseBtn, pulse_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stepBtn = create_menu_row(screen, 145,
    "STEP MODE", "Angle position", COL_TEAL, false);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* timerBtn = create_menu_row(screen, 235,
    "TIMER MODE", "Timed rotation", COL_ACCENT, false);
  lv_obj_add_event_cb(timerBtn, timer_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* programsBtn = create_menu_row(screen, 325,
    "PROGRAMS", "Saved presets", COL_TEXT_DIM, false);
  lv_obj_add_event_cb(programsBtn, programs_event_cb, LV_EVENT_CLICKED, nullptr);

  // Back button (bottom)
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 160, 50);
  lv_obj_set_pos(backBtn, 50, 420);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 8, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  // Settings button (bottom right)
  lv_obj_t* settingsBtn = lv_btn_create(screen);
  lv_obj_set_size(settingsBtn, 160, 50);
  lv_obj_set_pos(settingsBtn, 590, 420);
  lv_obj_set_style_bg_color(settingsBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(settingsBtn, 8, 0);
  lv_obj_set_style_border_width(settingsBtn, 1, 0);
  lv_obj_set_style_border_color(settingsBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(settingsBtn, settings_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settLabel = lv_label_create(settingsBtn);
  lv_label_set_text(settLabel, "SETTINGS");
  lv_obj_set_style_text_color(settLabel, COL_TEXT_DIM, 0);
  lv_obj_center(settLabel);

  LOG_I("Screen menu: vertical list layout created");
}
