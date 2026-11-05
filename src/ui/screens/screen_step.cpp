// TIG Rotator Controller - Step Mode Screen
// Kinematics: STEPS and moves use angleToSteps(); RPM uses speed_* -> rpmToStepHzCalibrated().
// Same GEAR_RATIO (1:108 per motor.worm.svg), d_emne/D_RULLE, microsteps, calibration as rest of app.

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../tabular_font.h"
#include "../value_binding.h"
#include "../value_format.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../motor/motor.h"
#include "../../config.h"
#include "../../safety/safety.h"
#include "../test_screen_logic.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static const float STEP_PRESET_DEG[4] = {45.0f, 90.0f, 180.0f, 360.0f};

static constexpr int kCardX = 308, kCardW = 468, kCardH = 76, kArcSize = 216;
static constexpr float kAngleAdjustDeg = 5.0f;

static float currentAngle = 90.0f;
static float targetRpm = 2.0f;  // synced in screen_step_create from speed_get_target_rpm()
static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* arcWidget = nullptr;
static lv_obj_t* angleArcLabel = nullptr;
static UiTextBinding<32> angleArcText, angleText, actionText;
static lv_obj_t* stepDirIcon = nullptr;
static lv_obj_t* stepStopBtn = nullptr;
static lv_obj_t* stepBackBtn = nullptr;
static int settingsEnabled = -1;
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
    lv_obj_t* old = diaKb;
    diaKb = nullptr;
    lv_obj_delete_async(old);
  }
  if (diaTa) {
    lv_obj_t* old = diaTa;
    diaTa = nullptr;
    lv_obj_delete_async(old);
  }
  if (diaHint) {
    lv_obj_t* old = diaHint;
    diaHint = nullptr;
    lv_obj_delete_async(old);
  }
  diaClosePending = false;
}

static void step_clamp_target_rpm(void) {
  float cap = speed_get_rpm_max();
  if (targetRpm < speed_get_rpm_min()) targetRpm = speed_get_rpm_min();
  if (targetRpm > cap) targetRpm = cap;
}

static void step_push_rpm_to_speed_and_label(void) {
  step_clamp_target_rpm();
  speed_slider_set(targetRpm);
  ui_set_rpm_unit(rpmLabel, targetRpm);
}

