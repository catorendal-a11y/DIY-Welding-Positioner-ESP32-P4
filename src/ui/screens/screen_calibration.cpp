// TIG Rotator Controller - Calibration Screen
// Step-by-step calibration wizard: zero, rotate 360, verify, save

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/calibration.h"
#include "../../motor/microstep.h"
#include <cstdio>

static lv_obj_t* zeroValueLabel = nullptr;
static lv_obj_t* stepsDegValueLabel = nullptr;
static lv_obj_t* errorValueLabel = nullptr;
static lv_obj_t* toleranceValueLabel = nullptr;
static lv_obj_t* resultValueLabel = nullptr;
static lv_obj_t* progressBar = nullptr;
static lv_obj_t* checkLabels[4] = { nullptr };
static lv_obj_t* checkDots[4] = { nullptr };
static lv_obj_t* progressLine = nullptr;

static const int STEP_NONE = 0;
static const int STEP_ZERO = 1;
static const int STEP_ROTATE = 2;
static const int STEP_VERIFY = 3;
static const int STEP_SAVE = 4;
static int calStep = STEP_NONE;

static void back_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

static void restart_cb(lv_event_t* e) {
  calStep = STEP_ZERO;
  calibration_set_factor(1.0f);
  screen_calibration_update();
}

static void save_cb(lv_event_t* e) {
  calibration_save();
  calStep = STEP_SAVE;
  screen_calibration_update();
}

static void update_checklist() {
  lv_color_t doneCol = COL_GREEN;
  lv_color_t activeCol = lv_color_hex(0xFF9500);
  lv_color_t pendingCol = COL_TEXT_VDIM;

  const char* marks[] = { "[+]", "[+]", "[>]", "[ ]" };
  const char* texts[] = { "Set zero position", "Rotate 360 deg reference",
                          "Verify accuracy", "Save calibration" };

  for (int i = 0; i < 4; i++) {
    int stepNum = i + 1;
    bool done = (calStep > stepNum);
    bool active = (calStep == stepNum);

    if (checkLabels[i]) {
      lv_color_t col = done ? doneCol : (active ? activeCol : pendingCol);
      char buf[40];
      snprintf(buf, sizeof(buf), "%s %s", done ? "[+]" : (active ? "[>]" : "[ ]"), texts[i]);
      lv_label_set_text(checkLabels[i], buf);
      lv_obj_set_style_text_color(checkLabels[i], col, 0);
    }
    if (checkDots[i]) {
      lv_color_t col = done ? doneCol : (active ? activeCol : pendingCol);
      lv_obj_set_style_bg_color(checkDots[i], col, 0);
    }
  }
}

