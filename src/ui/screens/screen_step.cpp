// TIG Rotator Controller - Step Mode Screen
// Protractor arc, info panel, presets row, STEP/STOP action bar

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include "../../storage/storage.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static float currentAngle = 90.0f;
static float targetRpm = 0.5f;
static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* arcWidget = nullptr;
static lv_obj_t* angleArcLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* stepsLabel = nullptr;
static lv_obj_t* calibLabel = nullptr;
static lv_obj_t* presetBtns[7] = {nullptr};
static int activePreset = 1;  // 90 deg is default (index 1)
static lv_obj_t* customNumpad = nullptr;
static lv_obj_t* customTa = nullptr;
static volatile bool numpadClosePending = false;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_preset_styles() {
  const float presetAngles[] = {45.0f, 90.0f, 180.0f, 360.0f, 0.0f};  // 0 = custom
  for (int i = 0; i < 5; i++) {
    if (!presetBtns[i]) continue;
    bool isActive = (i < 4 && currentAngle == presetAngles[i]) || (i == 4 && activePreset == 4);
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
  if (customNumpad) { lv_obj_t* old = customNumpad; customNumpad = nullptr; lv_obj_delete_async(old); }
  if (customTa) { lv_obj_t* old = customTa; customTa = nullptr; lv_obj_delete_async(old); }
  screens_show(SCREEN_MAIN);
}

static void preset_cb(lv_event_t* e) {
  int index = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  const float presetAngles[] = {45.0f, 90.0f, 180.0f, 360.0f};
  if (index < 4) {
    currentAngle = presetAngles[index];
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
      float val = atof(txt);
      if (val > 0.0f) {
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

static void custom_btn_cb(lv_event_t* e) {
  if (!customNumpad) {
    customTa = lv_textarea_create(screenRoots[SCREEN_STEP]);
    lv_obj_set_size(customTa, 200, 50);
    lv_obj_align(customTa, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_one_line(customTa, true);
    lv_textarea_set_accepted_chars(customTa, "0123456789.");
    lv_obj_set_style_bg_color(customTa, COL_BG, 0);
    lv_obj_set_style_border_color(customTa, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customTa, 2, 0);
    lv_obj_set_style_text_color(customTa, COL_TEXT, 0);

    customNumpad = lv_keyboard_create(screenRoots[SCREEN_STEP]);
    lv_keyboard_set_mode(customNumpad, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(customNumpad, customTa);

    lv_obj_set_style_bg_color(customNumpad, COL_BG, 0);
    lv_obj_set_style_border_color(customNumpad, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customNumpad, 2, 0);
    lv_obj_add_event_cb(customNumpad, custom_keyboard_cb, LV_EVENT_ALL, nullptr);
  }
}

static void rpm_minus_cb(lv_event_t* e) {
  if (targetRpm > 0.1f) targetRpm -= 0.1f;
  if (targetRpm < MIN_RPM) targetRpm = MIN_RPM;
  if (targetRpm > speed_get_rpm_max()) targetRpm = speed_get_rpm_max();
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f RPM", targetRpm);
}

static void rpm_plus_cb(lv_event_t* e) {
  targetRpm += 0.1f;
  if (targetRpm < MIN_RPM) targetRpm = MIN_RPM;
  if (targetRpm > speed_get_rpm_max()) targetRpm = speed_get_rpm_max();
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f RPM", targetRpm);
}

static void calib_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  g_settings.calibration_factor += delta * 0.01f;
  if (g_settings.calibration_factor < 0.5f) g_settings.calibration_factor = 0.5f;
  if (g_settings.calibration_factor > 1.5f) g_settings.calibration_factor = 1.5f;
  if (calibLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", g_settings.calibration_factor);
    lv_label_set_text(calibLabel, buf);
  }
  storage_save_settings();
}

static void step_event_cb(lv_event_t* e) {
  speed_slider_set(targetRpm);
  control_start_step(currentAngle);
}

static void stop_event_cb(lv_event_t* e) {
  control_stop();
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

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
  lv_obj_set_style_text_font(title, FONT_LARGE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 5);

  // ── Left: Protractor circle (cx=210, cy=220, r=140) ──
  // Background circle
  lv_obj_t* protractor = lv_obj_create(screen);
  lv_obj_set_size(protractor, 300, 300);
  lv_obj_set_pos(protractor, 60, 55);
  lv_obj_set_style_bg_color(protractor, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_radius(protractor, 150, 0);
  lv_obj_set_style_border_width(protractor, 1, 0);
  lv_obj_set_style_border_color(protractor, COL_BORDER, 0);
  lv_obj_set_style_pad_all(protractor, 0, 0);
  lv_obj_remove_flag(protractor, LV_OBJ_FLAG_SCROLLABLE);

  // Tick marks at 0/90/180/270 degrees using lines
  // In LVGL arc: 0 deg = right (3 o'clock), clockwise
  // We draw tick marks at 0 (right), 90 (bottom), 180 (left), 270 (top)
  // For each tick: short line near the edge of the circle
  // Circle center in protractor coords: (150, 150), radius ~130 for tick placement
  {
    // Tick line thickness
    const int cx = 150, cy = 150;
    const int rOuter = 140, rInner = 125;

    // 0 deg (right): line from (150+rOuter, 150) to (150+rInner, 150)
    lv_obj_t* tick0 = lv_line_create(protractor);
    static lv_point_precise_t pts0[] = {{cx + rOuter, cy}, {cx + rInner, cy}};
    lv_line_set_points(tick0, pts0, 2);
    lv_obj_set_style_line_color(tick0, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick0, 2, 0);

    // 90 deg (bottom): line from (150, 150+rOuter) to (150, 150+rInner)
    lv_obj_t* tick90 = lv_line_create(protractor);
    static lv_point_precise_t pts90[] = {{cx, cy + rOuter}, {cx, cy + rInner}};
    lv_line_set_points(tick90, pts90, 2);
    lv_obj_set_style_line_color(tick90, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick90, 2, 0);

    // 180 deg (left): line from (150-rOuter, 150) to (150-rInner, 150)
    lv_obj_t* tick180 = lv_line_create(protractor);
    static lv_point_precise_t pts180[] = {{cx - rOuter, cy}, {cx - rInner, cy}};
    lv_line_set_points(tick180, pts180, 2);
    lv_obj_set_style_line_color(tick180, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick180, 2, 0);

    // 270 deg (top): line from (150, 150-rOuter) to (150, 150-rInner)
    lv_obj_t* tick270 = lv_line_create(protractor);
    static lv_point_precise_t pts270[] = {{cx, cy - rOuter}, {cx, cy - rInner}};
    lv_line_set_points(tick270, pts270, 2);
    lv_obj_set_style_line_color(tick270, COL_GAUGE_TICK, 0);
    lv_obj_set_style_line_width(tick270, 2, 0);
  }

  // Arc showing current angle (0 to current value) in COL_ACCENT
  arcWidget = lv_arc_create(protractor);
  lv_obj_set_size(arcWidget, 260, 260);
  lv_obj_align(arcWidget, LV_ALIGN_CENTER, 0, 0);
  lv_arc_set_range(arcWidget, 0, 360);
  lv_arc_set_value(arcWidget, (int32_t)currentAngle);
  lv_arc_set_bg_angles(arcWidget, 0, 360);
  // Indicator color
  lv_obj_set_style_arc_color(arcWidget, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcWidget, 4, LV_PART_INDICATOR);
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

  // ── Right panel (x=410) ──
  const int rightX = 410;
  int rightY = 50;

  // TARGET label
  lv_obj_t* targetLbl = lv_label_create(screen);
  lv_label_set_text(targetLbl, "TARGET");
  lv_obj_set_style_text_font(targetLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(targetLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(targetLbl, rightX, rightY);

  // Target angle value
  angleLabel = lv_label_create(screen);
  char targetBuf[16];
  snprintf(targetBuf, sizeof(targetBuf), "%.0f deg", currentAngle);
  lv_label_set_text(angleLabel, targetBuf);
  lv_obj_set_style_text_font(angleLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(angleLabel, COL_ACCENT, 0);
  lv_obj_set_pos(angleLabel, rightX, rightY + 16);
  rightY += 70;

  // Separator
  lv_obj_t* sep1 = lv_obj_create(screen);
  lv_obj_set_size(sep1, 370, 1);
  lv_obj_set_pos(sep1, rightX, rightY);
  lv_obj_set_style_bg_color(sep1, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(sep1, 0, 0);
  lv_obj_set_style_pad_all(sep1, 0, 0);
  lv_obj_set_style_radius(sep1, 0, 0);
  lv_obj_remove_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);
  rightY += 8;

  // SPEED label + value
  lv_obj_t* speedLbl = lv_label_create(screen);
  lv_label_set_text(speedLbl, "SPEED:");
  lv_obj_set_style_text_font(speedLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(speedLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(speedLbl, rightX, rightY);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", targetRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_set_pos(rpmLabel, rightX + 70, rightY - 2);

  lv_obj_t* rpmUnit = lv_label_create(screen);
  lv_label_set_text(rpmUnit, "RPM");
  lv_obj_set_style_text_font(rpmUnit, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmUnit, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmUnit, rightX + 130, rightY + 2);
  rightY += 30;

  // DIR label + value
  lv_obj_t* dirLbl = lv_label_create(screen);
  lv_label_set_text(dirLbl, "DIR:");
  lv_obj_set_style_text_font(dirLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dirLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(dirLbl, rightX, rightY);

  Direction dir = speed_get_direction();
  lv_obj_t* dirVal = lv_label_create(screen);
  lv_label_set_text(dirVal, (dir == DIR_CW) ? "CW" : "CCW");
  lv_obj_set_style_text_font(dirVal, FONT_LARGE, 0);
  lv_obj_set_style_text_color(dirVal, COL_TEXT, 0);
  lv_obj_set_pos(dirVal, rightX + 50, rightY - 2);
  rightY += 30;

  // STEPS label + value
  lv_obj_t* stepsLbl = lv_label_create(screen);
  lv_label_set_text(stepsLbl, "STEPS:");
  lv_obj_set_style_text_font(stepsLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(stepsLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(stepsLbl, rightX, rightY);

  stepsLabel = lv_label_create(screen);
  long steps = angleToSteps(currentAngle);
  lv_label_set_text_fmt(stepsLabel, "%ld", steps);
  lv_obj_set_style_text_font(stepsLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stepsLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(stepsLabel, rightX + 55, rightY + 1);
  rightY += 30;

  // CALIB label + value + buttons
  lv_obj_t* calibLbl = lv_label_create(screen);
  lv_label_set_text(calibLbl, "CALIB:");
  lv_obj_set_style_text_font(calibLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(calibLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(calibLbl, rightX, rightY);

  calibLabel = lv_label_create(screen);
  char calibBuf[16];
  snprintf(calibBuf, sizeof(calibBuf), "%.2f", g_settings.calibration_factor);
  lv_label_set_text(calibLabel, calibBuf);
  lv_obj_set_style_text_font(calibLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(calibLabel, COL_TEXT, 0);
  lv_obj_set_pos(calibLabel, rightX + 60, rightY - 2);

  lv_obj_t* calibMinusBtn = lv_button_create(screen);
  lv_obj_set_size(calibMinusBtn, 80, 36);
  lv_obj_set_pos(calibMinusBtn, rightX + 120, rightY - 4);
  lv_obj_set_style_bg_color(calibMinusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(calibMinusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(calibMinusBtn, 1, 0);
  lv_obj_set_style_border_color(calibMinusBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(calibMinusBtn, 0, 0);
  lv_obj_set_style_pad_all(calibMinusBtn, 0, 0);
  lv_obj_add_event_cb(calibMinusBtn, calib_adj_cb, LV_EVENT_CLICKED, (void*)-1);
  lv_obj_t* calibMinusLbl = lv_label_create(calibMinusBtn);
  lv_label_set_text(calibMinusLbl, "-1%");
  lv_obj_set_style_text_font(calibMinusLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(calibMinusLbl, COL_TEXT, 0);
  lv_obj_center(calibMinusLbl);

  lv_obj_t* calibPlusBtn = lv_button_create(screen);
  lv_obj_set_size(calibPlusBtn, 80, 36);
  lv_obj_set_pos(calibPlusBtn, rightX + 210, rightY - 4);
  lv_obj_set_style_bg_color(calibPlusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(calibPlusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(calibPlusBtn, 1, 0);
  lv_obj_set_style_border_color(calibPlusBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(calibPlusBtn, 0, 0);
  lv_obj_set_style_pad_all(calibPlusBtn, 0, 0);
  lv_obj_add_event_cb(calibPlusBtn, calib_adj_cb, LV_EVENT_CLICKED, (void*)1);
  lv_obj_t* calibPlusLbl = lv_label_create(calibPlusBtn);
  lv_label_set_text(calibPlusLbl, "+1%");
  lv_obj_set_style_text_font(calibPlusLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(calibPlusLbl, COL_TEXT, 0);
  lv_obj_center(calibPlusLbl);

  // ── Presets row (y=378, 118x36 each) ──
  const int presetY = 378;
  const int presetW = 106;
  const int presetH = 40;
  const int presetGap = 4;
  const int presetStartX = 8;

  const float presetAngles[] = {45.0f, 90.0f, 180.0f, 360.0f};
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
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
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
  lv_obj_set_style_text_font(customLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(customLabel, COL_TEXT, 0);
  lv_obj_center(customLabel);

  // RPM - button (6th)
  presetBtns[5] = lv_button_create(screen);
  lv_obj_set_size(presetBtns[5], presetW, presetH);
  lv_obj_set_pos(presetBtns[5], presetStartX + 5 * (presetW + presetGap), presetY);
  lv_obj_set_style_bg_color(presetBtns[5], COL_BTN_BG, 0);
  lv_obj_set_style_radius(presetBtns[5], RADIUS_BTN, 0);
  lv_obj_set_style_border_width(presetBtns[5], 1, 0);
  lv_obj_set_style_border_color(presetBtns[5], COL_BORDER, 0);
  lv_obj_add_event_cb(presetBtns[5], rpm_minus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* rpmMinusLabel = lv_label_create(presetBtns[5]);
  lv_label_set_text(rpmMinusLabel, "RPM -");
  lv_obj_set_style_text_font(rpmMinusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmMinusLabel, COL_TEXT, 0);
  lv_obj_center(rpmMinusLabel);

  // RPM + button (7th)
  presetBtns[6] = lv_button_create(screen);
  lv_obj_set_size(presetBtns[6], presetW, presetH);
  lv_obj_set_pos(presetBtns[6], presetStartX + 6 * (presetW + presetGap), presetY);
  lv_obj_set_style_bg_color(presetBtns[6], COL_BTN_BG, 0);
  lv_obj_set_style_radius(presetBtns[6], RADIUS_BTN, 0);
  lv_obj_set_style_border_width(presetBtns[6], 1, 0);
  lv_obj_set_style_border_color(presetBtns[6], COL_BORDER, 0);
  lv_obj_add_event_cb(presetBtns[6], rpm_plus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* rpmPlusLabel = lv_label_create(presetBtns[6]);
  lv_label_set_text(rpmPlusLabel, "RPM +");
  lv_obj_set_style_text_font(rpmPlusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmPlusLabel, COL_TEXT, 0);
  lv_obj_center(rpmPlusLabel);

  // ── Bottom bar: BACK + STEP + STOP (y=428) ──
  const int barY = 428;
  const int barH = 48;
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
  lv_obj_set_style_text_font(backLabel, FONT_SUBTITLE, 0);
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

  lv_obj_t* stepLabel = lv_label_create(stepBtn);
  lv_label_set_text(stepLabel, "> STEP");
  lv_obj_set_style_text_font(stepLabel, FONT_SUBTITLE, 0);
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
  lv_obj_set_style_text_font(stopLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(stopLabel, COL_RED, 0);
  lv_obj_center(stopLabel);

  LOG_I("Screen step: protractor layout created");
}

void screen_step_invalidate_widgets() {
  angleLabel = nullptr;
  arcWidget = nullptr;
  angleArcLabel = nullptr;
  rpmLabel = nullptr;
  stepsLabel = nullptr;
  calibLabel = nullptr;
  for (int i = 0; i < 7; i++) presetBtns[i] = nullptr;
  customNumpad = nullptr;
  customTa = nullptr;
  numpadClosePending = false;
}

void screen_step_update() {
  if (numpadClosePending) {
    if (customNumpad) { lv_obj_t* old = customNumpad; customNumpad = nullptr; lv_obj_delete_async(old); }
    if (customTa) { lv_obj_t* old = customTa; customTa = nullptr; lv_obj_delete_async(old); }
    numpadClosePending = false;
  }
  if (!screens_is_active(SCREEN_STEP)) return;

  SystemState state = control_get_state();
  if (state == STATE_STEP) {
    float accum = control_get_step_accumulated();
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f / %.0f deg", accum, currentAngle);
    if (angleArcLabel) lv_label_set_text(angleArcLabel, buf);
    if (angleLabel) {
      char buf2[32];
      snprintf(buf2, sizeof(buf2), "%.0f / %.0f deg", accum, currentAngle);
      lv_label_set_text(angleLabel, buf2);
    }
    // Update arc to show progress
    if (arcWidget) {
      int32_t progress = 0;
      if (currentAngle > 0.0f) {
        progress = (int32_t)((accum / currentAngle) * 360.0f);
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
