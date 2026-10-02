// TIG Rotator Controller - Edit Step Settings Screen
// TARGET ANGLE, RPM, DIRECTION, REPEATS, DWELL TIME, COMPUTED values, CANCEL/SAVE

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/microstep.h"
#include "../../motor/speed.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* diameterLabel = nullptr;
static lv_obj_t* dirBtns[2] = {nullptr};
static lv_obj_t* repeatsLabel = nullptr;
static lv_obj_t* dwellLabel = nullptr;
static lv_obj_t* totalAngleLabel = nullptr;
static lv_obj_t* durationLabel = nullptr;
static lv_obj_t* stepsLabel = nullptr;

// Local edit state (not committed to preset until SAVE)
static float editAngle = 90.0f;
static float editRpm = 2.0f;
static float editDiameterMm = 0.0f;
static int editDir = 0;  // 0=CW, 1=CCW
static int editRepeats = 1;
static float editDwell = 0.0f;  // seconds

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_computed() {
  if (!totalAngleLabel) return;

  // Total angle = angle * repeats
  float totalAngle = editAngle * editRepeats;
  lv_label_set_text_fmt(totalAngleLabel, "%.0f deg", totalAngle);

  // Duration: time for all steps + dwell between steps
  // Step duration = angle / 360 / RPM * 60 seconds
  // Total duration = step_duration * repeats + dwell * (repeats - 1)
  if (editRpm > 0.01f) {
    float stepDuration = (editAngle / 360.0f) / editRpm * 60.0f;
    float totalDuration = stepDuration * editRepeats + editDwell * (editRepeats > 0 ? editRepeats - 1 : 0);
    if (durationLabel) {
      if (totalDuration >= 60.0f) {
        lv_label_set_text_fmt(durationLabel, "%.1f min", totalDuration / 60.0f);
      } else {
        lv_label_set_text_fmt(durationLabel, "%.1f sec", totalDuration);
      }
    }
  } else {
    if (durationLabel) lv_label_set_text(durationLabel, "--");
  }

  // Total motor steps must use the same calibration path as runtime step mode.
  long stepCount = angleToStepsForDiameter(editAngle, editDiameterMm);
  if (stepCount < 0) stepCount = -stepCount;
  long totalSteps = stepCount * editRepeats;
  if (stepsLabel) lv_label_set_text_fmt(stepsLabel, "%ld", totalSteps);
}

static void update_diameter_label() {
  if (!diameterLabel) return;
  if (editDiameterMm < 1.0f) {
    lv_label_set_text(diameterLabel, "DEFAULT");
  } else {
    lv_label_set_text_fmt(diameterLabel, "%.0f mm", editDiameterMm);
  }
}

