// TIG Rotator Controller - Main Screen
// Modern Dark Theme UI with Side Panels & Central Meter

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

// Center Gauge
static lv_obj_t* rpmMeter = nullptr;
static lv_meter_scale_t* scaleBase = nullptr;
static lv_meter_scale_t* scaleActive = nullptr;
static lv_meter_indicator_t* rpmIndicator = nullptr;
static lv_obj_t* rpmLabel = nullptr;

// Side Panel Buttons
static lv_obj_t* startBtn = nullptr;
static lv_obj_t* stopBtn = nullptr;
static lv_obj_t* jogBtn = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;
static lv_obj_t* menuBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// STATE BADGE COLORS
// ───────────────────────────────────────────────────────────────────────────────
static const char* state_badge_strings[] = {
  "IDLE",      // STATE_IDLE
  "RUNNING",   // STATE_RUNNING
  "PULSE",     // STATE_PULSE
  "STEP",      // STATE_STEP
  "JOG",       // STATE_JOG
  "TIMER",     // STATE_TIMER
  "STOPPING",  // STATE_STOPPING
  "ESTOP"      // STATE_ESTOP
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
static void start_event_cb(lv_event_t* e) {
  if (control_get_state() == STATE_IDLE) {
    control_start_continuous();
  }
}

static void stop_event_cb(lv_event_t* e) {
  if (control_get_state() == STATE_RUNNING || control_get_state() == STATE_JOG) {
    control_stop();
  }
}

static void jog_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    if (speed_get_direction() == DIR_CW) {
      control_start_jog_cw();
    } else {
      control_start_jog_ccw();
    }
  } else if (code == LV_EVENT_RELEASED) {
    control_stop();
  }
}

static void ccw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CCW);
  screen_main_update();
}

static void cw_event_cb(lv_event_t* e) {
  speed_set_direction(DIR_CW);
  screen_main_update();
}

static void speed_down_event_cb(lv_event_t* e) {
  if (speed_using_slider()) {
    float rpm = speed_get_target_rpm();
    if (rpm > 0.1f) speed_slider_set(rpm - 0.1f);
    screen_main_update();
  }
}

static void speed_up_event_cb(lv_event_t* e) {
  if (speed_using_slider()) {
    float rpm = speed_get_target_rpm();
    if (rpm < 5.0f) speed_slider_set(rpm + 0.1f);
    screen_main_update();
  }
}

static void menu_event_cb(lv_event_t* e) {
  screens_show(SCREEN_MENU);
}

