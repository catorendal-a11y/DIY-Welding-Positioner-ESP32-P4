// TIG Rotator Controller - Main Screen
// Pixel-perfect recreation of ui_mockup.svg using LVGL 8.x
// Layout: 800×480, dark industrial, cyan accents

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
static lv_obj_t* voltageLabel = nullptr;

// Center Gauge
static lv_obj_t* rpmMeter = nullptr;
static lv_meter_scale_t* scaleBase = nullptr;
static lv_meter_scale_t* scaleActive = nullptr;
static lv_meter_indicator_t* rpmIndicator = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmUnitLabel = nullptr;

// Side Panel Buttons
static lv_obj_t* startBtn = nullptr;
static lv_obj_t* stopBtn = nullptr;
static lv_obj_t* jogBtn = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;
static lv_obj_t* menuBtn = nullptr;

// Speed Buttons
static lv_obj_t* speedDownBtn = nullptr;
static lv_obj_t* speedUpBtn = nullptr;

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
  float rpm = speed_get_target_rpm();
  if (rpm > 0.1f) speed_slider_set(rpm - 0.1f);
  screen_main_update();
}

static void speed_up_event_cb(lv_event_t* e) {
  float rpm = speed_get_target_rpm();
  if (rpm < MAX_RPM) speed_slider_set(rpm + 0.1f);
  screen_main_update();
}

