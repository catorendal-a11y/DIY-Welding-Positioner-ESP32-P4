// TIG Rotator Controller - Main Screen (T-22)
// RPM gauge, start/stop, direction, speed slider, bottom navigation

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../control/control.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* mainScreen = nullptr;
static lv_obj_t* stateLabel = nullptr;
static lv_obj_t* rpmArc = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmUnitLabel = nullptr;
static lv_obj_t* startStopBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* speedSlider = nullptr;
static lv_obj_t* speedValueLabel = nullptr;
static lv_obj_t* speedSourceLabel = nullptr;
static lv_obj_t* dirLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// STATE BADGE COLORS
// ───────────────────────────────────────────────────────────────────────────────
static const char* state_badge_strings[] = {
  "● IDLE",      // STATE_IDLE
  "● RUNNING",   // STATE_RUNNING
  "◉ PULSE",     // STATE_PULSE
  "◈ STEP",      // STATE_STEP
  "◎ JOG",       // STATE_JOG
  "⏱ TIMER",     // STATE_TIMER
  "◌ STOPPING",  // STATE_STOPPING
  "⛔ ESTOP"     // STATE_ESTOP
};

static lv_color_t state_badge_colors[] = {
  COL_TEXT_DIM,   // IDLE
  COL_GREEN,      // RUNNING
  COL_PURPLE,     // PULSE
  COL_TEAL,       // STEP
  COL_AMBER,      // JOG
  COL_ACCENT,     // TIMER
  COL_AMBER,      // STOPPING
  COL_RED         // ESTOP
};

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void start_stop_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();

  if (state == STATE_IDLE) {
    control_start_continuous();
  } else if (state == STATE_RUNNING) {
    control_stop();
  }
}

static void ccw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CCW);
  screen_main_update();  // Refresh button states
}

static void cw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CW);
  screen_main_update();  // Refresh button states
}

static void speed_slider_event_cb(lv_event_t* e) {
  lv_obj_t* slider = lv_event_get_target(e);
  int32_t value = lv_slider_get_value(slider);
  float rpm = value * 0.1f;
  speed_slider_set(rpm);
  lv_label_set_text_fmt(speedValueLabel, "%.1f RPM", rpm);
}

static void nav_advanced_event_cb(lv_event_t* e) {
  screens_show(SCREEN_MENU);
}

static void nav_programs_event_cb(lv_event_t* e) {
  screens_show(SCREEN_PROGRAMS);
}

static void nav_dir_event_cb(lv_event_t* e) {
  Direction dir = speed_get_direction();
  speed_set_direction((dir == DIR_CW) ? DIR_CCW : DIR_CW);
  screen_main_update();
}

