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
  ui_create_btn(screen, 192, 408, 288, 56, "SETUP WIZARD", FONT_BTN, UI_BTN_NORMAL, nav_click_cb, (void*)(intptr_t)SCREEN_SETUP);
  ui_create_btn(screen, 496, 408, 280, 56, "About", FONT_BTN, UI_BTN_NORMAL, nav_click_cb,
                (void*)(intptr_t)SCREEN_ABOUT);
}