static void menu_event_cb(lv_event_t* e) {
  screens_show(SCREEN_MENU);
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: Create a side-panel button at absolute SVG coordinates
// SVG uses translate(x, y) for each button group
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_svg_button(lv_obj_t* parent, int16_t x, int16_t y,
                                    int16_t w, int16_t h, int16_t radius,
                                    const char* text, bool active) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);

  // SVG active state: fill=#0c1a1c, stroke=#00E5FF, stroke-width=2
  // SVG inactive state: fill=#121212, stroke=#222, stroke-width=1
  if (active) {
    lv_obj_set_style_bg_color(btn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
  } else {
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
  }
  lv_obj_set_style_radius(btn, radius, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);

  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  // SVG active text: font-size=20, font-weight=bold, fill=#FFF
  // SVG inactive text: font-size=18, fill=#555 or #FFF
  if (active) {
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
  } else {
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
  }
  lv_obj_center(label);

  return btn;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — Pixel-perfect match to ui_mockup.svg
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create() {
  mainScreen = screenRoots[SCREEN_MAIN];
  lv_obj_set_style_bg_color(mainScreen, COL_BG, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // TOP STATUS BAR
  // SVG: "TIG ROTATOR" at (30,35) fill=#444, "RUNNING" at (400,35) fill=#00E676
  //      "24.5V" at (770,35) fill=#444
  // ─────────────────────────────────────────────────────────────────────────
  lv_obj_t* title = lv_label_create(mainScreen);
  lv_label_set_text(title, "TIG ROTATOR");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(title, 30, 15);   // SVG: x=30 y=35 (baseline), adjust for top-left anchor

  stateLabel = lv_label_create(mainScreen);
  lv_label_set_text(stateLabel, "IDLE");
  lv_obj_set_style_text_font(stateLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(stateLabel, COL_TEXT_DIM, 0);
  lv_obj_align(stateLabel, LV_ALIGN_TOP_MID, 0, 15);

  voltageLabel = lv_label_create(mainScreen);
  lv_label_set_text(voltageLabel, "24.5V");
  lv_obj_set_style_text_font(voltageLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(voltageLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(voltageLabel, 735, 15);  // SVG: x=770 anchor=end, approx -35px for text width

  // ─────────────────────────────────────────────────────────────────────────
  // CENTRAL GAUGE (SVG: arc at center 400,240, radius=160)
  // ─────────────────────────────────────────────────────────────────────────
  rpmMeter = lv_meter_create(mainScreen);
  lv_obj_set_size(rpmMeter, GAUGE_R * 2 + 20, GAUGE_R * 2 + 20);  // 340x340
  // Center the meter visually at SVG (400, 240)
  lv_obj_set_pos(rpmMeter, GAUGE_CX - GAUGE_R - 10, GAUGE_CY - GAUGE_R - 10);
  lv_obj_set_style_bg_color(rpmMeter, COL_BG, 0);
  lv_obj_set_style_bg_opa(rpmMeter, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rpmMeter, 0, 0);
  lv_obj_set_style_pad_all(rpmMeter, 10, 0);
  lv_obj_remove_style(rpmMeter, nullptr, LV_PART_INDICATOR);

  // Background arc scale (SVG: stroke=#111, stroke-width=15)
  scaleBase = lv_meter_add_scale(rpmMeter);
  lv_meter_set_scale_ticks(rpmMeter, scaleBase, 61, 2, 12, COL_GAUGE_TICK);
  lv_meter_set_scale_range(rpmMeter, scaleBase, 0, 60, 270, 135);

  // Active sweep scale (SVG: stroke=#00E5FF, stroke-width=12, with glow filter)
  scaleActive = lv_meter_add_scale(rpmMeter);
  lv_meter_set_scale_ticks(rpmMeter, scaleActive, 61, 3, 16, COL_ACCENT);
  lv_meter_set_scale_range(rpmMeter, scaleActive, 0, 60, 270, 135);

  // Arc indicator (the cyan sweep)
  rpmIndicator = lv_meter_add_arc(rpmMeter, scaleActive, 12, COL_ACCENT, -5);
  lv_meter_set_indicator_start_value(rpmMeter, rpmIndicator, 0);
  lv_meter_set_indicator_end_value(rpmMeter, rpmIndicator, 0);

  // ─────────────────────────────────────────────────────────────────────────
  // LARGE RPM VALUE (SVG: "1.8" at (400,260) font-size=100 fill=#FFF bold)
  // ─────────────────────────────────────────────────────────────────────────
  rpmLabel = lv_label_create(mainScreen);
  lv_label_set_text(rpmLabel, "0.0");
  // lv_font_montserrat_48 is the largest commonly included Montserrat in LVGL 8
  // The SVG calls for font-size=100, so we use 48 which is the biggest built-in
  lv_obj_set_style_text_font(rpmLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  // SVG: y=260 (baseline from top), center horizontally at x=400
  lv_obj_set_pos(rpmLabel, GAUGE_CX, GAUGE_CY - 15);
  lv_obj_set_style_text_align(rpmLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, 0, GAUGE_CY - 40);

  // "RPM" unit label (SVG: (400,305) font-size=22 fill=#00E5FF bold, letter-spacing=3)
  rpmUnitLabel = lv_label_create(mainScreen);
  lv_label_set_text(rpmUnitLabel, "R P M");
  lv_obj_set_style_text_font(rpmUnitLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(rpmUnitLabel, COL_ACCENT, 0);
  lv_obj_set_style_text_letter_space(rpmUnitLabel, 4, 0);
  lv_obj_align(rpmUnitLabel, LV_ALIGN_TOP_MID, 0, GAUGE_CY + 20);

  // ─────────────────────────────────────────────────────────────────────────
  // LEFT SIDE PANEL BUTTONS
  // SVG coordinates: translate(35, 120/210/300), width=130, height=75, rx=12
  // ─────────────────────────────────────────────────────────────────────────
  startBtn = create_svg_button(mainScreen, LEFT_BTN_X, BTN_ROW_1_Y,
                                BTN_W, BTN_H, RADIUS_BTN, "ON", true);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  stopBtn = create_svg_button(mainScreen, LEFT_BTN_X, BTN_ROW_2_Y,
                               BTN_W, BTN_H, RADIUS_BTN, "STOP", false);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

  jogBtn = create_svg_button(mainScreen, LEFT_BTN_X, BTN_ROW_3_Y,
                              BTN_W, BTN_H, RADIUS_BTN, "JOG", false);
  lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

  // ─────────────────────────────────────────────────────────────────────────
  // RIGHT SIDE PANEL BUTTONS
  // SVG coordinates: translate(635, 120/210/300), width=130, height=75, rx=12
  // ─────────────────────────────────────────────────────────────────────────
  cwBtn = create_svg_button(mainScreen, RIGHT_BTN_X, BTN_ROW_1_Y,
                             BTN_W, BTN_H, RADIUS_BTN, LV_SYMBOL_UP " CW", true);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  ccwBtn = create_svg_button(mainScreen, RIGHT_BTN_X, BTN_ROW_2_Y,
                              BTN_W, BTN_H, RADIUS_BTN, LV_SYMBOL_DOWN " CCW", false);
  lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

  menuBtn = create_svg_button(mainScreen, RIGHT_BTN_X, BTN_ROW_3_Y,
                               BTN_W, BTN_H, RADIUS_BTN, "MENU", false);
  lv_obj_add_event_cb(menuBtn, menu_event_cb, LV_EVENT_CLICKED, nullptr);

  // ─────────────────────────────────────────────────────────────────────────
  // SPEED CONTROL BUTTONS (under gauge)
  // SVG: translate(295/410, 400), width=95, height=55, rx=10
  // ─────────────────────────────────────────────────────────────────────────
  speedDownBtn = create_svg_button(mainScreen, SPD_BTN_L_X, SPD_BTN_Y,
                                    BTN_W_SM, BTN_H_SM, RADIUS_BTN_SM, "- RPM", false);
  // Override border to match SVG #333
  lv_obj_set_style_border_color(speedDownBtn, COL_BORDER_SPD, 0);
  lv_obj_add_event_cb(speedDownBtn, speed_down_event_cb, LV_EVENT_CLICKED, nullptr);

  speedUpBtn = create_svg_button(mainScreen, SPD_BTN_R_X, SPD_BTN_Y,
                                  BTN_W_SM, BTN_H_SM, RADIUS_BTN_SM, "+ RPM", false);
  lv_obj_set_style_border_color(speedUpBtn, COL_BORDER_SPD, 0);
  lv_obj_add_event_cb(speedUpBtn, speed_up_event_cb, LV_EVENT_CLICKED, nullptr);

  LOG_I("Screen main: SVG-matched layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE — Dynamic state-driven visual updates
// ───────────────────────────────────────────────────────────────────────────────

// Helper: Set button to SVG active or inactive visual state
static void set_btn_svg_state(lv_obj_t* btn, bool active) {
  if (active) {
    // SVG active: fill=#0c1a1c, stroke=#00E5FF 2px, shadow glow
    lv_obj_set_style_bg_color(btn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_shadow_color(btn, COL_ACCENT, 0);
    lv_obj_set_style_shadow_width(btn, 20, 0);
    lv_obj_set_style_shadow_spread(btn, 2, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
    // Update child label to white bold
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) {
      lv_obj_set_style_text_color(label, COL_TEXT, 0);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    }
  } else {
    // SVG inactive: fill=#121212, stroke=#222 1px, no shadow
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    // Update child label to dimmed
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) {
      lv_obj_set_style_text_color(label, COL_TEXT_DIM, 0);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    }
  }
}

void screen_main_update() {
  if (!screens_is_active(SCREEN_MAIN)) return;

  SystemState state = control_get_state();
  Direction dir = speed_get_direction();
  float rpm = speed_get_actual_rpm();
  int32_t val = (int32_t)(rpm * 10.0f);

  // Update State Badge (SVG: center top, green when running)
  lv_label_set_text(stateLabel, state_badge_strings[state]);
  lv_obj_set_style_text_color(stateLabel, state_badge_colors[state], 0);

  // Update Gauge Arc
  lv_meter_set_indicator_end_value(rpmMeter, rpmIndicator, val);
  lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);

  // Dynamic Button States (matching SVG active/inactive patterns)
  bool is_running = (state == STATE_RUNNING || state == STATE_PULSE ||
                     state == STATE_STEP || state == STATE_TIMER);

  set_btn_svg_state(startBtn, is_running);
  set_btn_svg_state(stopBtn, !is_running && state != STATE_IDLE);

  // Direction buttons reflect current direction
  set_btn_svg_state(cwBtn, (dir == DIR_CW));
  set_btn_svg_state(ccwBtn, (dir == DIR_CCW));
}
