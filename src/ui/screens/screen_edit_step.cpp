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
static lv_obj_t* dirBtns[2] = {nullptr};
static lv_obj_t* repeatsLabel = nullptr;
static lv_obj_t* dwellLabel = nullptr;
static lv_obj_t* totalAngleLabel = nullptr;
static lv_obj_t* durationLabel = nullptr;
static lv_obj_t* stepsLabel = nullptr;

// Local edit state (not committed to preset until SAVE)
static float editAngle = 90.0f;
static float editRpm = 2.0f;
static int editDir = 0;       // 0=CW, 1=CCW
static int editRepeats = 1;
static float editDwell = 0.0f; // seconds

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

  // Total steps
  long totalSteps = (long)(editAngle * editRepeats * ((float)microstep_get_steps_per_rev() * GEAR_RATIO / 360.0f));
  if (stepsLabel) lv_label_set_text_fmt(stepsLabel, "%ld", totalSteps);
}

static void update_dir_buttons() {
  for (int i = 0; i < 2; i++) {
    if (!dirBtns[i]) continue;
    bool isActive = (i == editDir);
    lv_obj_set_style_bg_color(dirBtns[i], isActive ? COL_BTN_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_color(dirBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_border_width(dirBtns[i], isActive ? 2 : 1, 0);
    lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
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
static void create_adj_row(lv_obj_t* parent, int y,
                           const char* titleText, lv_obj_t** valueLabel,
                           lv_event_cb_t minusCb, lv_event_cb_t plusCb,
                           void* minusData, void* plusData,
                           const lv_font_t* valueFont, lv_color_t valueColor,
                           int valueW) {
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
  lv_obj_set_style_bg_color(minusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(minusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(minusBtn, 1, 0);
  lv_obj_set_style_border_color(minusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(minusBtn, minusCb, LV_EVENT_CLICKED, minusData);

  lv_obj_t* minusLbl = lv_label_create(minusBtn);
  lv_label_set_text(minusLbl, "-");
  lv_obj_set_style_text_font(minusLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(minusLbl, COL_TEXT, 0);
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
  lv_obj_set_style_bg_color(plusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(plusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(plusBtn, 1, 0);
  lv_obj_set_style_border_color(plusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(plusBtn, plusCb, LV_EVENT_CLICKED, plusData);

  lv_obj_t* plusLbl = lv_label_create(plusBtn);
  lv_label_set_text(plusLbl, "+");
  lv_obj_set_style_text_font(plusLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(plusLbl, COL_TEXT, 0);
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
static void create_info_row(lv_obj_t* parent, int y, const char* labelText,
                            lv_obj_t** valueLabel) {
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
  editDir = p ? p->direction : 0;
  editRepeats = p ? p->step_repeats : 1;
  editDwell = p ? p->step_dwell_sec : 0.0f;

  // ── Header bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // [ESC] button at right of header
  lv_obj_t* escBtn = lv_button_create(header);
  lv_obj_set_size(escBtn, 60, 24);
  lv_obj_set_pos(escBtn, SCREEN_W - 60 - PAD_X, 3);
  lv_obj_set_style_bg_color(escBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(escBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(escBtn, 1, 0);
  lv_obj_set_style_border_color(escBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(escBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* escLbl = lv_label_create(escBtn);
  lv_label_set_text(escLbl, "[ESC]");
  lv_obj_set_style_text_font(escLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(escLbl, COL_TEXT_DIM, 0);
  lv_obj_center(escLbl);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "EDIT STEP");
  lv_obj_set_style_text_font(title, FONT_LARGE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // ── Content area: scrollable for small screens ──
  lv_obj_t* content = lv_obj_create(screen);
  lv_obj_set_size(content, SCREEN_W, SCREEN_H - HEADER_H);
  lv_obj_set_pos(content, 0, HEADER_H);
  lv_obj_set_style_bg_color(content, COL_BG, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_set_style_radius(content, 0, 0);
  lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  int y = 6;

  // ── TARGET ANGLE (left half) ──
  const int leftW = 380;
  create_adj_row(content, y, "TARGET ANGLE", &angleLabel,
                 angle_adj_cb, angle_adj_cb,
                 (void*)(intptr_t)(-1), (void*)(intptr_t)1,
                 FONT_HUGE, COL_ACCENT, leftW - 120);
  if (angleLabel) lv_label_set_text_fmt(angleLabel, "%.0f", editAngle);

  // ── RPM (right half, x=400) ──
  {
    const int rpmX = 400;
    const int btnW = 48;
    const int btnH = 36;

    lv_obj_t* rpmTitle = lv_label_create(content);
    lv_label_set_text(rpmTitle, "RPM");
    lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(rpmTitle, rpmX, y);

    // Minus
    lv_obj_t* rpmMinus = lv_button_create(content);
    lv_obj_set_size(rpmMinus, btnW, btnH);
    lv_obj_set_pos(rpmMinus, rpmX, y + 14);
    lv_obj_set_style_bg_color(rpmMinus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(rpmMinus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(rpmMinus, 1, 0);
    lv_obj_set_style_border_color(rpmMinus, COL_BORDER, 0);
    lv_obj_add_event_cb(rpmMinus, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

    lv_obj_t* rpmMinusLbl = lv_label_create(rpmMinus);
    lv_label_set_text(rpmMinusLbl, "-");
    lv_obj_set_style_text_font(rpmMinusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(rpmMinusLbl, COL_TEXT, 0);
    lv_obj_center(rpmMinusLbl);

    // Value panel
    lv_obj_t* rpmPanel = lv_obj_create(content);
    lv_obj_set_size(rpmPanel, 200, btnH);
    lv_obj_set_pos(rpmPanel, rpmX + btnW + 8, y + 14);
    lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(rpmPanel, COL_BORDER, 0);
    lv_obj_set_style_border_width(rpmPanel, 1, 0);
    lv_obj_set_style_radius(rpmPanel, RADIUS_BTN, 0);
    lv_obj_set_style_pad_all(rpmPanel, 0, 0);
    lv_obj_remove_flag(rpmPanel, LV_OBJ_FLAG_SCROLLABLE);

    rpmLabel = lv_label_create(rpmPanel);
    lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
    lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
    lv_obj_center(rpmLabel);

    // Plus
    lv_obj_t* rpmPlus = lv_button_create(content);
    lv_obj_set_size(rpmPlus, btnW, btnH);
    lv_obj_set_pos(rpmPlus, rpmX + btnW + 8 + 200 + 8, y + 14);
    lv_obj_set_style_bg_color(rpmPlus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(rpmPlus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(rpmPlus, 1, 0);
    lv_obj_set_style_border_color(rpmPlus, COL_BORDER, 0);
    lv_obj_add_event_cb(rpmPlus, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t* rpmPlusLbl = lv_label_create(rpmPlus);
    lv_label_set_text(rpmPlusLbl, "+");
    lv_obj_set_style_text_font(rpmPlusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(rpmPlusLbl, COL_TEXT, 0);
    lv_obj_center(rpmPlusLbl);
  }

  y += 58;

  // ── Separator ──
  create_separator(content, y);
  y += 6;

  // ── DIRECTION: CW / CCW toggle (170x40 each) ──
  lv_obj_t* dirTitle = lv_label_create(content);
  lv_label_set_text(dirTitle, "DIRECTION");
  lv_obj_set_style_text_font(dirTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(dirTitle, PAD_X, y);

  y += 14;
  {
    const int dirBtnW = 170;
    const int dirBtnH = 40;
    const int dirGap = 10;

    const char* dirTexts[] = {"CW", "CCW"};

    for (int i = 0; i < 2; i++) {
      dirBtns[i] = lv_button_create(content);
      lv_obj_set_size(dirBtns[i], dirBtnW, dirBtnH);
      lv_obj_set_pos(dirBtns[i], PAD_X + i * (dirBtnW + dirGap), y);
      lv_obj_set_style_radius(dirBtns[i], RADIUS_BTN, 0);

      bool isActive = (i == editDir);
      lv_obj_set_style_bg_color(dirBtns[i], isActive ? COL_BTN_ACTIVE : COL_BTN_BG, 0);
      lv_obj_set_style_border_color(dirBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);
      lv_obj_set_style_border_width(dirBtns[i], isActive ? 2 : 1, 0);

      lv_obj_add_event_cb(dirBtns[i], dir_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

      lv_obj_t* lbl = lv_label_create(dirBtns[i]);
      lv_label_set_text(lbl, dirTexts[i]);
      lv_obj_set_style_text_font(lbl, FONT_MED, 0);
      lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
      lv_obj_center(lbl);
    }
  }

  y += 50;

  // ── Separator ──
  create_separator(content, y);
  y += 6;

  // ── REPEATS ──
  lv_obj_t* repeatsTitle = lv_label_create(content);
  lv_label_set_text(repeatsTitle, "REPEATS");
  lv_obj_set_style_text_font(repeatsTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(repeatsTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(repeatsTitle, PAD_X, y);

  {
    const int repX = 100;
    const int btnW = 48;
    const int btnH = 36;

    // Minus
    lv_obj_t* repMinus = lv_button_create(content);
    lv_obj_set_size(repMinus, btnW, btnH);
    lv_obj_set_pos(repMinus, repX, y - 2);
    lv_obj_set_style_bg_color(repMinus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(repMinus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(repMinus, 1, 0);
    lv_obj_set_style_border_color(repMinus, COL_BORDER, 0);
    lv_obj_add_event_cb(repMinus, repeats_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

    lv_obj_t* repMinusLbl = lv_label_create(repMinus);
    lv_label_set_text(repMinusLbl, "-");
    lv_obj_set_style_text_font(repMinusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(repMinusLbl, COL_TEXT, 0);
    lv_obj_center(repMinusLbl);

    // Value
    lv_obj_t* repPanel = lv_obj_create(content);
    lv_obj_set_size(repPanel, 100, btnH);
    lv_obj_set_pos(repPanel, repX + btnW + 8, y - 2);
    lv_obj_set_style_bg_color(repPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(repPanel, COL_BORDER, 0);
    lv_obj_set_style_border_width(repPanel, 1, 0);
    lv_obj_set_style_radius(repPanel, RADIUS_BTN, 0);
    lv_obj_set_style_pad_all(repPanel, 0, 0);
    lv_obj_remove_flag(repPanel, LV_OBJ_FLAG_SCROLLABLE);

    repeatsLabel = lv_label_create(repPanel);
    lv_label_set_text_fmt(repeatsLabel, "%d", editRepeats);
    lv_obj_set_style_text_font(repeatsLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(repeatsLabel, COL_TEXT, 0);
    lv_obj_center(repeatsLabel);

    // Plus
    lv_obj_t* repPlus = lv_button_create(content);
    lv_obj_set_size(repPlus, btnW, btnH);
    lv_obj_set_pos(repPlus, repX + btnW + 8 + 100 + 8, y - 2);
    lv_obj_set_style_bg_color(repPlus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(repPlus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(repPlus, 1, 0);
    lv_obj_set_style_border_color(repPlus, COL_BORDER, 0);
    lv_obj_add_event_cb(repPlus, repeats_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t* repPlusLbl = lv_label_create(repPlus);
    lv_label_set_text(repPlusLbl, "+");
    lv_obj_set_style_text_font(repPlusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(repPlusLbl, COL_TEXT, 0);
    lv_obj_center(repPlusLbl);
  }

  // ── DWELL TIME (right side, x=400) ──
  {
    const int dwellX = 400;
    const int btnW = 48;
    const int btnH = 36;

    lv_obj_t* dwellTitle = lv_label_create(content);
    lv_label_set_text(dwellTitle, "DWELL TIME");
    lv_obj_set_style_text_font(dwellTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(dwellTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(dwellTitle, dwellX, y);

    // Minus
    lv_obj_t* dwellMinus = lv_button_create(content);
    lv_obj_set_size(dwellMinus, btnW, btnH);
    lv_obj_set_pos(dwellMinus, dwellX, y + 14);
    lv_obj_set_style_bg_color(dwellMinus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(dwellMinus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(dwellMinus, 1, 0);
    lv_obj_set_style_border_color(dwellMinus, COL_BORDER, 0);
    lv_obj_add_event_cb(dwellMinus, dwell_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

    lv_obj_t* dwellMinusLbl = lv_label_create(dwellMinus);
    lv_label_set_text(dwellMinusLbl, "-");
    lv_obj_set_style_text_font(dwellMinusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(dwellMinusLbl, COL_TEXT, 0);
    lv_obj_center(dwellMinusLbl);

    // Value
    lv_obj_t* dwellPanel = lv_obj_create(content);
    lv_obj_set_size(dwellPanel, 200, btnH);
    lv_obj_set_pos(dwellPanel, dwellX + btnW + 8, y + 14);
    lv_obj_set_style_bg_color(dwellPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(dwellPanel, COL_BORDER, 0);
    lv_obj_set_style_border_width(dwellPanel, 1, 0);
    lv_obj_set_style_radius(dwellPanel, RADIUS_BTN, 0);
    lv_obj_set_style_pad_all(dwellPanel, 0, 0);
    lv_obj_remove_flag(dwellPanel, LV_OBJ_FLAG_SCROLLABLE);

    dwellLabel = lv_label_create(dwellPanel);
    lv_label_set_text_fmt(dwellLabel, "%.1f sec", editDwell);
    lv_obj_set_style_text_font(dwellLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(dwellLabel, COL_TEXT, 0);
    lv_obj_center(dwellLabel);

    // Plus
    lv_obj_t* dwellPlus = lv_button_create(content);
    lv_obj_set_size(dwellPlus, btnW, btnH);
    lv_obj_set_pos(dwellPlus, dwellX + btnW + 8 + 200 + 8, y + 14);
    lv_obj_set_style_bg_color(dwellPlus, COL_BTN_BG, 0);
    lv_obj_set_style_radius(dwellPlus, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(dwellPlus, 1, 0);
    lv_obj_set_style_border_color(dwellPlus, COL_BORDER, 0);
    lv_obj_add_event_cb(dwellPlus, dwell_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_t* dwellPlusLbl = lv_label_create(dwellPlus);
    lv_label_set_text(dwellPlusLbl, "+");
    lv_obj_set_style_text_font(dwellPlusLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(dwellPlusLbl, COL_TEXT, 0);
    lv_obj_center(dwellPlusLbl);
  }

  y += 58;

  // ── Separator ──
  create_separator(content, y);
  y += 6;

  // ── COMPUTED VALUES ──
  lv_obj_t* computedTitle = lv_label_create(content);
  lv_label_set_text(computedTitle, "COMPUTED");
  lv_obj_set_style_text_font(computedTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(computedTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(computedTitle, PAD_X, y);
  y += 16;

  create_info_row(content, y, "TOTAL ANGLE:", &totalAngleLabel);
  y += 18;
  create_info_row(content, y, "DURATION:", &durationLabel);
  y += 18;
  create_info_row(content, y, "STEPS:", &stepsLabel);
  y += 24;

  // Populate computed values
  update_computed();

  // ── Separator ──
  create_separator(content, y);
  y += 6;

  // ── CANCEL + SAVE buttons ──
  {
    const int btnW = BTN_W_ACTION;
    const int btnH = 40;
    const int btnGap = 20;
    const int totalBtnW = btnW * 2 + btnGap;
    const int btnStartX = (SCREEN_W - totalBtnW) / 2;

    ui_create_btn(content, btnStartX, y, btnW, btnH, "CANCEL", FONT_MED, UI_BTN_NORMAL, cancel_cb,
                  nullptr);
    ui_create_btn(content, btnStartX + btnW + btnGap, y, btnW, btnH, "SAVE", FONT_MED, UI_BTN_ACCENT,
                  save_cb, nullptr);
  }

  LOG_I("Screen edit step: full edit layout created");
}

void screen_edit_step_invalidate_widgets() {
  angleLabel = nullptr;
  rpmLabel = nullptr;
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

  Preset* p = screen_program_edit_get_preset();
  if (!p) return;

  // Sync local state from preset
  editAngle = p->step_angle;
  editRpm = p->rpm;

  // Update angle display
  if (angleLabel) lv_label_set_text_fmt(angleLabel, "%.0f", editAngle);
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
  if (repeatsLabel) lv_label_set_text_fmt(repeatsLabel, "%d", editRepeats);
  if (dwellLabel) lv_label_set_text_fmt(dwellLabel, "%.1f sec", editDwell);
  update_computed();
}
