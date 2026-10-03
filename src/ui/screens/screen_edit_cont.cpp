// TIG Rotator Controller - Edit Continuous Settings Screen
// Brutalist v2.0 design matching new_ui.svg SCREEN_EDIT_CONT

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static float editRpm = 1.0f;
static bool softStartEnabled = false;
static uint32_t autoStopSeconds = 0;
static lv_obj_t* autoStopLabel = nullptr;
static void auto_stop_cb(lv_event_t* e) {
  int next = (int)autoStopSeconds + (int)(intptr_t)lv_event_get_user_data(e) * 5;
  autoStopSeconds = constrain(next, 0, 3600);
  if (autoStopSeconds)
    lv_label_set_text_fmt(autoStopLabel, "%lu sec", (unsigned long)autoStopSeconds);
  else
    lv_label_set_text(autoStopLabel, "OFF / continuous");
}
static bool directionCW = true;  // true = CW, false = CCW

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* rpmValueLabel = nullptr;
static lv_obj_t* rpmBar = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* ccwBtn = nullptr;
static lv_obj_t* ssOnBtn = nullptr;
static lv_obj_t* ssOffBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void restyle_direction() {
  if (cwBtn) {
    const UiBtnStyle s = directionCW ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(cwBtn, s);
    lv_obj_t* lbl = lv_obj_get_child(cwBtn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(s), 0);
  }
  if (ccwBtn) {
    const UiBtnStyle s = directionCW ? UI_BTN_NORMAL : UI_BTN_ACCENT;
    ui_btn_style_post(ccwBtn, s);
    lv_obj_t* lbl = lv_obj_get_child(ccwBtn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(s), 0);
  }
}

static void restyle_soft_start() {
  if (ssOnBtn) {
    const UiBtnStyle s = softStartEnabled ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(ssOnBtn, s);
    lv_obj_t* lbl = lv_obj_get_child(ssOnBtn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(s), 0);
  }
  if (ssOffBtn) {
    const UiBtnStyle s = softStartEnabled ? UI_BTN_NORMAL : UI_BTN_ACCENT;
    ui_btn_style_post(ssOffBtn, s);
    lv_obj_t* lbl = lv_obj_get_child(ssOffBtn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(s), 0);
  }
}

static void update_rpm_display() {
  if (rpmValueLabel) {
    ui_set_rpm(rpmValueLabel, editRpm);
  }
  if (rpmBar) {
    lv_bar_set_value(rpmBar, (int32_t)(editRpm * 1000.0f + 0.5f), LV_ANIM_OFF);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────


static void rpm_adj_cb(lv_event_t* e) {
  intptr_t delta = (intptr_t)lv_event_get_user_data(e);
  editRpm += (float)delta * ui_rpm_increment(editRpm);
  float mx = speed_get_rpm_max();
  if (editRpm < MIN_RPM) editRpm = MIN_RPM;
  if (editRpm > mx) editRpm = mx;

  update_rpm_display();
}

static void cw_event_cb(lv_event_t* e) {
  directionCW = true;
  restyle_direction();
}

static void ccw_event_cb(lv_event_t* e) {
  directionCW = false;
  restyle_direction();
}

static void ss_on_event_cb(lv_event_t* e) {
  softStartEnabled = true;
  restyle_soft_start();
}

static void ss_off_event_cb(lv_event_t* e) {
  softStartEnabled = false;
  restyle_soft_start();
}

static void cancel_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAM_EDIT); }

