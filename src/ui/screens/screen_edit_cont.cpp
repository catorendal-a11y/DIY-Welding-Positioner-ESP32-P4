// TIG Rotator Controller - Edit Continuous Settings Screen
// Brutalist v2.0 design matching new_ui.svg SCREEN_EDIT_CONT

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
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
    lv_label_set_text_fmt(rpmValueLabel, "%.1f", editRpm);
  }
  if (rpmBar) {
    lv_bar_set_value(rpmBar, (int32_t)(editRpm * 1000.0f + 0.5f), LV_ANIM_OFF);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
    screen_program_edit_update_ui();
    screens_show(SCREEN_PROGRAM_EDIT);
}

static void rpm_adj_cb(lv_event_t* e) {
  intptr_t delta = (intptr_t)lv_event_get_user_data(e);
  editRpm += (float)delta * 0.1f;  // delta in tenths: -1→-0.1, -10→-1.0, etc.
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

static void cancel_cb(lv_event_t* e) {
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void save_cb(lv_event_t* e) {
  Preset* p = screen_program_edit_get_preset();
  if (p) {
    p->rpm = editRpm;
    p->direction = directionCW ? 0 : 1;
    p->cont_soft_start = softStartEnabled ? 1 : 0;
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
    lv_obj_set_style_bg_color(screen, COL_BG, 0);

    Preset* p = screen_program_edit_get_preset();
    editRpm = p ? p->rpm : 1.0f;
    directionCW = p ? (p->direction == 0) : true;
    softStartEnabled = p ? (p->cont_soft_start != 0) : false;
    float rpm = editRpm;

    // ── Header (POST 38px) ──
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // Title (SVG: x=12, "EDIT CONTINUOUS")
    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "EDIT CONTINUOUS");
    lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(title, COL_ACCENT, 0);
    lv_obj_set_pos(title, 12, 11);

    // [ESC] button at right of header
    lv_obj_t* escBtn = lv_button_create(header);
    lv_obj_set_size(escBtn, 60, 24);
    lv_obj_set_pos(escBtn, SCREEN_W - 60 - PAD_X, 7);
    ui_btn_style_post(escBtn, UI_BTN_NORMAL);
    lv_obj_add_event_cb(escBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* escLbl = lv_label_create(escBtn);
    lv_label_set_text(escLbl, "[ESC]");
    lv_obj_set_style_text_font(escLbl, FONT_SMALL, 0);
    lv_obj_set_style_text_color(escLbl, COL_TEXT_DIM, 0);
    lv_obj_center(escLbl);

    // ── Separator line under header ──
    lv_obj_t* line1 = lv_obj_create(screen);
    lv_obj_set_size(line1, SCREEN_W, 1);
    lv_obj_set_pos(line1, 0, HEADER_H);
    lv_obj_set_style_bg_color(line1, COL_BORDER, 0);
    lv_obj_set_style_pad_all(line1, 0, 0);
    lv_obj_set_style_border_width(line1, 0, 0);
    lv_obj_set_style_radius(line1, 0, 0);
    lv_obj_remove_flag(line1, LV_OBJ_FLAG_SCROLLABLE);

    ui_add_post_header_accent(screen);

    // ── Large RPM value (SVG: y=58, huge font, #FF9500 bold) ──
    ui_create_post_card(screen, 12, 46, 776, 124);
    ui_create_post_card(screen, 12, 182, 776, 70);
    ui_create_post_row(screen, 12, 256, 776, 34);

    rpmValueLabel = lv_label_create(screen);
    lv_label_set_text_fmt(rpmValueLabel, "%.1f", rpm);
    lv_obj_set_style_text_font(rpmValueLabel, FONT_HUGE, 0);
    lv_obj_set_style_text_color(rpmValueLabel, COL_ACCENT, 0);
    lv_obj_set_pos(rpmValueLabel, 20, 46);

    // "RPM" unit label next to value
    lv_obj_t* rpmUnit = lv_label_create(screen);
    lv_label_set_text(rpmUnit, "RPM");
    lv_obj_set_style_text_font(rpmUnit, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(rpmUnit, COL_TEXT_DIM, 0);
    lv_obj_set_pos(rpmUnit, 20, 90);

    // ── Progress bar (SVG: 20,142,760,3) ──
    rpmBar = lv_bar_create(screen);
    lv_obj_set_size(rpmBar, 760, 3);
    lv_obj_set_pos(rpmBar, 20, 106);
    lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, 0);
    lv_obj_set_style_border_width(rpmBar, 0, 0);
    lv_obj_set_style_radius(rpmBar, 1, 0);
    {
      float mx = speed_get_rpm_max();
      lv_bar_set_range(rpmBar, (int32_t)(MIN_RPM * 1000.0f + 0.5f), (int32_t)(mx * 1000.0f + 0.5f));
    }
    lv_bar_set_value(rpmBar, (int32_t)(rpm * 1000.0f + 0.5f), LV_ANIM_OFF);

    // Scale marks below progress bar (derived from MIN_RPM / speed_get_rpm_max())
    char s0[12], s1[12], s2[12], s3[12];
    {
      float mx = speed_get_rpm_max();
      snprintf(s0, sizeof(s0), "%.3f", (double)MIN_RPM);
      snprintf(s1, sizeof(s1), "%.3f", (double)(MIN_RPM + (mx - MIN_RPM) * 0.33f));
      snprintf(s2, sizeof(s2), "%.3f", (double)(MIN_RPM + (mx - MIN_RPM) * 0.66f));
      snprintf(s3, sizeof(s3), "%.3f", (double)mx);
    }
    const char* scaleLabels[] = {s0, s1, s2, s3};
    const int scaleX[] = {20, 265, 510, 740};
    for (int i = 0; i < 4; i++) {
      lv_obj_t* tick = lv_label_create(screen);
      lv_label_set_text(tick, scaleLabels[i]);
      lv_obj_set_style_text_font(tick, FONT_TINY, 0);
      lv_obj_set_style_text_color(tick, COL_TEXT_VDIM, 0);
      lv_obj_set_pos(tick, scaleX[i], 111);
    }

    // ── 4 adjustment buttons (SVG: two rows or one row, -0.1/+0.1/-1.0/+1.0) ──
    // Row 1: -0.1 and +0.1 (SVG: 180x42 each)
    // Row 2: -1.0 and +1.0 (SVG: 190x42 each)
    const int adjY1 = 124;
    const int adjY2 = 170;
    const int adjBtnW = 180;
    const int adjBtnW2 = 190;
    const int adjBtnH = 42;

    // -0.1 button
    lv_obj_t* m01Btn = lv_button_create(screen);
    lv_obj_set_size(m01Btn, adjBtnW, adjBtnH);
    lv_obj_set_pos(m01Btn, 20, adjY1);
    ui_btn_style_post(m01Btn, UI_BTN_NORMAL);
    lv_obj_add_event_cb(m01Btn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

    lv_obj_t* m01Lbl = lv_label_create(m01Btn);
    lv_label_set_text(m01Lbl, "- 0.1");
    lv_obj_set_style_text_font(m01Lbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(m01Lbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
    lv_obj_center(m01Lbl);

    // +0.1 button
    lv_obj_t* p01Btn = lv_button_create(screen);
    lv_obj_set_size(p01Btn, adjBtnW, adjBtnH);
    lv_obj_set_pos(p01Btn, 210, adjY1);
    ui_btn_style_post(p01Btn, UI_BTN_ACCENT);
    lv_obj_add_event_cb(p01Btn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t* p01Lbl = lv_label_create(p01Btn);
    lv_label_set_text(p01Lbl, "+ 0.1");
    lv_obj_set_style_text_font(p01Lbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(p01Lbl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
    lv_obj_center(p01Lbl);

    // -1.0 button
    lv_obj_t* m10Btn = lv_button_create(screen);
    lv_obj_set_size(m10Btn, adjBtnW2, adjBtnH);
    lv_obj_set_pos(m10Btn, 400, adjY1);
    ui_btn_style_post(m10Btn, UI_BTN_NORMAL);
    lv_obj_add_event_cb(m10Btn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-10));

    lv_obj_t* m10Lbl = lv_label_create(m10Btn);
    lv_label_set_text(m10Lbl, "- 1.0");
    lv_obj_set_style_text_font(m10Lbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(m10Lbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
    lv_obj_center(m10Lbl);

    // +1.0 button
    lv_obj_t* p10Btn = lv_button_create(screen);
    lv_obj_set_size(p10Btn, adjBtnW2, adjBtnH);
    lv_obj_set_pos(p10Btn, 600, adjY1);
    ui_btn_style_post(p10Btn, UI_BTN_ACCENT);
    lv_obj_add_event_cb(p10Btn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)10);

    lv_obj_t* p10Lbl = lv_label_create(p10Btn);
    lv_label_set_text(p10Lbl, "+ 1.0");
    lv_obj_set_style_text_font(p10Lbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(p10Lbl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
    lv_obj_center(p10Lbl);

    // ── Separator at y=170 ──
    lv_obj_t* line2 = lv_obj_create(screen);
    lv_obj_set_size(line2, SCREEN_W, 1);
    lv_obj_set_pos(line2, 0, 176);
    lv_obj_set_style_bg_color(line2, COL_SEPARATOR, 0);
    lv_obj_set_style_pad_all(line2, 0, 0);
    lv_obj_set_style_border_width(line2, 0, 0);
    lv_obj_set_style_radius(line2, 0, 0);
    lv_obj_remove_flag(line2, LV_OBJ_FLAG_SCROLLABLE);

    // ── DIRECTION section (SVG: y=186) ──
    lv_obj_t* dirLabel = lv_label_create(screen);
    lv_label_set_text(dirLabel, "DIRECTION");
    lv_obj_set_style_text_font(dirLabel, FONT_TINY, 0);
    lv_obj_set_style_text_color(dirLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(dirLabel, 20, 186);

    // CW button (SVG: 200x42, toggle style)
    cwBtn = lv_button_create(screen);
    lv_obj_set_size(cwBtn, 200, 42);
    lv_obj_set_pos(cwBtn, 20, 202);
    lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cwLbl = lv_label_create(cwBtn);
    lv_label_set_text(cwLbl, "CW");
    lv_obj_set_style_text_font(cwLbl, FONT_NORMAL, 0);
    lv_obj_center(cwLbl);

    // CCW button
    ccwBtn = lv_button_create(screen);
    lv_obj_set_size(ccwBtn, 200, 42);
    lv_obj_set_pos(ccwBtn, 230, 202);
    lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* ccwLbl = lv_label_create(ccwBtn);
    lv_label_set_text(ccwLbl, "CCW");
    lv_obj_set_style_text_font(ccwLbl, FONT_NORMAL, 0);
    lv_obj_center(ccwLbl);

    // ── SOFT START section ──
    lv_obj_t* ssLabel = lv_label_create(screen);
    lv_label_set_text(ssLabel, "SOFT START");
    lv_obj_set_style_text_font(ssLabel, FONT_TINY, 0);
    lv_obj_set_style_text_color(ssLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(ssLabel, 480, 186);

    // ON button (SVG: 140x42)
    ssOnBtn = lv_button_create(screen);
    lv_obj_set_size(ssOnBtn, 140, 42);
    lv_obj_set_pos(ssOnBtn, 480, 202);
    lv_obj_add_event_cb(ssOnBtn, ss_on_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* ssOnLbl = lv_label_create(ssOnBtn);
    lv_label_set_text(ssOnLbl, "ON");
    lv_obj_set_style_text_font(ssOnLbl, FONT_NORMAL, 0);
    lv_obj_center(ssOnLbl);

    // OFF button
    ssOffBtn = lv_button_create(screen);
    lv_obj_set_size(ssOffBtn, 140, 42);
    lv_obj_set_pos(ssOffBtn, 628, 202);
    lv_obj_add_event_cb(ssOffBtn, ss_off_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* ssOffLbl = lv_label_create(ssOffBtn);
    lv_label_set_text(ssOffLbl, "OFF");
    lv_obj_set_style_text_font(ssOffLbl, FONT_NORMAL, 0);
    lv_obj_center(ssOffLbl);

    // Apply initial toggle styles
    restyle_direction();
    restyle_soft_start();

    ui_create_separator_line(screen, 0, 254, SCREEN_W, COL_SEPARATOR);

    // ── Info line (decorative; gear matches config GEAR_RATIO) ──
    lv_obj_t* infoLabel = lv_label_create(screen);
    char infoBuf[72];
    snprintf(infoBuf, sizeof(infoBuf), "ACCEL . STEPS/S 3556 . GEAR 1:%.0f . MICRO 8x", (double)GEAR_RATIO);
    lv_label_set_text(infoLabel, infoBuf);
    lv_obj_set_style_text_font(infoLabel, FONT_TINY, 0);
    lv_obj_set_style_text_color(infoLabel, COL_TEXT_VDIM, 0);
    lv_obj_set_pos(infoLabel, 20, 262);

    ui_create_btn(screen, 120, 406, 260, 52, "CANCEL", FONT_NORMAL, UI_BTN_NORMAL, cancel_cb, nullptr);
    ui_create_btn(screen, 420, 406, 260, 52, "SAVE", FONT_NORMAL, UI_BTN_ACCENT, save_cb, nullptr);

    LOG_I("Screen edit cont: v2.0 layout created");
}

void screen_edit_cont_invalidate_widgets() {
  rpmValueLabel = nullptr;
  rpmBar = nullptr;
  cwBtn = nullptr;
  ccwBtn = nullptr;
  ssOnBtn = nullptr;
  ssOffBtn = nullptr;
}

void screen_edit_cont_update() {
    Preset* p = screen_program_edit_get_preset();
    editRpm = p ? p->rpm : 1.0f;
    directionCW = p ? (p->direction == 0) : true;
    softStartEnabled = p ? (p->cont_soft_start != 0) : false;
    update_rpm_display();
    restyle_direction();
    restyle_soft_start();
}