static void refresh_diameter_summary() {
  if (!diameterSummaryLabel) return;
  float mm = speed_get_workpiece_diameter_mm();
  if (mm < 1.0f) {
    lv_label_set_text_fmt(diameterSummaryLabel, "Diameter %.0f mm (default)", (double)(D_EMNE * 1000.0f));
  } else {
    char buf[56];
    snprintf(buf, sizeof(buf), "Diameter %.0f mm", (double)mm);
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
  const char* text = (d == DIR_CW) ? "CW" : "CCW";
  if (strcmp(lv_label_get_text(stepDirValLbl), text) != 0) lv_label_set_text(stepDirValLbl, text);
  ui_set_rotation_icon(stepDirIcon, d);
}

static void update_info_panel() {
  if (angleArcLabel) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%.5g\xC2\xB0", currentAngle);
    angleArcText.set(buf);
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
static void remember_angle() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const bool changed = g_settings.step_angle != currentAngle;
  g_settings.step_angle = currentAngle;
  xSemaphoreGive(g_settings_mutex);
  if (changed) storage_save_settings();
}

static void back_event_cb(lv_event_t* e) {
  remember_angle();
  purge_diameter_overlay_async();
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
  screens_show(SCREEN_MAIN);
}

static void preset_cb(lv_event_t* e) {
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  if (index < 4 && ui_control_state() == STATE_IDLE) {
    control_reset_step_accumulator();
    currentAngle = STEP_PRESET_DEG[index];
    activePreset = index;
    update_preset_styles();
    update_info_panel();
  }
}

static void adjust_angle(float delta) {
  if (ui_control_state() != STATE_IDLE) return;
  float angle = currentAngle + delta;
  if (angle < kAngleAdjustDeg) angle = kAngleAdjustDeg;
  if (angle > 3600.0f) angle = 3600.0f;
  control_reset_step_accumulator();
  currentAngle = angle;
  activePreset = 4;
  for (int i = 0; i < 4; i++)
    if (STEP_PRESET_DEG[i] == angle) activePreset = i;
  update_preset_styles();
  update_info_panel();
}
static void angle_minus_cb(lv_event_t*) { adjust_angle(-kAngleAdjustDeg); }
static void angle_plus_cb(lv_event_t*) { adjust_angle(kAngleAdjustDeg); }

static void show_entry_error(lv_obj_t* hint, const char* message) {
  if (!hint) return;
  lv_label_set_text(hint, message);
  lv_obj_set_style_text_color(hint, g_col_red, 0);
}

static void custom_keyboard_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (customTa) {
      const char* txt = lv_textarea_get_text(customTa);
      float val = step_parse_first_float(txt);
      if (ui_control_state() == STATE_IDLE && !(val > 0.0f && step_angle_valid(val))) {
        show_entry_error(customHint, "Enter an angle above 0, up to 3600 degrees");
        return;
      }
      if (ui_control_state() == STATE_IDLE) {
        control_reset_step_accumulator();
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
      if (ui_control_state() == STATE_IDLE && !(mm >= 1.0f && mm <= 20000.0f)) {
        show_entry_error(diaHint, "Enter a diameter from 1 to 20000 mm");
        return;
      }
      if (ui_control_state() == STATE_IDLE) {
        speed_set_workpiece_diameter_mm(mm);
        xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
        g_settings.workpiece_diameter_mm = mm;
        xSemaphoreGive(g_settings_mutex);
        storage_request_settings_save();
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
  if (ui_control_state() != STATE_IDLE) return;
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

static void set_settings_enabled(lv_obj_t* obj, bool enabled) {
  if (obj == stepActionBtn || obj == stepStopBtn || obj == stepBackBtn) return;
  if (lv_obj_check_type(obj, &lv_button_class)) lv_obj_set_disabled(obj, !enabled);
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) set_settings_enabled(lv_obj_get_child(obj, i), enabled);
}

static void target_card_cb(lv_event_t* e) {
  (void)e;
  if (ui_control_state() != STATE_IDLE) return;
  custom_btn_cb(nullptr);
}

static void rpm_minus_cb(lv_event_t* e) {
  (void)e;
  if (ui_control_state() != STATE_IDLE) return;
  targetRpm -= ui_rpm_increment(targetRpm);
  step_push_rpm_to_speed_and_label();
}

static void rpm_plus_cb(lv_event_t* e) {
  (void)e;
  if (ui_control_state() != STATE_IDLE) return;
  targetRpm += ui_rpm_increment(targetRpm);
  step_push_rpm_to_speed_and_label();
}

static void step_event_cb(lv_event_t* e) {
  (void)e;
  if (ui_control_state() != STATE_IDLE) return;
  step_push_rpm_to_speed_and_label();
  remember_angle();
  control_start_step(currentAngle);
}

static void stop_event_cb(lv_event_t* e) {
  (void)e;
  control_stop();
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_value_card(lv_obj_t* screen, int y, const char* title, lv_obj_t** value, lv_event_cb_t minusCb, lv_event_cb_t plusCb) {
  lv_obj_t* card = ui_create_post_card(screen, kCardX, y, kCardW, kCardH);
  lv_obj_set_scrollable(card, false);
  ui_create_text(card, 16, 8, 200, title, FONT_SUBTITLE, COL_TEXT_DIM);
  *value = ui_create_text(card, 16, 32, 230, "", FONT_XXL, COL_TEXT);
  lv_obj_set_size(ui_create_pm_btn(card, kCardW - 215, 9, "-", FONT_XL, UI_BTN_NORMAL, minusCb, nullptr), 100, 56);
  lv_obj_set_size(ui_create_pm_btn(card, kCardW - 109, 9, "+", FONT_XL, UI_BTN_NORMAL, plusCb, nullptr), 100, 56);
  return card;
}

void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];
  screen_step_invalidate_widgets();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  targetRpm = speed_get_target_rpm();
  step_clamp_target_rpm();
  static bool savedAngleLoaded = false;
  if (!savedAngleLoaded) {
    savedAngleLoaded = true;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    currentAngle = g_settings.step_angle;
    xSemaphoreGive(g_settings_mutex);
    activePreset = 4;
    for (int i = 0; i < 4; i++)
      if (STEP_PRESET_DEG[i] == currentAngle) activePreset = i;
  }

  ui_create_header(screen, "Step mode", "", nullptr);

  lv_obj_t* leftCard = ui_create_post_card(screen, 24, 96, 268, 278);
  lv_obj_set_scrollable(leftCard, false);
  arcWidget = lv_arc_create(leftCard);
  lv_obj_set_size(arcWidget, kArcSize, kArcSize);
  lv_obj_center(arcWidget);
  lv_arc_set_range(arcWidget, 0, 360);
  lv_arc_set_rotation(arcWidget, 270);
  lv_arc_set_bg_angles(arcWidget, 0, 360);
  lv_arc_set_value(arcWidget, (int32_t)currentAngle);
  lv_obj_set_style_arc_color(arcWidget, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcWidget, 16, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arcWidget, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcWidget, 16, LV_PART_MAIN);
  lv_obj_set_style_opa(arcWidget, 0, LV_PART_KNOB);
  lv_obj_set_clickable(arcWidget, false);
  lv_obj_set_scrollable(arcWidget, false);

  angleArcLabel = lv_label_create(leftCard);
  lv_obj_set_style_text_font(angleArcLabel, FONT_HUGE, 0);
  lv_obj_set_width(angleArcLabel, kArcSize - 40);
  lv_obj_set_style_text_color(angleArcLabel, COL_TEXT, 0);
  lv_obj_set_style_text_align(angleArcLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(angleArcLabel);
  angleArcText.bind(angleArcLabel);

  lv_obj_t* targetCard = create_value_card(screen, 96, "Target (calc.)", &angleLabel, angle_minus_cb, angle_plus_cb);
  lv_obj_set_clickable(targetCard, true);
  lv_obj_add_event_cb(targetCard, target_card_cb, LV_EVENT_CLICKED, nullptr);
  angleText.bind(angleLabel);

  lv_obj_t* rpmCard = create_value_card(screen, 182, "Speed", &rpmLabel, rpm_minus_cb, rpm_plus_cb);
  lv_obj_set_clickable(rpmCard, false);
  stepDirValLbl = ui_create_text(rpmCard, 86, 8, 48, "", FONT_SUBTITLE, COL_GREEN);
  stepDirIcon = ui_create_rotation_icon(rpmCard, 24, COL_GREEN, DIR_CW);
  lv_obj_set_pos(stepDirIcon, 132, 5);
  step_refresh_dir_lbl();

  stepsLabel = nullptr;

  const char* presetTexts[] = {"45\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "360\xC2\xB0"};
  for (int i = 0; i < 4; i++) {
    const bool isActive = (i == activePreset);
    presetBtns[i] = ui_create_btn(screen, kCardX + i * 119, 268, 111, 50, presetTexts[i], FONT_BTN,
                                  isActive ? UI_BTN_ACCENT : UI_BTN_NORMAL, preset_cb, (void*)(intptr_t)i);
  }
  lv_obj_t* diaBtn = ui_create_btn(screen, kCardX, 326, kCardW, 48, "", FONT_SUBTITLE, UI_BTN_NORMAL, diameter_btn_cb, nullptr);
  diameterSummaryLabel = lv_obj_get_child(diaBtn, 0);

  stepBackBtn = ui_create_btn(screen, 24, 408, 152, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  settingsEnabled = -1;
  stepActionBtn = ui_create_btn(screen, 192, 408, 584, 56, "", FONT_LARGE, UI_BTN_ACCENT, step_event_cb, nullptr);
  stepStopBtn = ui_create_btn(screen, 192, 408, 584, 56, UI_LABEL_STOP, FONT_LARGE, UI_BTN_DANGER, stop_event_cb, nullptr);
  lv_obj_set_user_data(stepActionBtn, (void*)(uintptr_t)UI_ACTION_START);
  lv_obj_set_user_data(stepStopBtn, (void*)(uintptr_t)UI_ACTION_STOP);
  lv_obj_set_hidden(stepStopBtn, true);
  actionText.bind(lv_obj_get_child(stepActionBtn, 0));

  update_info_panel();
  refresh_diameter_summary();
  step_push_rpm_to_speed_and_label();
}

void screen_step_invalidate_widgets() {
  angleArcText.reset();
  angleText.reset();
  actionText.reset();
  stepActionBtn = nullptr;
  stepStopBtn = nullptr;
  stepBackBtn = nullptr;
  stepDirIcon = nullptr;
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
  ui_mark_motion_callback(screenRoots[SCREEN_STEP], step_event_cb);
  if (numpadClosePending) {
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
  }
  if (diaClosePending) {
    if (diaKb) {
      lv_obj_t* old = diaKb;
      diaKb = nullptr;
      lv_obj_delete_async(old);
    }
    if (diaTa) {
      lv_obj_t* old = diaTa;
      diaTa = nullptr;
      lv_obj_delete_async(old);
    }
    if (diaHint) {
      lv_obj_t* old = diaHint;
      diaHint = nullptr;
      lv_obj_delete_async(old);
    }
    diaClosePending = false;
  }
  if (!screens_is_active(SCREEN_STEP)) return;

  const SystemState actionState = ui_control_state();
  const bool moving = actionState != STATE_IDLE && actionState != STATE_ESTOP;
  const bool inhibited = !ui_control_fresh() || safety_inhibit_motion() || safety_is_estop_locked() || actionState == STATE_ESTOP;
  if (stepActionBtn) {
    const bool feasible = angleToSteps(currentAngle) > 0 && motor_milli_hz_for_rpm_calibrated(targetRpm) != 0;
    char action[32];
    snprintf(action, sizeof(action), LV_SYMBOL_PLAY "  Step %.5g\xC2\xB0", currentAngle);
    actionText.set(inhibited ? "Start blocked" : feasible ? action : "Out of range");
    lv_obj_set_disabled(stepActionBtn, actionState != STATE_IDLE || !feasible || inhibited);
    const bool showStop = moving || !ui_control_fresh();
    lv_obj_set_hidden(stepActionBtn, showStop);
    lv_obj_set_hidden(stepStopBtn, !showStop);
    const int enabled = actionState == STATE_IDLE ? 1 : 0;
    if (enabled != settingsEnabled) {
      settingsEnabled = enabled;
      set_settings_enabled(screenRoots[SCREEN_STEP], enabled);
    }
  }

  SystemState state = ui_control_state();
  if (state == STATE_STEP) {
    float accum = ui_control_view().progress_degrees;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f / %.5g\xC2\xB0", accum, currentAngle);
    angleText.set(buf);
    snprintf(buf, sizeof(buf), "%.0f\xC2\xB0\nof %.5g\xC2\xB0", accum, currentAngle);
    angleArcText.set(buf);
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
      snprintf(buf, sizeof(buf), "%.5g\xC2\xB0", currentAngle);
      angleText.set(buf);
    }
  }
  step_refresh_dir_lbl();
}