static void save_cb(lv_event_t* e) {
  Preset* p = screen_program_edit_get_preset();
  if (p) {
    p->rpm = editRpm;
    p->direction = speed_resolve_direction(directionCW ? DIR_CW : DIR_CCW);
    p->cont_soft_start = softStartEnabled ? 1 : 0;
    p->timer_auto_stop = autoStopSeconds > 0;
    p->timer_ms = autoStopSeconds * 1000u;
  }
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE -- new_ui.svg SCREEN_EDIT_CONT
// Header: "EDIT CONTINUOUS" + BACK button
// Large RPM (y=58): "2.0" in huge font, #FF9500 bold
// Progress bar (20,142,760,3) with scale marks
// 4 adjustment buttons: -0.1, +0.1, -1.0, +1.0
// Separator at y=240
// DIRECTION (y=266): CW/CCW toggle (200x42 each)
// SOFT START: ON/OFF toggle (140x42 each)
// Separator at y=340
// Info: decorative line; gear ratio from GEAR_RATIO; accel/steps are illustrative.
// CANCEL + SAVE buttons
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_cont_create() {
  lv_obj_t* screen = screenRoots[SCREEN_EDIT_CONT];
  lv_obj_clean(screen);
  Preset* p = screen_program_edit_get_preset();
  editRpm = p ? p->rpm : 1.0f;
  directionCW = !p || speed_resolve_direction((Direction)p->direction) == DIR_CW;
  softStartEnabled = p && p->cont_soft_start;
  ui_create_header(screen, "Continuous settings", "PROGRAM EDIT", nullptr);
  rpmBar = nullptr;
  rpmValueLabel = ui_create_adjust_card(screen, 24, 94, 368, "TARGET SPEED / RPM", rpm_adj_cb);
  ui_highlight_value_card(rpmValueLabel);
  ui_create_text(screen, 424, 94, 352, "DIRECTION", FONT_SUBTITLE, COL_TEXT_DIM);
  cwBtn = ui_create_btn(screen, 424, 128, 168, 64, "CW", FONT_BTN, UI_BTN_NORMAL, cw_event_cb, nullptr);
  ccwBtn = ui_create_btn(screen, 608, 128, 168, 64, "CCW", FONT_BTN, UI_BTN_NORMAL, ccw_event_cb, nullptr);
  autoStopSeconds = p && p->timer_auto_stop ? p->timer_ms / 1000u : 0;
  autoStopLabel = ui_create_adjust_card(screen, 408, 226, 368, "AUTO STOP / 0 = OFF", auto_stop_cb);
  if (autoStopSeconds)
    lv_label_set_text_fmt(autoStopLabel, "%lu sec", (unsigned long)autoStopSeconds);
  else
    lv_label_set_text(autoStopLabel, "OFF / continuous");
  ui_create_post_card(screen, 24, 226, 368, 124);
  ui_create_text(screen, 42, 240, 200, "SOFT START", FONT_LARGE, COL_TEXT);
  ssOnBtn = ui_create_btn(screen, 38, 282, 160, 56, "ON", FONT_BTN, UI_BTN_NORMAL, ss_on_event_cb, nullptr);
  ssOffBtn =
      ui_create_btn(screen, 212, 282, 166, 56, "OFF", FONT_BTN, UI_BTN_NORMAL, ss_off_event_cb, nullptr);
  ui_create_text(screen, 24, 360, 752, "Settings are applied to the program when you save.", FONT_SUBTITLE,
                 COL_TEXT_DIM);
  ui_create_btn(screen, 24, 408, 152, 56, "CANCEL", FONT_BTN, UI_BTN_NORMAL, cancel_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "SAVE", FONT_BTN, UI_BTN_ACCENT, save_cb, nullptr);
  update_rpm_display();
  restyle_direction();
  restyle_soft_start();
}

void screen_edit_cont_invalidate_widgets() {
  autoStopLabel = nullptr;
  rpmValueLabel = nullptr;
  rpmBar = nullptr;
  cwBtn = nullptr;
  ccwBtn = nullptr;
  ssOnBtn = nullptr;
  ssOffBtn = nullptr;
}

void screen_edit_cont_update() {
  // Draft is loaded on entry, never overwritten by a periodic refresh.
  update_rpm_display();
  restyle_direction();
  restyle_soft_start();
}