// Helper to style buttons
static void style_side_panel_button(lv_obj_t* btn, const char* text) {
  lv_obj_set_size(btn, BTN_W, BTN_H);
  lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER, 0);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(label, COL_TEXT, 0);
  lv_obj_center(label);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create() {
  mainScreen = screenRoots[SCREEN_MAIN];
  lv_obj_set_style_bg_color(mainScreen, COL_BG, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // TOP STATUS BAR (y=0, h=40)
  // ─────────────────────────────────────────────────────────────────────────
  lv_obj_t* statusBar = lv_obj_create(mainScreen);
  lv_obj_set_size(statusBar, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_style_bg_color(statusBar, COL_BG, 0);
  lv_obj_set_style_border_width(statusBar, 0, 0);
  lv_obj_set_style_pad_all(statusBar, 0, 0);

  lv_obj_t* title = lv_label_create(statusBar);
  lv_label_set_text(title, "TIG ROTATOR");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEXT_DIM, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

  stateLabel = lv_label_create(statusBar);
  lv_label_set_text(stateLabel, "IDLE");
  lv_obj_set_style_text_font(stateLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(stateLabel, COL_TEXT_DIM, 0);
  lv_obj_align(stateLabel, LV_ALIGN_CENTER, 0, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // CENTRAL GAUGE (lv_meter)
  // ─────────────────────────────────────────────────────────────────────────
  rpmMeter = lv_meter_create(mainScreen);
  lv_obj_set_size(rpmMeter, 360, 360);
  lv_obj_align(rpmMeter, LV_ALIGN_CENTER, 0, 10);
  lv_obj_set_style_bg_color(rpmMeter, COL_BG, 0);
  lv_obj_set_style_border_width(rpmMeter, 0, 0);
  lv_obj_remove_style(rpmMeter, nullptr, LV_PART_INDICATOR);

  // Background tick scale (Dark grey ticks)
  scaleBase = lv_meter_add_scale(rpmMeter);
  lv_meter_set_scale_ticks(rpmMeter, scaleBase, 61, 2, 12, COL_BORDER);
  lv_meter_set_scale_range(rpmMeter, scaleBase, 0, 50, 270, 135);

  // Active track scale (Cyan ticks overlay)
  scaleActive = lv_meter_add_scale(rpmMeter);
  lv_meter_set_scale_ticks(rpmMeter, scaleActive, 61, 3, 16, COL_ACCENT);
  lv_meter_set_scale_range(rpmMeter, scaleActive, 0, 50, 270, 135);
  
  // Indicator arc for active track
  rpmIndicator = lv_meter_add_arc(rpmMeter, scaleActive, 10, COL_ACCENT, -5);
  lv_meter_set_indicator_start_value(rpmMeter, rpmIndicator, 0);
  lv_meter_set_indicator_end_value(rpmMeter, rpmIndicator, 0);

  // Large RPM Label in Center
  rpmLabel = lv_label_create(mainScreen);
  lv_label_set_text(rpmLabel, "0.0");
  lv_obj_set_style_text_font(rpmLabel, &lv_font_montserrat_48, 0); // Need big font
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_CENTER, 0, 10);

  lv_obj_t* unitLabel = lv_label_create(mainScreen);
  lv_label_set_text(unitLabel, "RPM");
  lv_obj_set_style_text_font(unitLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(unitLabel, COL_ACCENT, 0);
  lv_obj_align(unitLabel, LV_ALIGN_CENTER, 0, 50);

  // ─────────────────────────────────────────────────────────────────────────
  // SPEED CONTROL BUTTONS (Under the gauge)
  // ─────────────────────────────────────────────────────────────────────────
  lv_obj_t* speedDownBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(speedDownBtn, 90, 50);
  lv_obj_align(speedDownBtn, LV_ALIGN_BOTTOM_MID, -90, -10);
  lv_obj_set_style_bg_color(speedDownBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(speedDownBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(speedDownBtn, 1, 0);
  lv_obj_set_style_border_color(speedDownBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(speedDownBtn, speed_down_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* downLabel = lv_label_create(speedDownBtn);
  lv_label_set_text(downLabel, "- RPM");
  lv_obj_set_style_text_font(downLabel, &lv_font_montserrat_18, 0);
  lv_obj_center(downLabel);

  lv_obj_t* speedUpBtn = lv_btn_create(mainScreen);
  lv_obj_set_size(speedUpBtn, 90, 50);
  lv_obj_align(speedUpBtn, LV_ALIGN_BOTTOM_MID, 90, -10);
  lv_obj_set_style_bg_color(speedUpBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(speedUpBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(speedUpBtn, 1, 0);
  lv_obj_set_style_border_color(speedUpBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(speedUpBtn, speed_up_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* upLabel = lv_label_create(speedUpBtn);
  lv_label_set_text(upLabel, "+ RPM");
  lv_obj_set_style_text_font(upLabel, &lv_font_montserrat_18, 0);
  lv_obj_center(upLabel);

  // ─────────────────────────────────────────────────────────────────────────
  // LEFT PANEL - Power & Modes
  // ─────────────────────────────────────────────────────────────────────────
  startBtn = lv_btn_create(mainScreen);
  style_side_panel_button(startBtn, "ON");
  lv_obj_align(startBtn, LV_ALIGN_LEFT_MID, PAD_SCREEN, -100);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  stopBtn = lv_btn_create(mainScreen);
  style_side_panel_button(stopBtn, "STOP");
  lv_obj_align(stopBtn, LV_ALIGN_LEFT_MID, PAD_SCREEN, 0);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

  jogBtn = lv_btn_create(mainScreen);
  style_side_panel_button(jogBtn, "JOG");
  lv_obj_align(jogBtn, LV_ALIGN_LEFT_MID, PAD_SCREEN, 100);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  // ─────────────────────────────────────────────────────────────────────────
  // RIGHT PANEL - Direction & Menu
  // ─────────────────────────────────────────────────────────────────────────
  cwBtn = lv_btn_create(mainScreen);
  style_side_panel_button(cwBtn, "CW ^");
  lv_obj_align(cwBtn, LV_ALIGN_RIGHT_MID, -PAD_SCREEN, -100);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  ccwBtn = lv_btn_create(mainScreen);
  style_side_panel_button(ccwBtn, "CCW v");
  lv_obj_align(ccwBtn, LV_ALIGN_RIGHT_MID, -PAD_SCREEN, 0);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

  menuBtn = lv_btn_create(mainScreen);
  style_side_panel_button(menuBtn, "MENU");
  lv_obj_align(menuBtn, LV_ALIGN_RIGHT_MID, -PAD_SCREEN, 100);
  lv_obj_add_event_cb(menuBtn, menu_event_cb, LV_EVENT_CLICKED, nullptr);

  LOG_I("Screen main: dark theme layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
// Helper to set active "glow" state on side buttons
static void set_btn_active_state(lv_obj_t* btn, bool active, lv_color_t color) {
  if (active) {
    lv_obj_set_style_border_color(btn, color, 0);
    lv_obj_set_style_shadow_color(btn, color, 0);
    lv_obj_set_style_shadow_width(btn, 15, 0);
    lv_obj_set_style_shadow_spread(btn, 2, 0);
  } else {
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
  }
}

void screen_main_update() {
  if (!screens_is_active(SCREEN_MAIN)) return;

  SystemState state = control_get_state();
  Direction dir = speed_get_direction();
  float rpm = speed_get_actual_rpm();
  int32_t val = (int32_t)(rpm * 10.0f);

  // Update State Badge
  lv_label_set_text(stateLabel, state_badge_strings[state]);
  lv_obj_set_style_text_color(stateLabel, state_badge_colors[state], 0);

  // Update Meter
  lv_meter_set_indicator_end_value(rpmMeter, rpmIndicator, val);
  lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);
  
  // Because LVGL meter scales ticks statically based on indicator, 
  // we fake the "active ticks" by hiding the top ticks in the arc.
  // A cleaner approach is overlaying a clip area, but the simple arc indicator 
  // provides the vivid Cyan sweep over the grey base ticks, matching the design.

  // Update Button Glow States
  bool is_running = (state == STATE_RUNNING || state == STATE_PULSE || state == STATE_STEP || state == STATE_TIMER);
  set_btn_active_state(startBtn, is_running, COL_ACCENT);
  set_btn_active_state(stopBtn, (state == STATE_IDLE), COL_TEXT_DIM);

  set_btn_active_state(cwBtn, (dir == DIR_CW), COL_ACCENT);
  set_btn_active_state(ccwBtn, (dir == DIR_CCW), COL_ACCENT);
}
