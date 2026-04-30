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
static lv_obj_t* presetBtns[4] = {nullptr};
static lv_obj_t* stepDirValLbl = nullptr;
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
static void purge_diameter_overlay_async() {
  if (diaKb) {
    lv_obj_t* old = diaKb; diaKb = nullptr; lv_obj_delete_async(old);
  }
  if (diaTa) {
    lv_obj_t* old = diaTa; diaTa = nullptr; lv_obj_delete_async(old);
  }
  if (diaHint) {
    lv_obj_t* old = diaHint; diaHint = nullptr; lv_obj_delete_async(old);
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
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.2f", (double)targetRpm);
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
  for (int i = 0; i < 4; i++) {
    if (!presetBtns[i]) continue;
    bool isActive = (activePreset == i);
    const UiBtnStyle ms = isActive ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(presetBtns[i], ms);
    lv_obj_t* lbl = lv_obj_get_child(presetBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(ms), 0);
  }
}

static void step_refresh_dir_lbl(void) {
  if (!stepDirValLbl) return;
  Direction d = speed_get_direction();
  lv_label_set_text(stepDirValLbl, (d == DIR_CW) ? "DIR CW" : "DIR CCW");
}

static void update_info_panel() {
  // Update angle arc label (POST mockup: number + DEG on second line when idle)
  if (angleArcLabel) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f\nDEG", currentAngle);
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
  purge_diameter_overlay_async();
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
  (void)e;
  if (!customNumpad) {
    purge_diameter_overlay_async();
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

static void target_card_cb(lv_event_t* e) {
  (void)e;
  custom_btn_cb(nullptr);
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
  screen_step_invalidate_widgets();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  targetRpm = speed_get_target_rpm();
  step_clamp_target_rpm();

  ui_create_settings_header(screen, "STEP MODE", "ANGLE MOVE", COL_HDR_MUTED);

  // POST mockup #6: gauge left, stack right; keep x+w <= SCREEN_W (800) for presets + aux row + footer
  const int kProSz = 276;
  const int kGaugeX = 20;
  const int kGaugeY = HEADER_H + 20;
  const int kTargetX = kGaugeX + kProSz + 16;
  const int kTargetW = SCREEN_W - kTargetX - 20;
  const int kTargetY = kGaugeY;
  const int kTargetH = 70;
  const int kRpmY = kTargetY + kTargetH + 20;
  const int kPresetH = 44;
  const int kPresetW = 86;
  const int kPresetGap = 10;
  const int kPresetY = kRpmY + kTargetH + 22;
  const int kAuxY = kPresetY + kPresetH + 10;
  const int kFooterY = 414;
  const int kFooterH = 50;
  const int kFooterW = 246;

  lv_obj_t* leftCard = ui_create_post_card(screen, kGaugeX, kGaugeY, kProSz, kProSz);
  lv_obj_remove_flag(leftCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* protractor = lv_obj_create(leftCard);
  lv_obj_set_size(protractor, kProSz, kProSz);
  lv_obj_set_pos(protractor, 0, 0);
  lv_obj_set_style_bg_color(protractor, COL_PROTRACTOR_BG, 0);
  lv_obj_set_style_radius(protractor, kProSz / 2, 0);
  lv_obj_set_style_border_width(protractor, 1, 0);
  lv_obj_set_style_border_color(protractor, COL_BORDER, 0);
  lv_obj_set_style_pad_all(protractor, 0, 0);
  lv_obj_remove_flag(protractor, LV_OBJ_FLAG_SCROLLABLE);

  {
    lv_obj_t* tick0 = lv_line_create(protractor);
    static const lv_point_precise_t pts0[] = {{258, 138}, {244, 138}};
    lv_line_set_points(tick0, pts0, 2);
    lv_obj_set_style_line_color(tick0, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick0, 2, 0);

    lv_obj_t* tick90 = lv_line_create(protractor);
    static const lv_point_precise_t pts90[] = {{138, 258}, {138, 244}};
    lv_line_set_points(tick90, pts90, 2);
    lv_obj_set_style_line_color(tick90, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick90, 2, 0);

    lv_obj_t* tick180 = lv_line_create(protractor);
    static const lv_point_precise_t pts180[] = {{18, 138}, {32, 138}};
    lv_line_set_points(tick180, pts180, 2);
    lv_obj_set_style_line_color(tick180, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick180, 2, 0);

    lv_obj_t* tick270 = lv_line_create(protractor);
    static const lv_point_precise_t pts270[] = {{138, 18}, {138, 32}};
    lv_line_set_points(tick270, pts270, 2);
    lv_obj_set_style_line_color(tick270, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick270, 2, 0);
  }

  arcWidget = lv_arc_create(protractor);
  lv_obj_set_size(arcWidget, kProSz - 36, kProSz - 36);
  lv_obj_align(arcWidget, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arcWidget, 0, 360);
  lv_arc_set_value(arcWidget, (int32_t)currentAngle);
  lv_arc_set_bg_angles(arcWidget, 0, 360);
  lv_obj_set_style_arc_color(arcWidget, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcWidget, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arcWidget, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcWidget, 12, LV_PART_MAIN);
  lv_obj_set_style_opa(arcWidget, 0, LV_PART_KNOB);
  lv_obj_remove_flag(arcWidget, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(arcWidget, LV_OBJ_FLAG_SCROLLABLE);

  angleArcLabel = lv_label_create(protractor);
  {
    char angleBuf[24];
    snprintf(angleBuf, sizeof(angleBuf), "%.0f\nDEG", currentAngle);
    lv_label_set_text(angleArcLabel, angleBuf);
  }
  lv_obj_set_style_text_font(angleArcLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(angleArcLabel, COL_ACCENT, 0);
  lv_obj_set_style_text_align(angleArcLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(angleArcLabel);

  lv_obj_t* targetCard = ui_create_post_card(screen, kTargetX, kTargetY, kTargetW, kTargetH);
  lv_obj_remove_flag(targetCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(targetCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(targetCard, target_card_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* targetLbl = lv_label_create(targetCard);
  lv_label_set_text(targetLbl, "TARGET");
  lv_obj_set_style_text_font(targetLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(targetLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(targetLbl, 24, 14);

  angleLabel = lv_label_create(targetCard);
  {
    char targetBuf[20];
    snprintf(targetBuf, sizeof(targetBuf), "%.0f deg", currentAngle);
    lv_label_set_text(angleLabel, targetBuf);
  }
  lv_obj_set_style_text_font(angleLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(angleLabel, COL_ACCENT, 0);
  lv_obj_set_pos(angleLabel, 144, 18);

  lv_obj_t* rpmCard = ui_create_post_card(screen, kTargetX, kRpmY, kTargetW, kTargetH);
  lv_obj_remove_flag(rpmCard, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* rpmTitle = lv_label_create(rpmCard);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, 24, 14);

  rpmLabel = lv_label_create(rpmCard);
  lv_label_set_text_fmt(rpmLabel, "%.2f", (double)targetRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_set_pos(rpmLabel, 144, 18);

  stepDirValLbl = lv_label_create(rpmCard);
  lv_obj_set_style_text_font(stepDirValLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(stepDirValLbl, COL_GREEN, 0);
  lv_obj_set_pos(stepDirValLbl, 304, 24);
  step_refresh_dir_lbl();

  stepsLabel = nullptr;

  const int kPmW = BTN_W_PM * 2 + 8;
  ui_create_pm_btn(screen, kTargetX, kAuxY, "-", FONT_NORMAL, UI_BTN_NORMAL, rpm_minus_cb, nullptr);
  ui_create_pm_btn(screen, kTargetX + BTN_W_PM + 8, kAuxY, "+", FONT_NORMAL, UI_BTN_ACCENT, rpm_plus_cb,
                   nullptr);

  diameterSummaryLabel = lv_label_create(screen);
  lv_label_set_long_mode(diameterSummaryLabel, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_width(diameterSummaryLabel, 128);
  lv_obj_set_style_text_font(diameterSummaryLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(diameterSummaryLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(diameterSummaryLabel, kTargetX + kPmW + 6, kAuxY + 10);

  lv_obj_t* diaTapBtn = lv_button_create(screen);
  lv_obj_set_size(diaTapBtn, 72, 40);
  lv_obj_set_pos(diaTapBtn, kTargetX + kPmW + 6 + 128 + 6, kAuxY);
  ui_btn_style_post(diaTapBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(diaTapBtn, diameter_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* diaTapLbl = lv_label_create(diaTapBtn);
  lv_label_set_text(diaTapLbl, "DIA");
  lv_obj_set_style_text_font(diaTapLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(diaTapLbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(diaTapLbl);

  lv_obj_t* resetBtn = lv_button_create(screen);
  lv_obj_set_size(resetBtn, 72, 40);
  lv_obj_set_pos(resetBtn, kTargetX + kPmW + 6 + 128 + 6 + 72 + 6, kAuxY);
  ui_btn_style_post(resetBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(resetBtn, step_reset_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* resetLbl = lv_label_create(resetBtn);
  lv_label_set_text(resetLbl, "RST");
  lv_obj_set_style_text_font(resetLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(resetLbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(resetLbl);

  const char* presetTexts[] = {"45", "90", "180", "360"};
  const int presetX0 = kTargetX;
  for (int i = 0; i < 4; i++) {
    presetBtns[i] = lv_button_create(screen);
    lv_obj_set_size(presetBtns[i], kPresetW, kPresetH);
    lv_obj_set_pos(presetBtns[i], presetX0 + i * (kPresetW + kPresetGap), kPresetY);
    lv_obj_set_style_radius(presetBtns[i], RADIUS_BTN, 0);
    lv_obj_add_event_cb(presetBtns[i], preset_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    bool isActive = (i == activePreset);
    ui_btn_style_post(presetBtns[i], isActive ? UI_BTN_ACCENT : UI_BTN_NORMAL);

    lv_obj_t* lbl = lv_label_create(presetBtns[i]);
    lv_label_set_text(lbl, presetTexts[i]);
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(isActive ? UI_BTN_ACCENT : UI_BTN_NORMAL), 0);
    lv_obj_center(lbl);
  }

  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, kFooterW, kFooterH);
  lv_obj_set_pos(backBtn, 20, kFooterY);
  ui_btn_style_post(backBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "< BACK");
  lv_obj_set_style_text_font(backLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(backLabel, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(backLabel);

  lv_obj_t* stepBtn = lv_button_create(screen);
  lv_obj_set_size(stepBtn, kFooterW, kFooterH);
  lv_obj_set_pos(stepBtn, 277, kFooterY);
  ui_btn_style_post(stepBtn, UI_BTN_ACCENT);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);
  stepActionBtn = stepBtn;
  lv_obj_t* stepLabel = lv_label_create(stepBtn);
  lv_label_set_text(stepLabel, "> STEP");
  lv_obj_set_style_text_font(stepLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stepLabel, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  lv_obj_center(stepLabel);

  lv_obj_t* stopBtn = lv_button_create(screen);
  lv_obj_set_size(stopBtn, kFooterW, kFooterH);
  lv_obj_set_pos(stopBtn, 534, kFooterY);
  ui_btn_style_post(stopBtn, UI_BTN_DANGER);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "X STOP");
  lv_obj_set_style_text_font(stopLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stopLabel, ui_btn_label_color_post(UI_BTN_DANGER), 0);
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
  stepDirValLbl = nullptr;
  diameterSummaryLabel = nullptr;
  for (int i = 0; i < 4; i++) presetBtns[i] = nullptr;
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
  step_refresh_dir_lbl();
}
