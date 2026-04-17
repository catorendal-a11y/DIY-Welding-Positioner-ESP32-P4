// TIG Rotator Controller - Step Mode Screen
// Kinematics: STEPS and moves use angleToSteps(); RPM uses speed_* -> rpmToStepHzCalibrated().
// Same GEAR_RATIO (1:108 per motor.worm.svg), d_emne/D_RULLE, microsteps, calibration as rest of app.

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include "../test_screen_logic.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static const float STEP_PRESET_DEG[4] = {45.0f, 90.0f, 180.0f, 360.0f};

static float currentAngle = 90.0f;
static float targetRpm = 2.0f;  // synced in screen_step_create from speed_get_target_rpm()
static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* arcWidget = nullptr;
static lv_obj_t* angleArcLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* stepsLabel = nullptr;
static lv_obj_t* presetBtns[5] = {nullptr};
static int activePreset = 1;  // 90 deg is default (index 1)
static lv_obj_t* customNumpad = nullptr;
static lv_obj_t* customTa = nullptr;
static lv_obj_t* customHint = nullptr;
static bool numpadClosePending = false;

static lv_obj_t* stepActionBtn = nullptr;
static lv_obj_t* diameterSummaryLabel = nullptr;
static lv_obj_t* diaKb = nullptr;
static lv_obj_t* diaTa = nullptr;
static lv_obj_t* diaHint = nullptr;
static bool diaClosePending = false;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void purge_diameter_overlay_sync() {
  if (diaKb) {
    lv_obj_delete(diaKb);
    diaKb = nullptr;
  }
  if (diaTa) {
    lv_obj_delete(diaTa);
    diaTa = nullptr;
  }
  if (diaHint) {
    lv_obj_delete(diaHint);
    diaHint = nullptr;
  }
  diaClosePending = false;
}

static void step_clamp_target_rpm(void) {
  float cap = speed_get_rpm_max();
  if (targetRpm < MIN_RPM) targetRpm = MIN_RPM;
  if (targetRpm > cap) targetRpm = cap;
}

static void step_push_rpm_to_speed_and_label(void) {
  step_clamp_target_rpm();
  speed_slider_set(targetRpm);
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", targetRpm);
}

static void refresh_diameter_summary() {
  if (!diameterSummaryLabel) return;
  float mm = speed_get_workpiece_diameter_mm();
  if (mm < 1.0f) {
    lv_label_set_text(diameterSummaryLabel, "Part: not set");
  } else {
    char buf[56];
    snprintf(buf, sizeof(buf), "Part: OD %.0f mm", (double)mm);
    lv_label_set_text(diameterSummaryLabel, buf);
  }
}