static void nav_settings_event_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create() {
  mainScreen = screenRoots[SCREEN_MAIN];

  // ─────────────────────────────────────────────────────────────────────────
  // STATUS BAR (y=0, h=36)
  // ─────────────────────────────────────────────────────────────────────────
  lv_obj_t* statusBar = lv_obj_create(mainScreen);
  lv_obj_set_pos(statusBar, 0, 0);
  lv_obj_set_size(statusBar, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_style_bg_color(statusBar, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(statusBar, 0, 0);
  lv_obj_set_style_pad_all(statusBar, 0, 0);

  // Title
  lv_obj_t* title = lv_label_create(statusBar);
  lv_label_set_text(title, "TIG ROTATOR");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

  // State badge
  stateLabel = lv_label_create(statusBar);
  lv_label_set_text(stateLabel, state_badge_strings[STATE_IDLE]);
  lv_obj_set_style_text_font(stateLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(stateLabel, state_badge_colors[STATE_IDLE], 0);
  lv_obj_align(stateLabel, LV_ALIGN_CENTER, 60, 0);

  // Speed source indicator
  speedSourceLabel = lv_label_create(statusBar);
  lv_label_set_text(speedSourceLabel, "▣ POT");
  lv_obj_set_style_text_font(speedSourceLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(speedSourceLabel, COL_TEXT_DIM, 0);
  lv_obj_align(speedSourceLabel, LV_ALIGN_CENTER, 100, 0);

  // System OK indicator
  lv_obj_t* okLabel = lv_label_create(statusBar);
  lv_label_set_text(okLabel, "⚡ OK");
  lv_obj_set_style_text_font(okLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(okLabel, COL_GREEN, 0);
  lv_obj_align(okLabel, LV_ALIGN_RIGHT_MID, -12, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // RPM GAUGE (y=44, h=148) - lv_arc
  // ─────────────────────────────────────────────────────────────────────────
  rpmArc = lv_arc_create(mainScreen);
  lv_obj_set_size(rpmArc, 220, 140);
  lv_obj_align(rpmArc, LV_ALIGN_TOP_MID, 0, 44);
  lv_arc_set_range(rpmArc, 0, 50);  // 0-5.0 RPM × 10
  lv_arc_set_value(rpmArc, 0);
  lv_arc_set_bg_angles(rpmArc, 135, 45);  // 270 degree arc
  lv_obj_remove_style(rpmArc, nullptr, LV_PART_KNOB);  // Hide knob
  lv_obj_set_style_arc_width(rpmArc, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(rpmArc, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(rpmArc, COL_BG_CARD, LV_PART_MAIN);
  lv_obj_set_style_arc_color(rpmArc, COL_ACCENT, LV_PART_INDICATOR);

  // RPM number (centered in arc)
  rpmLabel = lv_label_create(mainScreen);
  lv_label_set_text(rpmLabel, "0.0");
  lv_obj_set_style_text_font(rpmLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, 0, 88);

  // RPM unit
  rpmUnitLabel = lv_label_create(mainScreen);
  lv_label_set_text(rpmUnitLabel, "RPM");
  lv_obj_set_style_text_font(rpmUnitLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(rpmUnitLabel, COL_TEXT_DIM, 0);
  lv_obj_align(rpmUnitLabel, LV_ALIGN_TOP_MID, 45, 100);

  // ─────────────────────────────────────────────────────────────────────────
  // ACTION BUTTONS (y=204, h=60)
  // ─────────────────────────────────────────────────────────────────────────
  // CCW button
  ccwBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(ccwBtn, 100, 60);
  lv_obj_set_pos(ccwBtn, 10, 204);
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(ccwBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* ccwLabel = lv_label_create(ccwBtn);
  lv_label_set_text(ccwLabel, "◄ CCW");
  lv_obj_center(ccwLabel);

  // START/STOP button (center)
  startStopBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(startStopBtn, 160, 60);
  lv_obj_set_pos(startStopBtn, 120, 204);
  lv_obj_set_style_bg_color(startStopBtn, COL_GREEN_DARK, 0);
  lv_obj_set_style_radius(startStopBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(startStopBtn, start_stop_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startStopLabel = lv_label_create(startStopBtn);
  lv_label_set_text(startStopLabel, "▶ START");
  lv_obj_set_style_text_font(startStopLabel, &lv_font_montserrat_18, 0);
  lv_obj_center(startStopLabel);

  // CW button
  cwBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(cwBtn, 100, 60);
  lv_obj_set_pos(cwBtn, 290, 204);
  lv_obj_set_style_bg_color(cwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(cwBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(cwBtn, 2, 0);
  lv_obj_set_style_border_color(cwBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cwLabel = lv_label_create(cwBtn);
  lv_label_set_text(cwLabel, "CW ►");
  lv_obj_center(cwLabel);

  // Direction label (below buttons)
  dirLabel = lv_label_create(mainScreen);
  lv_label_set_text(dirLabel, "↻ CW");
  lv_obj_set_style_text_color(dirLabel, COL_TEXT_DIM, 0);
  lv_obj_align(dirLabel, LV_ALIGN_TOP_MID, 60, 200);

  // ─────────────────────────────────────────────────────────────────────────
  // SPEED SLIDER (y=272, h=20)
  // ─────────────────────────────────────────────────────────────────────────
  speedSlider = lv_slider_create(mainScreen);
  lv_obj_set_size(speedSlider, 340, 20);
  lv_obj_align(speedSlider, LV_ALIGN_BOTTOM_MID, -40, -48);
  lv_slider_set_range(speedSlider, 1, 50);  // ×0.1 RPM
  lv_slider_set_value(speedSlider, 25, LV_ANIM_OFF);  // Default 2.5 RPM
  lv_obj_set_style_bg_color(speedSlider, COL_BG_INPUT, 0);
  lv_obj_set_style_bg_color(speedSlider, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_add_event_cb(speedSlider, speed_slider_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Speed value label
  speedValueLabel = lv_label_create(mainScreen);
  lv_label_set_text(speedValueLabel, "2.5 RPM");
  lv_obj_set_style_text_font(speedValueLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(speedValueLabel, COL_TEXT, 0);
  lv_obj_align(speedValueLabel, LV_ALIGN_BOTTOM_MID, 60, -45);

  // ─────────────────────────────────────────────────────────────────────────
  // BOTTOM NAV (y=280, h=44)
  // ─────────────────────────────────────────────────────────────────────────
  // ADVANCED button
  lv_obj_t* advBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(advBtn, 110, NAV_BAR_H);
  lv_obj_set_pos(advBtn, 0, NAV_BAR_Y);
  lv_obj_set_style_bg_color(advBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(advBtn, 0, 0);
  lv_obj_add_event_cb(advBtn, nav_advanced_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* advLabel = lv_label_create(advBtn);
  lv_label_set_text(advLabel, "ADVANCED");
  lv_obj_center(advLabel);

  // PROGRAMS button
  lv_obj_t* progBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(progBtn, 110, NAV_BAR_H);
  lv_obj_set_pos(progBtn, 115, NAV_BAR_Y);
  lv_obj_set_style_bg_color(progBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(progBtn, 0, 0);
  lv_obj_add_event_cb(progBtn, nav_programs_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* progLabel = lv_label_create(progBtn);
  lv_label_set_text(progLabel, "PROGRAMS");
  lv_obj_center(progLabel);

  // Direction toggle button
  lv_obj_t* dirBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(dirBtn, 130, NAV_BAR_H);
  lv_obj_set_pos(dirBtn, 230, NAV_BAR_Y);
  lv_obj_set_style_bg_color(dirBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(dirBtn, 0, 0);
  lv_obj_add_event_cb(dirBtn, nav_dir_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* dirBtnLabel = lv_label_create(dirBtn);
  lv_label_set_text(dirBtnLabel, "↻ CW ▼");
  lv_obj_center(dirBtnLabel);

  // SETTINGS button
  lv_obj_t* settingsBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(settingsBtn, 115, NAV_BAR_H);
  lv_obj_set_pos(settingsBtn, 365, NAV_BAR_Y);
  lv_obj_set_style_bg_color(settingsBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(settingsBtn, 0, 0);
  lv_obj_add_event_cb(settingsBtn, nav_settings_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsLabel = lv_label_create(settingsBtn);
  lv_label_set_text(settingsLabel, "⚙ SETTINGS");
  lv_obj_center(settingsLabel);

  LOG_I("Screen main: created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_update() {
  if (!screens_is_active(SCREEN_MAIN)) return;

  SystemState state = control_get_state();
  Direction dir = speed_get_direction();
  float rpm = speed_get_actual_rpm();

  // Update state badge
  lv_label_set_text(stateLabel, state_badge_strings[state]);
  lv_obj_set_style_text_color(stateLabel, state_badge_colors[state], 0);

  // Update RPM gauge and label
  lv_arc_set_value(rpmArc, (int)(rpm * 10));
  lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);

  // Update speed source indicator
  lv_label_set_text(speedSourceLabel, speed_using_slider() ? "◈ SLIDER" : "▣ POT");

  // Update START/STOP button
  lv_obj_t* btnLabel = lv_obj_get_child(startStopBtn, 0);
  if (state == STATE_IDLE) {
    lv_label_set_text(btnLabel, "▶ START");
    lv_obj_set_style_bg_color(startStopBtn, COL_GREEN_DARK, 0);
  } else if (state == STATE_RUNNING) {
    lv_label_set_text(btnLabel, "■ STOP");
    lv_obj_set_style_bg_color(startStopBtn, COL_RED, 0);
  }

  // Update direction buttons
  if (dir == DIR_CW) {
    lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);
    lv_obj_set_style_text_color(cwBtn, COL_ACCENT, 0);
    lv_obj_set_style_text_color(ccwBtn, COL_TEXT, 0);
    lv_label_set_text(dirLabel, "↻ CW");
  } else {
    lv_obj_set_style_border_color(ccwBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_color(cwBtn, COL_BORDER, 0);
    lv_obj_set_style_text_color(ccwBtn, COL_ACCENT, 0);
    lv_obj_set_style_text_color(cwBtn, COL_TEXT, 0);
    lv_label_set_text(dirLabel, "↻ CCW");
  }
}
