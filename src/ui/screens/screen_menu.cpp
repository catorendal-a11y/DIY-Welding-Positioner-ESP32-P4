// TIG Rotator Controller - Advanced Menu Screen (T-24)
// Navigation hub for all advanced modes

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  screens_show(SCREEN_MAIN);
}

static void pulse_event_cb(lv_event_t* e) {
  screens_show(SCREEN_PULSE);
}

static void step_event_cb(lv_event_t* e) {
  screens_show(SCREEN_STEP);
}

static void jog_event_cb(lv_event_t* e) {
  screens_show(SCREEN_JOG);
}

static void timer_event_cb(lv_event_t* e) {
  screens_show(SCREEN_TIMER);
}

static void settings_event_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_menu_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MENU];

  // Header
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  // Back button
  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, STATUS_BAR_H);
  lv_obj_set_style_bg_color(backBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_set_style_text_color(backLabel, COL_ACCENT, 0);
  lv_obj_center(backLabel);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "ADVANCED SETTINGS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // MODE BUTTONS (2×2 grid)
  // ─────────────────────────────────────────────────────────────────────────
  const int btnW = 220;
  const int btnH = 100;
  const int gap = 16;
  const int startX = 12;
  const int startY = 44;

  // PULSE button (top-left)
  lv_obj_t* pulseBtn = lv_btn_create(screen);
  lv_obj_set_size(pulseBtn, btnW, btnH);
  lv_obj_set_pos(pulseBtn, startX, startY);
  lv_obj_set_style_bg_color(pulseBtn, COL_BG_CARD, 0);
  lv_obj_set_style_radius(pulseBtn, RADIUS_CARD, 0);
  lv_obj_set_style_border_width(pulseBtn, 2, 0);
  lv_obj_set_style_border_color(pulseBtn, COL_PURPLE, 0);
  lv_obj_add_event_cb(pulseBtn, pulse_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* pulseIcon = lv_label_create(pulseBtn);
  lv_label_set_text(pulseIcon, "◉");
  lv_obj_set_style_text_font(pulseIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(pulseIcon, COL_PURPLE, 0);
  lv_obj_align(pulseIcon, LV_ALIGN_TOP_LEFT, 12, 8);

  lv_obj_t* pulseTitle = lv_label_create(pulseBtn);
  lv_label_set_text(pulseTitle, "PULSE MODE");
  lv_obj_set_style_text_font(pulseTitle, &lv_font_montserrat_16, 0);
  lv_obj_align(pulseTitle, LV_ALIGN_TOP_LEFT, 12, 36);

  lv_obj_t* pulseSub = lv_label_create(pulseBtn);
  lv_label_set_text(pulseSub, "Rotate / Pause cycle");
  lv_obj_set_style_text_color(pulseSub, COL_TEXT_DIM, 0);
  lv_obj_align(pulseSub, LV_ALIGN_TOP_LEFT, 12, 60);

  // STEP button (top-right)
  lv_obj_t* stepBtn = lv_btn_create(screen);
  lv_obj_set_size(stepBtn, btnW, btnH);
  lv_obj_set_pos(stepBtn, startX + btnW + gap, startY);
  lv_obj_set_style_bg_color(stepBtn, COL_BG_CARD, 0);
  lv_obj_set_style_radius(stepBtn, RADIUS_CARD, 0);
  lv_obj_set_style_border_width(stepBtn, 2, 0);
  lv_obj_set_style_border_color(stepBtn, COL_TEAL, 0);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stepIcon = lv_label_create(stepBtn);
  lv_label_set_text(stepIcon, "◈");
  lv_obj_set_style_text_font(stepIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(stepIcon, COL_TEAL, 0);
  lv_obj_align(stepIcon, LV_ALIGN_TOP_LEFT, 12, 8);

  lv_obj_t* stepTitle = lv_label_create(stepBtn);
  lv_label_set_text(stepTitle, "STEP MODE");
  lv_obj_set_style_text_font(stepTitle, &lv_font_montserrat_16, 0);
  lv_obj_align(stepTitle, LV_ALIGN_TOP_LEFT, 12, 36);

  lv_obj_t* stepSub = lv_label_create(stepBtn);
  lv_label_set_text(stepSub, "Fixed angle steps");
  lv_obj_set_style_text_color(stepSub, COL_TEXT_DIM, 0);
  lv_obj_align(stepSub, LV_ALIGN_TOP_LEFT, 12, 60);

  // JOG button (bottom-left)
  lv_obj_t* jogBtn = lv_btn_create(screen);
  lv_obj_set_size(jogBtn, btnW, btnH);
  lv_obj_set_pos(jogBtn, startX, startY + btnH + gap);
  lv_obj_set_style_bg_color(jogBtn, COL_BG_CARD, 0);
  lv_obj_set_style_radius(jogBtn, RADIUS_CARD, 0);
  lv_obj_set_style_border_width(jogBtn, 2, 0);
  lv_obj_set_style_border_color(jogBtn, COL_AMBER, 0);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* jogIcon = lv_label_create(jogBtn);
  lv_label_set_text(jogIcon, "◎");
  lv_obj_set_style_text_font(jogIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(jogIcon, COL_AMBER, 0);
  lv_obj_align(jogIcon, LV_ALIGN_TOP_LEFT, 12, 8);

  lv_obj_t* jogTitle = lv_label_create(jogBtn);
  lv_label_set_text(jogTitle, "JOG MODE");
  lv_obj_set_style_text_font(jogTitle, &lv_font_montserrat_16, 0);
  lv_obj_align(jogTitle, LV_ALIGN_TOP_LEFT, 12, 36);

  lv_obj_t* jogSub = lv_label_create(jogBtn);
  lv_label_set_text(jogSub, "Manual positioning");
  lv_obj_set_style_text_color(jogSub, COL_TEXT_DIM, 0);
  lv_obj_align(jogSub, LV_ALIGN_TOP_LEFT, 12, 60);

  // TIMER button (bottom-right)
  lv_obj_t* timerBtn = lv_btn_create(screen);
  lv_obj_set_size(timerBtn, btnW, btnH);
  lv_obj_set_pos(timerBtn, startX + btnW + gap, startY + btnH + gap);
  lv_obj_set_style_bg_color(timerBtn, COL_BG_CARD, 0);
  lv_obj_set_style_radius(timerBtn, RADIUS_CARD, 0);
  lv_obj_set_style_border_width(timerBtn, 2, 0);
  lv_obj_set_style_border_color(timerBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(timerBtn, timer_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* timerIcon = lv_label_create(timerBtn);
  lv_label_set_text(timerIcon, "⏱");
  lv_obj_set_style_text_font(timerIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(timerIcon, COL_ACCENT, 0);
  lv_obj_align(timerIcon, LV_ALIGN_TOP_LEFT, 12, 8);

  lv_obj_t* timerTitle = lv_label_create(timerBtn);
  lv_label_set_text(timerTitle, "TIMER MODE");
  lv_obj_set_style_text_font(timerTitle, &lv_font_montserrat_16, 0);
  lv_obj_align(timerTitle, LV_ALIGN_TOP_LEFT, 12, 36);

  lv_obj_t* timerSub = lv_label_create(timerBtn);
  lv_label_set_text(timerSub, "Run for duration");
  lv_obj_set_style_text_color(timerSub, COL_TEXT_DIM, 0);
  lv_obj_align(timerSub, LV_ALIGN_TOP_LEFT, 12, 60);

  // SETTINGS button (bottom)
  lv_obj_t* settingsBtn = lv_btn_create(screen);
  lv_obj_set_size(settingsBtn, 456, 60);
  lv_obj_set_pos(settingsBtn, 12, startY + (btnH + gap) * 2);
  lv_obj_set_style_bg_color(settingsBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(settingsBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(settingsBtn, settings_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsIcon = lv_label_create(settingsBtn);
  lv_label_set_text(settingsIcon, "⚙");
  lv_obj_set_style_text_font(settingsIcon, &lv_font_montserrat_24, 0);
  lv_obj_align(settingsIcon, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* settingsLabel = lv_label_create(settingsBtn);
  lv_label_set_text(settingsLabel, "SETTINGS");
  lv_obj_set_style_text_font(settingsLabel, &lv_font_montserrat_18, 0);
  lv_obj_align(settingsLabel, LV_ALIGN_LEFT_MID, 50, 0);

  lv_obj_t* settingsSub = lv_label_create(settingsBtn);
  lv_label_set_text(settingsSub, "System configuration");
  lv_obj_set_style_text_color(settingsSub, COL_TEXT_DIM, 0);
  lv_obj_align(settingsSub, LV_ALIGN_RIGHT_MID, -16, 0);

  LOG_I("Screen menu: created");
}