static void update_preset_styles() {
  for (int i = 0; i < 5; i++) {
    if (!presetBtns[i]) continue;
    bool isActive = (i < 4 && currentAngle == STEP_PRESET_DEG[i]) || (i == 4 && activePreset == 4);
    lv_obj_set_style_bg_color(presetBtns[i], isActive ? COL_BTN_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_width(presetBtns[i], isActive ? 2 : 1, 0);
    lv_obj_set_style_border_color(presetBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_t* lbl = lv_obj_get_child(presetBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
  }
}

static void update_info_panel() {
  // Update angle arc label
  if (angleArcLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f deg", currentAngle);
    lv_label_set_text(angleArcLabel, buf);
  }

  // Update arc indicator
  if (arcWidget) {
    // Map angle (0-360) to arc range (0-360 degrees rotation)
    // Arc background: full circle (0-360), indicator: 0 to currentAngle
    int32_t arcAngle = (int32_t)currentAngle;
    if (arcAngle > 360) arcAngle = 360;
    if (arcAngle < 0) arcAngle = 0;
    lv_arc_set_value(arcWidget, arcAngle);
  }

  // Update steps display
  if (stepsLabel) {
    long steps = angleToSteps(currentAngle);
    lv_label_set_text_fmt(stepsLabel, "%ld", steps);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  purge_diameter_overlay_sync();
  if (customNumpad) { lv_obj_t* old = customNumpad; customNumpad = nullptr; lv_obj_delete_async(old); }
  if (customTa) { lv_obj_t* old = customTa; customTa = nullptr; lv_obj_delete_async(old); }
  if (customHint) { lv_obj_t* old = customHint; customHint = nullptr; lv_obj_delete_async(old); }
  screens_show(SCREEN_MAIN);
}

static void step_reset_btn_cb(lv_event_t* e) {
  (void)e;
  if (control_get_state() == STATE_IDLE) {
    control_reset_step_accumulator();
    update_info_panel();
  }
}

static void preset_cb(lv_event_t* e) {
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  if (index < 4) {
    if (control_get_state() == STATE_IDLE) {
      control_reset_step_accumulator();
    }
    currentAngle = STEP_PRESET_DEG[index];
    activePreset = index;
    update_preset_styles();
    update_info_panel();
  }
}

static void custom_keyboard_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (customTa) {
      const char* txt = lv_textarea_get_text(customTa);
      float val = step_parse_first_float(txt);
      if (val > 0.0f && step_angle_valid(val)) {
        if (control_get_state() == STATE_IDLE) {
          control_reset_step_accumulator();
        }
        currentAngle = val;
        if (currentAngle > 3600.0f) currentAngle = 3600.0f;
        activePreset = 4;
        update_preset_styles();
        update_info_panel();
      }
    }
  }

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    numpadClosePending = true;
  }
}

static void diameter_keyboard_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (diaTa) {
      const char* txt = lv_textarea_get_text(diaTa);
      float mm = step_parse_first_float(txt);
      if (mm >= 1.0f && mm <= 20000.0f) {
        speed_set_workpiece_diameter_mm(mm);
        refresh_diameter_summary();
        update_info_panel();
      }
    }
  }
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    diaClosePending = true;
  }
}