void screen_calibration_create() {
  lv_obj_t* screen = screenRoots[SCREEN_CALIBRATION];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 28);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "CALIBRATION");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PX, 6);

  int y = 36;

  lv_obj_t* leftCard = lv_obj_create(screen);
  lv_obj_set_size(leftCard, 376, 80);
  lv_obj_set_pos(leftCard, PX, y);
  lv_obj_set_style_bg_color(leftCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(leftCard, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(leftCard, 1, 0);
  lv_obj_set_style_radius(leftCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(leftCard, 0, 0);
  lv_obj_set_style_shadow_width(leftCard, 0, 0);
  lv_obj_remove_flag(leftCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(leftCard, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* encSecLabel = lv_label_create(leftCard);
  lv_label_set_text(encSecLabel, "ENCODER ZERO");
  lv_obj_set_style_text_font(encSecLabel, FONT_BODY, 0);
  lv_obj_set_style_text_color(encSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(encSecLabel, 12, 8);

  zeroValueLabel = lv_label_create(leftCard);
  lv_label_set_text(zeroValueLabel, "0.00 deg");
  lv_obj_set_style_text_font(zeroValueLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(zeroValueLabel, COL_GREEN, 0);
  lv_obj_set_pos(zeroValueLabel, 12, 26);

  lv_obj_t* rightCard = lv_obj_create(screen);
  lv_obj_set_size(rightCard, 376, 80);
  lv_obj_set_pos(rightCard, PX + 388, y);
  lv_obj_set_style_bg_color(rightCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(rightCard, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(rightCard, 1, 0);
  lv_obj_set_style_radius(rightCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(rightCard, 0, 0);
  lv_obj_set_style_shadow_width(rightCard, 0, 0);
  lv_obj_remove_flag(rightCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(rightCard, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* spdSecLabel = lv_label_create(rightCard);
  lv_label_set_text(spdSecLabel, "STEPS/DEGREE");
  lv_obj_set_style_text_font(spdSecLabel, FONT_BODY, 0);
  lv_obj_set_style_text_color(spdSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(spdSecLabel, 12, 8);

  stepsDegValueLabel = lv_label_create(rightCard);
  lv_label_set_text(stepsDegValueLabel, "0.00");
  lv_obj_set_style_text_font(stepsDegValueLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(stepsDegValueLabel, COL_TEXT, 0);
  lv_obj_set_pos(stepsDegValueLabel, 12, 26);

  y += 88;

  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SCREEN_W - 2 * PX, 1);
  lv_obj_set_pos(sep, PX, y);
  lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_CLICKABLE);

  y += 8;

  progressLine = lv_obj_create(screen);
  lv_obj_set_size(progressLine, 2, 4 * 36 - 8);
  lv_obj_set_pos(progressLine, PX + 8, y + 4);
  lv_obj_set_style_bg_color(progressLine, lv_color_hex(0x222222), 0);
  lv_obj_set_style_border_width(progressLine, 0, 0);
  lv_obj_set_style_radius(progressLine, 1, 0);
  lv_obj_set_style_pad_all(progressLine, 0, 0);
  lv_obj_remove_flag(progressLine, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(progressLine, LV_OBJ_FLAG_CLICKABLE);

  const char* texts[] = { "Set zero position", "Rotate 360 deg reference",
                          "Verify accuracy", "Save calibration" };

  for (int i = 0; i < 4; i++) {
    int iy = y + i * 36;

    checkDots[i] = lv_obj_create(screen);
    lv_obj_set_size(checkDots[i], 10, 10);
    lv_obj_set_pos(checkDots[i], PX + 4, iy + 13);
    lv_obj_set_style_bg_color(checkDots[i], COL_TEXT_VDIM, 0);
    lv_obj_set_style_border_width(checkDots[i], 0, 0);
    lv_obj_set_style_radius(checkDots[i], 5, 0);
    lv_obj_set_style_pad_all(checkDots[i], 0, 0);
    lv_obj_remove_flag(checkDots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(checkDots[i], LV_OBJ_FLAG_CLICKABLE);

    checkLabels[i] = lv_label_create(screen);
    char buf[40];
    snprintf(buf, sizeof(buf), "[ ] %s", texts[i]);
    lv_label_set_text(checkLabels[i], buf);
    lv_obj_set_style_text_font(checkLabels[i], FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(checkLabels[i], COL_TEXT_VDIM, 0);
    lv_obj_set_pos(checkLabels[i], PX + 24, iy + 10);
  }

  y += 4 * 36 + 8;

  lv_obj_t* infoBar = lv_obj_create(screen);
  lv_obj_set_size(infoBar, SCREEN_W - 2 * PX, 56);
  lv_obj_set_pos(infoBar, PX, y);
  lv_obj_set_style_bg_color(infoBar, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(infoBar, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(infoBar, 1, 0);
  lv_obj_set_style_radius(infoBar, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(infoBar, 0, 0);
  lv_obj_set_style_shadow_width(infoBar, 0, 0);
  lv_obj_remove_flag(infoBar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(infoBar, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* errLabel = lv_label_create(infoBar);
  lv_label_set_text(errLabel, "ERROR:");
  lv_obj_set_style_text_font(errLabel, FONT_BODY, 0);
  lv_obj_set_style_text_color(errLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(errLabel, 12, 6);

  errorValueLabel = lv_label_create(infoBar);
  lv_label_set_text(errorValueLabel, "+0.00 deg");
  lv_obj_set_style_text_font(errorValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(errorValueLabel, COL_GREEN, 0);
  lv_obj_set_pos(errorValueLabel, 12, 20);

  lv_obj_t* tolLabel = lv_label_create(infoBar);
  lv_label_set_text(tolLabel, "TOLERANCE:");
  lv_obj_set_style_text_font(tolLabel, FONT_BODY, 0);
  lv_obj_set_style_text_color(tolLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(tolLabel, 200, 6);

  toleranceValueLabel = lv_label_create(infoBar);
  lv_label_set_text(toleranceValueLabel, "+/-0.5 deg");
  lv_obj_set_style_text_font(toleranceValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(toleranceValueLabel, COL_TEXT, 0);
  lv_obj_set_pos(toleranceValueLabel, 200, 20);

  lv_obj_t* resLabel = lv_label_create(infoBar);
  lv_label_set_text(resLabel, "RESULT:");
  lv_obj_set_style_text_font(resLabel, FONT_BODY, 0);
  lv_obj_set_style_text_color(resLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(resLabel, 430, 6);

  resultValueLabel = lv_label_create(infoBar);
  lv_label_set_text(resultValueLabel, "PASS");
  lv_obj_set_style_text_font(resultValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(resultValueLabel, COL_GREEN, 0);
  lv_obj_set_pos(resultValueLabel, 430, 20);

  progressBar = lv_bar_create(infoBar);
  lv_obj_set_size(progressBar, 180, 10);
  lv_obj_set_pos(progressBar, 590, 22);
  lv_bar_set_range(progressBar, 0, 100);
  lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x222222), 0);
  lv_obj_set_style_bg_color(progressBar, COL_GREEN, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(progressBar, 0, 0);
  lv_obj_set_style_radius(progressBar, 2, 0);
  lv_obj_remove_flag(progressBar, LV_OBJ_FLAG_CLICKABLE);

  int footerY = 440;
  int footerH = 36;
  int btnW = 160;
  int gap = 8;

  lv_obj_t* backFooter = lv_button_create(screen);
  lv_obj_set_size(backFooter, btnW, footerH);
  lv_obj_set_pos(backFooter, PX, footerY);
  lv_obj_set_style_bg_color(backFooter, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backFooter, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backFooter, 1, 0);
  lv_obj_set_style_border_color(backFooter, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(backFooter, 0, 0);
  lv_obj_set_style_pad_all(backFooter, 0, 0);
  lv_obj_add_event_cb(backFooter, back_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* bLbl = lv_label_create(backFooter);
  lv_label_set_text(bLbl, "BACK");
  lv_obj_set_style_text_font(bLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(bLbl, COL_TEXT, 0);
  lv_obj_center(bLbl);

  lv_obj_t* restartBtn = lv_button_create(screen);
  lv_obj_set_size(restartBtn, btnW, footerH);
  lv_obj_set_pos(restartBtn, PX + btnW + gap, footerY);
  lv_obj_set_style_bg_color(restartBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(restartBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(restartBtn, 1, 0);
  lv_obj_set_style_border_color(restartBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(restartBtn, 0, 0);
  lv_obj_set_style_pad_all(restartBtn, 0, 0);
  lv_obj_add_event_cb(restartBtn, restart_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rLbl = lv_label_create(restartBtn);
  lv_label_set_text(rLbl, "RESTART");
  lv_obj_set_style_text_font(rLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rLbl, COL_TEXT, 0);
  lv_obj_center(rLbl);

  lv_obj_t* saveBtn = lv_button_create(screen);
  lv_obj_set_size(saveBtn, btnW + 60, footerH);
  lv_obj_set_pos(saveBtn, PX + 2 * (btnW + gap), footerY);
  lv_obj_set_style_bg_color(saveBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(saveBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(saveBtn, 2, 0);
  lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(saveBtn, 0, 0);
  lv_obj_set_style_pad_all(saveBtn, 0, 0);
  lv_obj_add_event_cb(saveBtn, save_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* sLbl = lv_label_create(saveBtn);
  lv_label_set_text(sLbl, "SAVE CALIBRATION");
  lv_obj_set_style_text_font(sLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(sLbl, COL_ACCENT, 0);
  lv_obj_center(sLbl);

  calStep = STEP_ZERO;
  screen_calibration_update();
}

void screen_calibration_update() {
  float factor = calibration_get_factor();
  uint32_t stepsPerRev = microstep_get_steps_per_rev();
  float stepsPerDeg = (float)stepsPerRev * GEAR_RATIO / 360.0f * factor;

  if (zeroValueLabel) {
    char buf[32];
    snprintf(buf, sizeof(buf), "0.00 deg");
    lv_label_set_text(zeroValueLabel, buf);
  }

  if (stepsDegValueLabel) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", stepsPerDeg);
    lv_label_set_text(stepsDegValueLabel, buf);
  }

  float errorDeg = (factor - 1.0f) * 360.0f;
  float absError = errorDeg < 0 ? -errorDeg : errorDeg;
  float tolerance = 0.5f;

  if (errorValueLabel) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%+.2f deg", errorDeg);
    lv_label_set_text(errorValueLabel, buf);
    lv_color_t col = (absError <= tolerance) ? COL_GREEN : COL_RED;
    lv_obj_set_style_text_color(errorValueLabel, col, 0);
  }

  if (resultValueLabel) {
    if (calStep >= STEP_SAVE) {
      lv_label_set_text(resultValueLabel, "SAVED");
      lv_obj_set_style_text_color(resultValueLabel, COL_GREEN, 0);
    } else if (absError <= tolerance) {
      lv_label_set_text(resultValueLabel, "PASS");
      lv_obj_set_style_text_color(resultValueLabel, COL_GREEN, 0);
    } else {
      lv_label_set_text(resultValueLabel, "FAIL");
      lv_obj_set_style_text_color(resultValueLabel, COL_RED, 0);
    }
  }

  if (progressBar) {
    int pct = (int)(absError / tolerance * 100.0f);
    lv_bar_set_value(progressBar, (pct > 100) ? 100 : pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progressBar, (pct <= 100) ? COL_GREEN : COL_RED, LV_PART_INDICATOR);
  }

  update_checklist();
}