static void update_dir_buttons() {
  for (int i = 0; i < 2; i++) {
    if (!dirBtns[i]) continue;
    bool isActive = (i == editDir);
    const UiBtnStyle ms = isActive ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(dirBtns[i], ms);
    lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(ms), 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void angle_adj_cb(lv_event_t* e) {
  float delta = (float)(intptr_t)lv_event_get_user_data(e);
  editAngle += delta;
  if (editAngle < 1.0f) editAngle = 1.0f;
  if (editAngle > 360.0f) editAngle = 360.0f;
  if (angleLabel) lv_label_set_text_fmt(angleLabel, "%.0f", editAngle);
  update_computed();
}

static void rpm_adj_cb(lv_event_t* e) {
  float delta = (float)(intptr_t)lv_event_get_user_data(e) * 0.1f;
  editRpm += delta;
  if (editRpm < MIN_RPM) editRpm = MIN_RPM;
  if (editRpm > speed_get_rpm_max()) editRpm = speed_get_rpm_max();
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
  update_computed();
}

static void dir_cb(lv_event_t* e) {
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  editDir = index;
  update_dir_buttons();
}

static void diameter_adj_cb(lv_event_t* e) {
  int delta = (int)(intptr_t)lv_event_get_user_data(e) * 10;
  if (editDiameterMm < 1.0f && delta > 0) {
    editDiameterMm = D_EMNE * 1000.0f;
  } else {
    editDiameterMm += delta;
  }
  if (editDiameterMm < 1.0f) editDiameterMm = 0.0f;
  if (editDiameterMm > 20000.0f) editDiameterMm = 20000.0f;
  update_diameter_label();
  update_computed();
}

static void repeats_adj_cb(lv_event_t* e) {
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  editRepeats += delta;
  if (editRepeats < 1) editRepeats = 1;
  if (editRepeats > 99) editRepeats = 99;
  if (repeatsLabel) lv_label_set_text_fmt(repeatsLabel, "%d", editRepeats);
  update_computed();
}

static void dwell_adj_cb(lv_event_t* e) {
  float delta = (float)(intptr_t)lv_event_get_user_data(e) * 0.5f;
  editDwell += delta;
  if (editDwell < 0.0f) editDwell = 0.0f;
  if (editDwell > 30.0f) editDwell = 30.0f;
  if (dwellLabel) lv_label_set_text_fmt(dwellLabel, "%.1f sec", editDwell);
  update_computed();
}

static void cancel_cb(lv_event_t* e) {
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void save_cb(lv_event_t* e) {
  Preset* p = screen_program_edit_get_preset();
  if (p) {
    p->step_angle = editAngle;
    p->rpm = editRpm;
    p->workpiece_diameter_mm = editDiameterMm;
    p->direction = (uint8_t)editDir;
    p->step_repeats = (uint16_t)editRepeats;
    p->step_dwell_sec = editDwell;
  }
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create a -/+ row with value display
// ───────────────────────────────────────────────────────────────────────────────
static void create_adj_row(lv_obj_t* parent, int y, const char* titleText, lv_obj_t** valueLabel,
                           lv_event_cb_t minusCb, lv_event_cb_t plusCb, void* minusData, void* plusData,
                           const lv_font_t* valueFont, lv_color_t valueColor, int valueW) {
  const int startX = PAD_X;
  const int btnW = 48;
  const int btnH = 36;
  const int valueGap = 8;

  // Title label
  lv_obj_t* titleLbl = lv_label_create(parent);
  lv_label_set_text(titleLbl, titleText);
  lv_obj_set_style_text_font(titleLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(titleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(titleLbl, startX, y);

  // Minus button
  lv_obj_t* minusBtn = lv_button_create(parent);
  lv_obj_set_size(minusBtn, btnW, btnH);
  lv_obj_set_pos(minusBtn, startX, y + 14);
  lv_obj_add_event_cb(minusBtn, minusCb, LV_EVENT_CLICKED, minusData);
  ui_btn_style_post(minusBtn, UI_BTN_NORMAL);

  lv_obj_t* minusLbl = lv_label_create(minusBtn);
  lv_label_set_text(minusLbl, "-");
  lv_obj_set_style_text_font(minusLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(minusLbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(minusLbl);

  // Value panel
  lv_obj_t* valuePanel = lv_obj_create(parent);
  lv_obj_set_size(valuePanel, valueW, btnH);
  lv_obj_set_pos(valuePanel, startX + btnW + valueGap, y + 14);
  lv_obj_set_style_bg_color(valuePanel, COL_PANEL_BG, 0);
  lv_obj_set_style_border_color(valuePanel, COL_BORDER, 0);
  lv_obj_set_style_border_width(valuePanel, 1, 0);
  lv_obj_set_style_radius(valuePanel, RADIUS_BTN, 0);
  lv_obj_set_style_pad_all(valuePanel, 0, 0);
  lv_obj_remove_flag(valuePanel, LV_OBJ_FLAG_SCROLLABLE);

  *valueLabel = lv_label_create(valuePanel);
  lv_obj_set_style_text_font(*valueLabel, valueFont, 0);
  lv_obj_set_style_text_color(*valueLabel, valueColor, 0);
  lv_obj_center(*valueLabel);

  // Plus button
  lv_obj_t* plusBtn = lv_button_create(parent);
  lv_obj_set_size(plusBtn, btnW, btnH);
  lv_obj_set_pos(plusBtn, startX + btnW + valueGap + valueW + valueGap, y + 14);
  lv_obj_add_event_cb(plusBtn, plusCb, LV_EVENT_CLICKED, plusData);
  ui_btn_style_post(plusBtn, UI_BTN_ACCENT);

  lv_obj_t* plusLbl = lv_label_create(plusBtn);
  lv_label_set_text(plusLbl, "+");
  lv_obj_set_style_text_font(plusLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(plusLbl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  lv_obj_center(plusLbl);
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create a separator line
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_separator(lv_obj_t* parent, int y) {
  lv_obj_t* sep = lv_obj_create(parent);
  lv_obj_set_size(sep, SCREEN_W - 2 * PAD_X, 1);
  lv_obj_set_pos(sep, PAD_X, y);
  lv_obj_set_style_bg_color(sep, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
  return sep;
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create a computed info row (label: value)
// ───────────────────────────────────────────────────────────────────────────────
static void create_info_row(lv_obj_t* parent, int y, const char* labelText, lv_obj_t** valueLabel) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, labelText);
  lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(lbl, PAD_X + 200, y);

  *valueLabel = lv_label_create(parent);
  lv_obj_set_style_text_font(*valueLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(*valueLabel, COL_TEXT, 0);
  lv_obj_set_pos(*valueLabel, PAD_X + 350, y);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_EDIT_STEP];
  lv_obj_clean(screen);
  Preset* p = screen_program_edit_get_preset();
  editAngle = p ? p->step_angle : 90.0f;
  editRpm = p ? p->rpm : 2.0f;
  editDiameterMm = p ? p->workpiece_diameter_mm : 0.0f;
  editDir = p ? p->direction : 0;
  editRepeats = p ? p->step_repeats : 1;
  editDwell = p ? p->step_dwell_sec : 0.0f;
  ui_create_header(screen, "Step settings", "PROGRAM EDIT", nullptr);
  angleLabel = ui_create_adjust_card(screen, 24, 86, 240, "ANGLE / degrees", angle_adj_cb);
  rpmLabel = ui_create_adjust_card(screen, 280, 86, 240, "SPEED / RPM", rpm_adj_cb);
  diameterLabel = ui_create_adjust_card(screen, 536, 86, 240, "PART DIAMETER", diameter_adj_cb);
  repeatsLabel = ui_create_adjust_card(screen, 24, 226, 240, "REPEATS", repeats_adj_cb);
  dwellLabel = ui_create_adjust_card(screen, 280, 226, 240, "DWELL / s", dwell_adj_cb);
  ui_create_post_card(screen, 536, 226, 240, 124);
  ui_create_text(screen, 550, 238, 212, "DIRECTION", FONT_NORMAL, COL_TEXT_DIM);
  dirBtns[0] = ui_create_btn(screen, 550, 282, 100, 56, "CW", FONT_BTN, UI_BTN_NORMAL, dir_cb, (void*)0);
  dirBtns[1] = ui_create_btn(screen, 662, 282, 100, 56, "CCW", FONT_BTN, UI_BTN_NORMAL, dir_cb, (void*)1);
  lv_label_set_text_fmt(angleLabel, "%.0f", editAngle);
  lv_label_set_text_fmt(rpmLabel, "%.3f", editRpm);
  lv_label_set_text_fmt(repeatsLabel, "%d", editRepeats);
  lv_label_set_text_fmt(dwellLabel, "%.1f sec", editDwell);
  ui_create_text(screen, 24, 366, 70, "TOTAL", FONT_SMALL, COL_TEXT_DIM);
  totalAngleLabel = ui_create_text(screen, 98, 362, 160, "", FONT_SUBTITLE, COL_TEXT);
  ui_create_text(screen, 280, 366, 72, "EST.", FONT_SMALL, COL_TEXT_DIM);
  durationLabel = ui_create_text(screen, 356, 362, 160, "", FONT_SUBTITLE, COL_TEXT);
  ui_create_text(screen, 536, 366, 70, "STEPS", FONT_SMALL, COL_TEXT_DIM);
  stepsLabel = ui_create_text(screen, 610, 362, 166, "", FONT_SUBTITLE, COL_TEXT);
  ui_create_btn(screen, 24, 408, 152, 56, "CANCEL", FONT_BTN, UI_BTN_NORMAL, cancel_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "SAVE", FONT_BTN, UI_BTN_ACCENT, save_cb, nullptr);
  update_diameter_label();
  update_dir_buttons();
  update_computed();
}

void screen_edit_step_invalidate_widgets() {
  angleLabel = nullptr;
  rpmLabel = nullptr;
  diameterLabel = nullptr;
  dirBtns[0] = nullptr;
  dirBtns[1] = nullptr;
  repeatsLabel = nullptr;
  dwellLabel = nullptr;
  totalAngleLabel = nullptr;
  durationLabel = nullptr;
  stepsLabel = nullptr;
}

void screen_edit_step_update() {
  if (!screens_is_active(SCREEN_EDIT_STEP)) return;

  if (angleLabel) lv_label_set_text_fmt(angleLabel, "%.0f", editAngle);
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
  update_diameter_label();
  if (repeatsLabel) lv_label_set_text_fmt(repeatsLabel, "%d", editRepeats);
  if (dwellLabel) lv_label_set_text_fmt(dwellLabel, "%.1f sec", editDwell);
  update_dir_buttons();
  update_computed();
}