static void diameter_btn_cb(lv_event_t* e) {
  (void)e;
  if (diaKb) return;
  if (customNumpad) {
    lv_obj_t* old = customNumpad;
    customNumpad = nullptr;
    lv_obj_delete_async(old);
  }
  if (customTa) {
    lv_obj_t* old = customTa;
    customTa = nullptr;
    lv_obj_delete_async(old);
  }
  if (customHint) {
    lv_obj_t* old = customHint;
    customHint = nullptr;
    lv_obj_delete_async(old);
  }
  numpadClosePending = false;

  lv_obj_t* scr = screenRoots[SCREEN_STEP];
  diaHint = lv_label_create(scr);
  lv_label_set_text(diaHint,
                    "Outer diameter in mm (number). Used for steps / RPM.\n"
                    "Preset angles (45/90/180/360) are still degrees on the part.");
  lv_obj_set_style_text_font(diaHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(diaHint, COL_TEXT_DIM, 0);
  lv_obj_align(diaHint, LV_ALIGN_TOP_MID, 0, 44);

  diaTa = lv_textarea_create(scr);
  lv_obj_set_size(diaTa, 420, 44);
  lv_obj_align(diaTa, LV_ALIGN_TOP_MID, 0, 100);
  lv_textarea_set_one_line(diaTa, true);
  lv_textarea_set_accepted_chars(diaTa, "0123456789.,");
  lv_obj_set_style_bg_color(diaTa, COL_BG, 0);
  lv_obj_set_style_border_color(diaTa, COL_ACCENT, 0);
  lv_obj_set_style_border_width(diaTa, 2, 0);
  lv_obj_set_style_text_color(diaTa, COL_TEXT, 0);

  diaKb = lv_keyboard_create(scr);
  lv_keyboard_set_mode(diaKb, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(diaKb, diaTa);
  lv_obj_set_style_border_width(diaKb, 2, 0);
  lv_obj_set_style_border_color(diaKb, COL_ACCENT, 0);
  lv_obj_set_size(diaKb, SCREEN_W, 220);
  lv_obj_align(diaKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(diaKb, diameter_keyboard_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(diaKb, diameter_keyboard_cb, LV_EVENT_CANCEL, nullptr);
}

static void custom_btn_cb(lv_event_t* e) {
  if (!customNumpad) {
    purge_diameter_overlay_sync();
    lv_obj_t* scr = screenRoots[SCREEN_STEP];
    customHint = lv_label_create(scr);
    lv_label_set_text(customHint, "Angle on the part in degrees (e.g. 90 or 90.5)");
    lv_obj_set_style_text_font(customHint, FONT_SMALL, 0);
    lv_obj_set_style_text_color(customHint, COL_TEXT_DIM, 0);
    lv_obj_align(customHint, LV_ALIGN_TOP_MID, 0, 44);

    customTa = lv_textarea_create(scr);
    lv_obj_set_size(customTa, 420, 44);
    lv_obj_align(customTa, LV_ALIGN_TOP_MID, 0, 92);
    lv_textarea_set_one_line(customTa, true);
    lv_textarea_set_accepted_chars(customTa, "0123456789.,");
    lv_obj_set_style_bg_color(customTa, COL_BG, 0);
    lv_obj_set_style_border_color(customTa, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customTa, 2, 0);
    lv_obj_set_style_text_color(customTa, COL_TEXT, 0);

    customNumpad = lv_keyboard_create(scr);
    lv_keyboard_set_mode(customNumpad, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(customNumpad, customTa);

    lv_obj_set_style_bg_color(customNumpad, COL_BG, 0);
    lv_obj_set_style_border_color(customNumpad, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customNumpad, 2, 0);
    lv_obj_add_event_cb(customNumpad, custom_keyboard_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(customNumpad, custom_keyboard_cb, LV_EVENT_CANCEL, nullptr);
  }
}

static void rpm_minus_cb(lv_event_t* e) {
  (void)e;
  if (targetRpm > 0.1f) targetRpm -= 0.1f;
  step_push_rpm_to_speed_and_label();
}

static void rpm_plus_cb(lv_event_t* e) {
  (void)e;
  targetRpm += 0.1f;
  step_push_rpm_to_speed_and_label();
}

static void step_event_cb(lv_event_t* e) {
  (void)e;
  if (control_get_state() != STATE_IDLE) return;
  step_push_rpm_to_speed_and_label();
  control_start_step(currentAngle);
}

static void stop_event_cb(lv_event_t* e) {
  (void)e;
  control_stop();
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];
  purge_diameter_overlay_sync();
  screen_step_invalidate_widgets();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  targetRpm = speed_get_target_rpm();
  step_clamp_target_rpm();

  // ── Header bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "STEP MODE");
  lv_obj_set_style_text_font(title, FONT_XL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 5);

  // ── Layout (800x480): gauge left, info right, presets full width, action bar bottom ──
  const int kBarH = 48;
  const int kBarMargin = 8;
  const int kBarY = SCREEN_H - kBarMargin - kBarH;
  const int kPresetH = 44;
  const int kPresetGapAboveBar = 8;
  const int kPresetY = kBarY - kPresetGapAboveBar - kPresetH;
  const int kContentTop = HEADER_H + 6;
  const int kProSz = 264;
  const int kProX = 36;
  const int kProY = kContentTop;
  const int kRightX = kProX + kProSz + 16;
  const int kRightMaxBottom = kPresetY - 10;

  // ── Left: protractor gauge (compact — leaves room for right column + presets) ──
  lv_obj_t* protractor = lv_obj_create(screen);
  lv_obj_set_size(protractor, kProSz, kProSz);
  lv_obj_set_pos(protractor, kProX, kProY);
  lv_obj_set_style_bg_color(protractor, COL_PROTRACTOR_BG, 0);
  lv_obj_set_style_radius(protractor, kProSz / 2, 0);
  lv_obj_set_style_border_width(protractor, 1, 0);
  lv_obj_set_style_border_color(protractor, COL_BORDER, 0);
  lv_obj_set_style_pad_all(protractor, 0, 0);
  lv_obj_remove_flag(protractor, LV_OBJ_FLAG_SCROLLABLE);

  // Tick marks (kProSz=264 -> center 132; rOuter 120, rInner 106)
  {
    lv_obj_t* tick0 = lv_line_create(protractor);
    static const lv_point_precise_t pts0[] = {{252, 132}, {238, 132}};
    lv_line_set_points(tick0, pts0, 2);
    lv_obj_set_style_line_color(tick0, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick0, 2, 0);

    // 90 deg (bottom)
    lv_obj_t* tick90 = lv_line_create(protractor);
    static const lv_point_precise_t pts90[] = {{132, 252}, {132, 238}};
    lv_line_set_points(tick90, pts90, 2);
    lv_obj_set_style_line_color(tick90, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick90, 2, 0);

    // 180 deg (left)
    lv_obj_t* tick180 = lv_line_create(protractor);
    static const lv_point_precise_t pts180[] = {{12, 132}, {26, 132}};
    lv_line_set_points(tick180, pts180, 2);
    lv_obj_set_style_line_color(tick180, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick180, 2, 0);

    // 270 deg (top)
    lv_obj_t* tick270 = lv_line_create(protractor);
    static const lv_point_precise_t pts270[] = {{132, 12}, {132, 26}};
    lv_line_set_points(tick270, pts270, 2);
    lv_obj_set_style_line_color(tick270, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick270, 2, 0);
  }

  // Arc showing current angle (0 to current value) in COL_ACCENT
  arcWidget = lv_arc_create(protractor);
  lv_obj_set_size(arcWidget, kProSz - 36, kProSz - 36);
  lv_obj_align(arcWidget, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arcWidget, 0, 360);
  lv_arc_set_value(arcWidget, (int32_t)currentAngle);
  lv_arc_set_bg_angles(arcWidget, 0, 360);
  // Indicator color
  lv_obj_set_style_arc_color(arcWidget, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcWidget, 5, LV_PART_INDICATOR);
  // Background arc
  lv_obj_set_style_arc_color(arcWidget, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcWidget, 2, LV_PART_MAIN);
  // Knob (hide it)
  lv_obj_set_style_opa(arcWidget, 0, LV_PART_KNOB);
  // Remove clickable
  lv_obj_remove_flag(arcWidget, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(arcWidget, LV_OBJ_FLAG_SCROLLABLE);

  // Angle label centered in protractor
  angleArcLabel = lv_label_create(protractor);
  char angleBuf[16];
  snprintf(angleBuf, sizeof(angleBuf), "%.0f deg", currentAngle);
  lv_label_set_text(angleArcLabel, angleBuf);
  lv_obj_set_style_text_font(angleArcLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(angleArcLabel, COL_ACCENT, 0);
  lv_obj_center(angleArcLabel);

  // ── Right panel (stack from top; ends above preset row — kRightMaxBottom) ──
  const int rightX = kRightX;
  int rightY = kProY;

  // TARGET label
  lv_obj_t* targetLbl = lv_label_create(screen);
  lv_label_set_text(targetLbl, "TARGET");
  lv_obj_set_style_text_font(targetLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(targetLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(targetLbl, rightX, rightY);

  // Target angle value
  angleLabel = lv_label_create(screen);
  char targetBuf[16];
  snprintf(targetBuf, sizeof(targetBuf), "%.0f deg", currentAngle);
  lv_label_set_text(angleLabel, targetBuf);
  lv_obj_set_style_text_font(angleLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(angleLabel, COL_ACCENT, 0);
  lv_obj_set_pos(angleLabel, rightX, rightY + 16);
  rightY += 58;

  // Separator
  const int sepW = SCREEN_W - rightX - 8;
  lv_obj_t* sep1 = lv_obj_create(screen);
  lv_obj_set_size(sep1, sepW, 1);
  lv_obj_set_pos(sep1, rightX, rightY);
  lv_obj_set_style_bg_color(sep1, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(sep1, 0, 0);
  lv_obj_set_style_pad_all(sep1, 0, 0);
  lv_obj_set_style_radius(sep1, 0, 0);
  lv_obj_remove_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);
  rightY += 6;

  // SPEED label + value
  lv_obj_t* speedLbl = lv_label_create(screen);
  lv_label_set_text(speedLbl, "SPEED:");
  lv_obj_set_style_text_font(speedLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(speedLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(speedLbl, rightX, rightY);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", targetRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_set_pos(rpmLabel, rightX + 72, rightY - 2);

  lv_obj_t* rpmUnit = lv_label_create(screen);
  lv_label_set_text(rpmUnit, "RPM");
  lv_obj_set_style_text_font(rpmUnit, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(rpmUnit, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmUnit, rightX + 132, rightY + 2);
  rightY += 30;

  // DIR label + value
  lv_obj_t* dirLbl = lv_label_create(screen);
  lv_label_set_text(dirLbl, "DIR:");
  lv_obj_set_style_text_font(dirLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(dirLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(dirLbl, rightX, rightY);

  Direction dir = speed_get_direction();
  lv_obj_t* dirVal = lv_label_create(screen);
  lv_label_set_text(dirVal, (dir == DIR_CW) ? "CW" : "CCW");
  lv_obj_set_style_text_font(dirVal, FONT_XL, 0);
  lv_obj_set_style_text_color(dirVal, COL_TEXT, 0);
  lv_obj_set_pos(dirVal, rightX + 50, rightY - 2);
  rightY += 30;

  // STEPS label + value
  lv_obj_t* stepsLbl = lv_label_create(screen);
  lv_label_set_text(stepsLbl, "STEPS:");
  lv_obj_set_style_text_font(stepsLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stepsLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(stepsLbl, rightX, rightY);

  stepsLabel = lv_label_create(screen);
  long steps = angleToSteps(currentAngle);
  lv_label_set_text_fmt(stepsLabel, "%ld", steps);
  lv_obj_set_style_text_font(stepsLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(stepsLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(stepsLabel, rightX + 58, rightY);
  rightY += 30;

  // RPM row (above dia); horizontal gap between RPM- block and dia on the row below
  const int kRpmDiaHGap = 16;
  const int kRpmBetween = 10;
  const int rpmBtnW = 104;
  const int rpmBtnH = 48;
  const int diaBtnW = 132;
  const int diaBtnH = 46;
  const int diaX = SCREEN_W - 8 - diaBtnW;
  const int rpmPlusX = SCREEN_W - 8 - rpmBtnW;
  const int rpmMinusX = rpmPlusX - kRpmBetween - rpmBtnW;

  lv_obj_t* rpmMinusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmMinusBtn, rpmBtnW, rpmBtnH);
  lv_obj_set_pos(rpmMinusBtn, rpmMinusX, rightY);
  lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmMinusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmMinusBtn, 1, 0);
  lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_minus_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rpmMinusLbl = lv_label_create(rpmMinusBtn);
  lv_label_set_text(rpmMinusLbl, "RPM -");
  lv_obj_set_style_text_font(rpmMinusLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rpmMinusLbl, COL_TEXT, 0);
  lv_obj_center(rpmMinusLbl);

  lv_obj_t* rpmPlusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmPlusBtn, rpmBtnW, rpmBtnH);
  lv_obj_set_pos(rpmPlusBtn, rpmPlusX, rightY);
  lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmPlusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmPlusBtn, 1, 0);
  lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_plus_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rpmPlusLbl = lv_label_create(rpmPlusBtn);
  lv_label_set_text(rpmPlusLbl, "RPM +");
  lv_obj_set_style_text_font(rpmPlusLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rpmPlusLbl, COL_TEXT, 0);
  lv_obj_center(rpmPlusLbl);

  rightY += rpmBtnH + kRpmDiaHGap;

  diameterSummaryLabel = lv_label_create(screen);
  lv_label_set_long_mode(diameterSummaryLabel, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(diameterSummaryLabel, (lv_coord_t)(diaX - rightX - 12));
  lv_obj_set_style_text_font(diameterSummaryLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(diameterSummaryLabel, COL_TEXT, 0);
  lv_obj_set_pos(diameterSummaryLabel, rightX, rightY + 2);

  lv_obj_t* diaTapBtn = lv_button_create(screen);
  lv_obj_set_size(diaTapBtn, diaBtnW, diaBtnH);
  lv_obj_set_pos(diaTapBtn, diaX, rightY);
  lv_obj_set_style_bg_color(diaTapBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(diaTapBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(diaTapBtn, 1, 0);
  lv_obj_set_style_border_color(diaTapBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(diaTapBtn, diameter_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* diaTapLbl = lv_label_create(diaTapBtn);
  lv_label_set_text(diaTapLbl, "dia mm");
  lv_obj_set_style_text_font(diaTapLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(diaTapLbl, COL_TEXT, 0);
  lv_obj_center(diaTapLbl);

  rightY += diaBtnH + 8;

  const int resetW = 168;
  const int resetH = 36;
  lv_obj_t* resetBtn = lv_button_create(screen);
  lv_obj_set_size(resetBtn, resetW, resetH);
  lv_obj_set_pos(resetBtn, rightX, rightY);
  lv_obj_set_style_bg_color(resetBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(resetBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(resetBtn, 1, 0);
  lv_obj_set_style_border_color(resetBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(resetBtn, step_reset_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* resetLbl = lv_label_create(resetBtn);
  lv_label_set_text(resetLbl, "RESET");
  lv_obj_set_style_text_font(resetLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(resetLbl, COL_TEXT, 0);
  lv_obj_center(resetLbl);
  rightY += resetH + 8;
#if DEBUG_BUILD
  if (rightY > kRightMaxBottom) {
    LOG_W("Step UI: right stack y=%d exceeds band %d", rightY, kRightMaxBottom);
  }
#endif

  // ── Presets row (full width; Y derived from bar — never overlaps reset column) ──
  const int presetY = kPresetY;
  const int presetStartX = 8;
  const int presetEndX = SCREEN_W - 8;
  const int presetGap = 6;
  const int presetH = kPresetH;
  const int presetW = (presetEndX - presetStartX - 4 * presetGap) / 5;

  const char* presetTexts[] = {"45", "90", "180", "360", "CUSTOM"};

  // 4 angle presets
  for (int i = 0; i < 4; i++) {
    presetBtns[i] = lv_button_create(screen);
    lv_obj_set_size(presetBtns[i], presetW, presetH);
    lv_obj_set_pos(presetBtns[i], presetStartX + i * (presetW + presetGap), presetY);
    lv_obj_set_style_radius(presetBtns[i], RADIUS_BTN, 0);
    lv_obj_add_event_cb(presetBtns[i], preset_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    bool isActive = (i == activePreset);
    lv_obj_set_style_bg_color(presetBtns[i], isActive ? COL_BTN_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_width(presetBtns[i], isActive ? 2 : 1, 0);
    lv_obj_set_style_border_color(presetBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);

    lv_obj_t* lbl = lv_label_create(presetBtns[i]);
    lv_label_set_text(lbl, presetTexts[i]);
    lv_obj_set_style_text_font(lbl, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(lbl);
  }

  // CUSTOM button (5th)
  presetBtns[4] = lv_button_create(screen);
  lv_obj_set_size(presetBtns[4], presetW, presetH);
  lv_obj_set_pos(presetBtns[4], presetStartX + 4 * (presetW + presetGap), presetY);
  lv_obj_set_style_bg_color(presetBtns[4], COL_BTN_BG, 0);
  lv_obj_set_style_radius(presetBtns[4], RADIUS_BTN, 0);
  lv_obj_set_style_border_width(presetBtns[4], 1, 0);
  lv_obj_set_style_border_color(presetBtns[4], COL_BORDER, 0);
  lv_obj_add_event_cb(presetBtns[4], custom_btn_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* customLabel = lv_label_create(presetBtns[4]);
  lv_label_set_text(customLabel, "CUSTOM");
  lv_obj_set_style_text_font(customLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(customLabel, COL_TEXT, 0);
  lv_obj_center(customLabel);

  // ── Bottom bar: BACK + STEP + STOP ──
  const int barY = kBarY;
  const int barH = kBarH;
  const int barBtnW = 256;
  const int barGap = 8;
  const int barStartX = 8;

  // < BACK button (left)
  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, barBtnW, barH);
  lv_obj_set_pos(backBtn, barStartX, barY);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, FONT_BTN, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  // > STEP button (center)
  lv_obj_t* stepBtn = lv_button_create(screen);
  lv_obj_set_size(stepBtn, barBtnW, barH);
  lv_obj_set_pos(stepBtn, barStartX + barBtnW + barGap, barY);
  lv_obj_set_style_bg_color(stepBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(stepBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(stepBtn, 2, 0);
  lv_obj_set_style_border_color(stepBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);
  stepActionBtn = stepBtn;

  lv_obj_t* stepLabel = lv_label_create(stepBtn);
  lv_label_set_text(stepLabel, "> STEP");
  lv_obj_set_style_text_font(stepLabel, FONT_BTN, 0);
  lv_obj_set_style_text_color(stepLabel, COL_ACCENT, 0);
  lv_obj_center(stepLabel);

  // [] STOP button (right)
  lv_obj_t* stopBtn = lv_button_create(screen);
  lv_obj_set_size(stopBtn, barBtnW, barH);
  lv_obj_set_pos(stopBtn, barStartX + (barBtnW + barGap) * 2, barY);
  lv_obj_set_style_bg_color(stopBtn, COL_BTN_DANGER, 0);
  lv_obj_set_style_radius(stopBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(stopBtn, 2, 0);
  lv_obj_set_style_border_color(stopBtn, COL_RED, 0);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "[] STOP");
  lv_obj_set_style_text_font(stopLabel, FONT_BTN, 0);
  lv_obj_set_style_text_color(stopLabel, COL_RED, 0);
  lv_obj_center(stopLabel);

  refresh_diameter_summary();
  step_push_rpm_to_speed_and_label();
}

void screen_step_invalidate_widgets() {
  stepActionBtn = nullptr;
  angleLabel = nullptr;
  arcWidget = nullptr;
  angleArcLabel = nullptr;
  rpmLabel = nullptr;
  stepsLabel = nullptr;
  diameterSummaryLabel = nullptr;
  for (int i = 0; i < 5; i++) presetBtns[i] = nullptr;
  customNumpad = nullptr;
  customTa = nullptr;
  customHint = nullptr;
  numpadClosePending = false;
  diaKb = nullptr;
  diaTa = nullptr;
  diaHint = nullptr;
  diaClosePending = false;
}

void screen_step_update() {
  if (numpadClosePending) {
    if (customNumpad) { lv_obj_t* old = customNumpad; customNumpad = nullptr; lv_obj_delete_async(old); }
    if (customTa) { lv_obj_t* old = customTa; customTa = nullptr; lv_obj_delete_async(old); }
    if (customHint) { lv_obj_t* old = customHint; customHint = nullptr; lv_obj_delete_async(old); }
    numpadClosePending = false;
  }
  if (diaClosePending) {
    if (diaKb) { lv_obj_t* old = diaKb; diaKb = nullptr; lv_obj_delete_async(old); }
    if (diaTa) { lv_obj_t* old = diaTa; diaTa = nullptr; lv_obj_delete_async(old); }
    if (diaHint) { lv_obj_t* old = diaHint; diaHint = nullptr; lv_obj_delete_async(old); }
    diaClosePending = false;
  }
  if (!screens_is_active(SCREEN_STEP)) return;

  if (stepActionBtn) {
    if (control_get_state() == STATE_IDLE) {
      lv_obj_remove_state(stepActionBtn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(stepActionBtn, LV_STATE_DISABLED);
    }
  }

  SystemState state = control_get_state();
  if (state == STATE_STEP) {
    float accum = control_get_step_accumulated();
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f / %.0f deg", accum, currentAngle);
    if (angleArcLabel) lv_label_set_text(angleArcLabel, buf);
    if (angleLabel) lv_label_set_text(angleLabel, buf);
    // Update arc to show progress (round; ensure a visible sliver once motion has started)
    if (arcWidget) {
      int32_t progress = 0;
      if (currentAngle > 0.001f) {
        float ratio = accum / currentAngle;
        if (ratio > 1.0f) ratio = 1.0f;
        if (ratio < 0.0f) ratio = 0.0f;
        progress = (int32_t)(ratio * 360.0f + 0.5f);
        if (accum > 0.0f && progress < 1) progress = 1;
      }
      if (progress > 360) progress = 360;
      if (progress < 0) progress = 0;
      lv_arc_set_value(arcWidget, progress);
    }
  } else {
    update_info_panel();
    if (angleLabel) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.0f deg", currentAngle);
      lv_label_set_text(angleLabel, buf);
    }
  }
}
