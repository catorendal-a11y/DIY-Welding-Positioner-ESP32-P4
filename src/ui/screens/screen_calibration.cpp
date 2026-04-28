// TIG Rotator Controller - Calibration Screen
// Workpiece MOVE 360 (angleToSteps), MEASURED DEG, APPLY (factor *= 360/measured),
// then MOVE 360 again; second MEASURED DEG -> PASS/FAIL vs cmd 360 deg (not factor~=1).
// Top row shows OUT (gear output) and WP (workpiece) steps; MOVE 360 always uses WP.

#include "../screens.h"
#include "../theme.h"
#include "../test_screen_logic.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../motor/calibration.h"
#include "../../motor/microstep.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../safety/safety.h"
#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static lv_obj_t* zeroValueLabel = nullptr;
static lv_obj_t* dualStepsDegLabel = nullptr;
static lv_obj_t* cmdStepsOutLabel = nullptr;
static lv_obj_t* cmdStepsWpLabel = nullptr;
static lv_obj_t* kinGearValueLabel = nullptr;
static lv_obj_t* kinMicroValueLabel = nullptr;
static lv_obj_t* kinGeoLabel = nullptr;
static lv_obj_t* wizardHintLabel = nullptr;
static lv_obj_t* wizardTrack = nullptr;
static lv_obj_t* wizardFill = nullptr;
static lv_obj_t* wizardDots[4] = { nullptr };
static lv_obj_t* measuredFieldBg = nullptr;
static lv_obj_t* measuredTapLabel = nullptr;
static lv_obj_t* applyMeasureBtn = nullptr;
static lv_obj_t* run360Btn = nullptr;
static lv_obj_t* stopMotionBtn = nullptr;
static lv_obj_t* jogMinusBtn = nullptr;
static lv_obj_t* jogPlusBtn = nullptr;
static lv_obj_t* calEntryKb = nullptr;
static lv_obj_t* calEntryTa = nullptr;
static lv_obj_t* calEntryHint = nullptr;
static bool calEntryClosePending = false;
static lv_obj_t* resultBar = nullptr;
static lv_obj_t* resultStatusLabel = nullptr;
static lv_obj_t* resultDetailLabel = nullptr;
static lv_obj_t* resultReadyLabel = nullptr;

static const float kCalCommandedDeg = 360.0f;
static const float kVerifyTolDeg = 0.5f;
static float s_measuredDeg = 0.0f;
static float s_verifyMeasuredDeg = 0.0f;

static const int STEP_NONE = 0;
static const int STEP_ZERO = 1;
static const int STEP_ROTATE = 2;
static const int STEP_POST_APPLY = 3;
static const int STEP_VERIFY_ROTATE = 4;
static const int STEP_VERIFY_MEASURE = 5;
static const int STEP_VERIFY_RESULT = 6;
static const int STEP_SAVE = 7;
static int calStep = STEP_NONE;
static bool s_verifyMoveSawRunning = false;

// Fixed workpiece RPM for calibration MOVE 360 (not Motor Config max).
static constexpr float kCalMoveRpm = 0.25f;
static constexpr float kCalMoveRpmMin = 0.05f;
static uint32_t calMoveStartMs = 0;
static uint32_t calMoveTimeoutMs = 0;
static bool calMoveSawStepState = false;

// Screen-space X of wizard dot centers (matches calibration_screen_proposal.svg).
static const int WIZ_DOT_CX[4] = { 50, 170, 290, 410 };

static const float kCalFactorStep = 0.0005f;

static void update_wizard_progress_fill() {
  if (!wizardFill) return;
  if (calStep >= STEP_SAVE) {
    lv_obj_set_width(wizardFill, CAL_WIZ_TRACK_W);
    return;
  }
  int idx = 0;
  if (calStep >= STEP_VERIFY_RESULT) {
    idx = 3;
  } else if (calStep >= STEP_POST_APPLY) {
    idx = 2;
  } else if (calStep >= STEP_ROTATE) {
    idx = 1;
  }
  const int leftX = CAL_CARD_LEFT;
  const int cxInCard = WIZ_DOT_CX[idx] - leftX;
  int w = cxInCard - CAL_WIZ_TRACK_X + 8;
  if (w < 10) w = 10;
  if (w > CAL_WIZ_TRACK_W) w = CAL_WIZ_TRACK_W;
  lv_obj_set_width(wizardFill, w);
}

static void measured_refresh_label() {
  if (!measuredTapLabel) return;
  const bool useVerify = (calStep == STEP_VERIFY_ROTATE || calStep == STEP_VERIFY_MEASURE ||
                          calStep == STEP_VERIFY_RESULT);
  const float v = useVerify ? s_verifyMeasuredDeg : s_measuredDeg;
  if (v > 0.0f) {
    char b[20];
    snprintf(b, sizeof(b), "%.2f", (double)v);
    lv_label_set_text(measuredTapLabel, b);
  } else {
    lv_label_set_text(measuredTapLabel, "---");
  }
}

static void calibration_purge_entry_ui() {
  if (calEntryKb) {
    lv_obj_t* o = calEntryKb;
    calEntryKb = nullptr;
    lv_obj_delete_async(o);
  }
  if (calEntryTa) {
    lv_obj_t* o = calEntryTa;
    calEntryTa = nullptr;
    lv_obj_delete_async(o);
  }
  if (calEntryHint) {
    lv_obj_t* o = calEntryHint;
    calEntryHint = nullptr;
    lv_obj_delete_async(o);
  }
  calEntryClosePending = false;
}

static void back_cb(lv_event_t* e) {
  (void)e;
  calibration_purge_entry_ui();
  control_stop_jog();
  calMoveTimeoutMs = 0;
  calMoveSawStepState = false;
  screens_show(SCREEN_SETTINGS);
}

static void restart_cb(lv_event_t* e) {
  (void)e;
  calibration_purge_entry_ui();
  control_stop_jog();
  calMoveTimeoutMs = 0;
  calMoveSawStepState = false;
  calStep = STEP_ZERO;
  s_measuredDeg = 0.0f;
  s_verifyMeasuredDeg = 0.0f;
  s_verifyMoveSawRunning = false;
  measured_refresh_label();
  calibration_set_factor(1.0f);
  screen_calibration_update();
}

static void factor_adj_cb(lv_event_t* e) {
  const intptr_t dir = (intptr_t)lv_event_get_user_data(e);
  float f = calibration_get_factor();
  calibration_set_factor(f + (dir > 0 ? kCalFactorStep : -kCalFactorStep));
  if (calStep == STEP_SAVE || calStep == STEP_VERIFY_RESULT) {
    calStep = STEP_POST_APPLY;
    s_verifyMeasuredDeg = 0.0f;
  } else if (calStep == STEP_VERIFY_ROTATE || calStep == STEP_VERIFY_MEASURE) {
    calStep = STEP_POST_APPLY;
    s_verifyMeasuredDeg = 0.0f;
  }
  screen_calibration_update();
}

static void save_cb(lv_event_t* e) {
  (void)e;
  calibration_save();
  calStep = STEP_SAVE;
  screen_calibration_update();
}

static void run_360_cb(lv_event_t* e) {
  (void)e;

  if (control_get_state() != STATE_IDLE) {
    LOG_W("CAL MOVE360 blocked: state=%s", control_get_state_string());
    return;
  }
  if (safety_inhibit_motion()) {
    LOG_W("CAL MOVE360 blocked: safety inhibit");
    return;
  }

  float rpm = kCalMoveRpm;
  const float maxRpm = speed_get_rpm_max();
  if (rpm > maxRpm) rpm = maxRpm;
  if (rpm < kCalMoveRpmMin) rpm = kCalMoveRpmMin;

  speed_set_slider_priority(true);
  speed_slider_set(rpm);

  const long steps = labs(angleToSteps(kCalCommandedDeg));
  const uint32_t mhz = motor_milli_hz_for_rpm_calibrated(rpm);
  const float hz = (float)mhz / 1000.0f;
  const float estSec = (hz > 0.1f) ? ((float)steps / hz) : 999999.0f;

  LOG_I("CAL MOVE360: rpm=%.3f steps=%ld hz=%.1f est=%.1fs factor=%.5f", (double)rpm, (long)steps,
        (double)hz, (double)estSec, (double)calibration_get_factor());

  calMoveStartMs = millis();
  calMoveTimeoutMs = (uint32_t)((estSec + 30.0f) * 1000.0f);
  if (calMoveTimeoutMs < 15000U) calMoveTimeoutMs = 15000U;
  if (calMoveTimeoutMs > 600000U) calMoveTimeoutMs = 600000U;
  calMoveSawStepState = false;

  control_reset_step_accumulator();

  if (!control_start_step(kCalCommandedDeg)) {
    LOG_W("CAL MOVE360: control_start_step failed");
    calMoveTimeoutMs = 0U;
    return;
  }

  if (calStep == STEP_VERIFY_ROTATE) {
    s_verifyMeasuredDeg = 0.0f;
    s_verifyMoveSawRunning = false;
    measured_refresh_label();
    screen_calibration_update();
    return;
  }

  if (calStep == STEP_POST_APPLY || calStep == STEP_VERIFY_RESULT) {
    s_verifyMeasuredDeg = 0.0f;
    s_verifyMoveSawRunning = false;
    calStep = STEP_VERIFY_ROTATE;
  } else {
    s_measuredDeg = 0.0f;
    calStep = STEP_ROTATE;
  }
  measured_refresh_label();
  screen_calibration_update();
}

static void cal_stop_motion_cb(lv_event_t* e) {
  (void)e;
  LOG_W("CAL STOP");
  calMoveTimeoutMs = 0U;
  calMoveSawStepState = false;
  control_stop();
}

static void jog_btn_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  const intptr_t dir = (intptr_t)lv_event_get_user_data(e);
  if (code == LV_EVENT_PRESSED) {
    if (safety_inhibit_motion()) return;
    if (control_get_state() != STATE_IDLE) return;
    if (dir > 0) {
      control_start_jog_cw();
    } else {
      control_start_jog_ccw();
    }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
  }
}

static void measured_keyboard_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (calEntryTa) {
      const char* txt = lv_textarea_get_text(calEntryTa);
      float v = step_parse_first_float(txt);
      if (v >= 0.5f && v <= 720.0f) {
        if (calStep == STEP_VERIFY_MEASURE) {
          s_verifyMeasuredDeg = v;
          calStep = STEP_VERIFY_RESULT;
        } else {
          s_measuredDeg = v;
        }
        measured_refresh_label();
      }
    }
  }
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    calEntryClosePending = true;
  }
}

static void measured_tap_cb(lv_event_t* e) {
  (void)e;
  if (calEntryKb) return;
  if (calStep != STEP_ROTATE && calStep != STEP_VERIFY_MEASURE) return;
  if (control_get_state() != STATE_IDLE) return;
  if (safety_inhibit_motion()) return;

  lv_obj_t* scr = screenRoots[SCREEN_CALIBRATION];
  calEntryHint = lv_label_create(scr);
  if (calStep == STEP_VERIFY_MEASURE) {
    lv_label_set_text(calEntryHint, "Angle after VERIFY MOVE (~360). OK when done.");
  } else {
    lv_label_set_text(calEntryHint, "Angle after this MOVE 360. OK when done.");
  }
  lv_obj_set_style_text_font(calEntryHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(calEntryHint, COL_TEXT_DIM, 0);
  lv_obj_align(calEntryHint, LV_ALIGN_TOP_MID, 0, 44);

  calEntryTa = lv_textarea_create(scr);
  lv_obj_set_size(calEntryTa, 420, 44);
  lv_obj_align(calEntryTa, LV_ALIGN_TOP_MID, 0, 92);
  lv_textarea_set_one_line(calEntryTa, true);
  lv_textarea_set_accepted_chars(calEntryTa, "0123456789.,");
  lv_obj_set_style_bg_color(calEntryTa, COL_BG, 0);
  lv_obj_set_style_border_color(calEntryTa, COL_ACCENT, 0);
  lv_obj_set_style_border_width(calEntryTa, 2, 0);
  lv_obj_set_style_text_color(calEntryTa, COL_TEXT, 0);
  if (calStep == STEP_VERIFY_MEASURE) {
    if (s_verifyMeasuredDeg > 0.0f) {
      char ib[20];
      snprintf(ib, sizeof(ib), "%.2f", (double)s_verifyMeasuredDeg);
      lv_textarea_set_text(calEntryTa, ib);
    }
  } else if (s_measuredDeg > 0.0f) {
    char ib[20];
    snprintf(ib, sizeof(ib), "%.2f", (double)s_measuredDeg);
    lv_textarea_set_text(calEntryTa, ib);
  }

  calEntryKb = lv_keyboard_create(scr);
  lv_keyboard_set_mode(calEntryKb, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(calEntryKb, calEntryTa);
  lv_obj_set_style_border_width(calEntryKb, 2, 0);
  lv_obj_set_style_border_color(calEntryKb, COL_ACCENT, 0);
  lv_obj_set_size(calEntryKb, SCREEN_W, 220);
  lv_obj_align(calEntryKb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(calEntryKb, measured_keyboard_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(calEntryKb, measured_keyboard_cb, LV_EVENT_CANCEL, nullptr);
}

static void apply_measure_cb(lv_event_t* e) {
  (void)e;
  if (calStep != STEP_ROTATE) return;
  if (s_measuredDeg < 0.5f) return;
  float ratio = kCalCommandedDeg / s_measuredDeg;
  if (ratio < 0.2f || ratio > 5.0f) return;
  float f = calibration_get_factor();
  calibration_set_factor(cal_factor_from_measured_rotation(f, kCalCommandedDeg, s_measuredDeg));
  calStep = STEP_POST_APPLY;
  s_verifyMeasuredDeg = 0.0f;
  screen_calibration_update();
}

static void style_result_bar_pass(bool pass) {
  if (!resultBar) return;
  if (pass) {
    ui_style_post_ok(resultBar);
    lv_obj_set_style_radius(resultBar, RADIUS_CARD, 0);
  } else {
    lv_obj_set_style_bg_color(resultBar, COL_BG_DANGER, 0);
    lv_obj_set_style_bg_opa(resultBar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(resultBar, COL_RED, 0);
    lv_obj_set_style_border_width(resultBar, 1, 0);
    lv_obj_set_style_radius(resultBar, RADIUS_CARD, 0);
  }
}

static void style_result_bar_neutral() {
  if (!resultBar) return;
  ui_style_post_card(resultBar);
  lv_obj_set_style_radius(resultBar, RADIUS_CARD, 0);
}

static void update_wizard() {
  if (wizardHintLabel) {
    if (calStep >= STEP_SAVE) {
      lv_label_set_text(wizardHintLabel, "Saved. RESTART or +/- and verify again.");
    } else if (calStep >= STEP_VERIFY_RESULT) {
      lv_label_set_text(wizardHintLabel, "SAVE if PASS, else +/- and verify MOVE.");
    } else if (calStep >= STEP_VERIFY_MEASURE) {
      lv_label_set_text(wizardHintLabel, "MEASURED DEG: enter angle after verify MOVE.");
    } else if (calStep >= STEP_VERIFY_ROTATE) {
      lv_label_set_text(wizardHintLabel, "VERIFY MOVE 360 running. Stop, then measure.");
    } else if (calStep >= STEP_POST_APPLY) {
      lv_label_set_text(wizardHintLabel, "MOVE 360 again, then MEASURED DEG.");
    } else if (calStep >= STEP_ROTATE) {
      lv_label_set_text(wizardHintLabel, "MEASURED DEG + APPLY when stopped.");
    } else {
      lv_label_set_text(wizardHintLabel,
                        "WP OD in KIN. MOVE 360, APPLY. Then verify MOVE + 2nd meas.");
    }
    lv_obj_set_style_text_color(wizardHintLabel, COL_TEXT_DIM, 0);
  }

  for (int i = 0; i < 4; i++) {
    if (!wizardDots[i]) continue;
    const int n = i + 1;
    bool done = (n == 1 && calStep >= STEP_ROTATE) || (n == 2 && calStep >= STEP_POST_APPLY) ||
                (n == 3 && calStep >= STEP_VERIFY_RESULT) || (n == 4 && calStep >= STEP_SAVE);
    bool active = (n == 1 && calStep == STEP_ZERO) || (n == 2 && calStep == STEP_ROTATE) ||
                  (n == 3 && (calStep == STEP_POST_APPLY || calStep == STEP_VERIFY_ROTATE ||
                              calStep == STEP_VERIFY_MEASURE)) ||
                  (n == 4 && calStep == STEP_VERIFY_RESULT);
    lv_color_t col = done ? COL_GREEN : (active ? COL_ACCENT : COL_TEXT_VDIM);
    lv_obj_set_style_bg_color(wizardDots[i], col, 0);
  }
  update_wizard_progress_fill();
}

void screen_calibration_create() {
  lv_obj_t* screen = screenRoots[SCREEN_CALIBRATION];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  char cap[28];
  snprintf(cap, sizeof(cap), "WROT %s", FW_VERSION);
  ui_create_settings_header(screen, "CALIBRATION", cap, COL_TEXT_DIM);

  const int topY = CAL_TOP_Y;
  const int topH = CAL_TOP_H;
  const int leftX = CAL_CARD_LEFT;
  const int xMid = leftX + CAL_TOP_W_A + CAL_TOP_GAP;
  const int xRight = xMid + CAL_TOP_W_B + CAL_TOP_GAP;
  const int wizY = CAL_WIZ_Y;
  const int wizH = CAL_WIZ_H;
  const int wizW = CAL_WIZ_W;
  const int wizPad = CAL_WIZ_PAD;
  const int rightX = CAL_RIGHT_X;
  const int rightW = CAL_RIGHT_W;

  lv_obj_t* factorCard = ui_create_post_card(screen, leftX, topY, CAL_TOP_W_A, topH);
  lv_obj_remove_flag(factorCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(factorCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* encSecLabel = lv_label_create(factorCard);
  lv_label_set_text(encSecLabel, "CAL FACTOR");
  lv_obj_set_style_text_font(encSecLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(encSecLabel, COL_TEXT, 0);
  lv_obj_set_pos(encSecLabel, 12, 6);

  lv_obj_t* pmMinus = ui_create_pm_btn(factorCard, CAL_FACTOR_PM_MINUS_X, CAL_FACTOR_PM_Y, "-", FONT_NORMAL,
                                       UI_BTN_NORMAL, factor_adj_cb, (void*)(intptr_t)-1);
  lv_obj_set_size(pmMinus, CAL_PM_BTN_W, CAL_PM_BTN_H);
  lv_obj_set_pos(pmMinus, CAL_FACTOR_PM_MINUS_X, CAL_FACTOR_PM_Y);
  lv_obj_t* pmPlus = ui_create_pm_btn(factorCard, CAL_FACTOR_PM_PLUS_X, CAL_FACTOR_PM_Y, "+", FONT_NORMAL,
                                      UI_BTN_ACCENT, factor_adj_cb, (void*)(intptr_t)1);
  lv_obj_set_size(pmPlus, CAL_PM_BTN_W, CAL_PM_BTN_H);
  lv_obj_set_pos(pmPlus, CAL_FACTOR_PM_PLUS_X, CAL_FACTOR_PM_Y);

  zeroValueLabel = lv_label_create(factorCard);
  lv_label_set_text(zeroValueLabel, "1.0000");
  lv_obj_set_style_text_font(zeroValueLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(zeroValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_pos(zeroValueLabel, 12, 30);
  lv_obj_set_width(zeroValueLabel, 92);
  lv_label_set_long_mode(zeroValueLabel, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* spdCard = ui_create_post_card(screen, xMid, topY, CAL_TOP_W_B, topH);
  lv_obj_remove_flag(spdCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(spdCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* spdSecLabel = lv_label_create(spdCard);
  lv_label_set_text(spdSecLabel, "STEPS/DEG OUT|WP");
  lv_obj_set_style_text_font(spdSecLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(spdSecLabel, COL_TEXT, 0);
  lv_obj_set_pos(spdSecLabel, 12, 6);

  dualStepsDegLabel = lv_label_create(spdCard);
  lv_label_set_text(dualStepsDegLabel, "OUT 0.00\nWP 0.00");
  lv_obj_set_style_text_font(dualStepsDegLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(dualStepsDegLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_style_text_line_space(dualStepsDegLabel, 3, 0);
  lv_obj_set_pos(dualStepsDegLabel, 12, 24);
  lv_obj_set_width(dualStepsDegLabel, CAL_TOP_W_B - 16);
  lv_label_set_long_mode(dualStepsDegLabel, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* cmdCard = ui_create_post_card(screen, xRight, topY, CAL_TOP_W_C, topH);
  lv_obj_remove_flag(cmdCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(cmdCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* cmdTitle = lv_label_create(cmdCard);
  lv_label_set_text(cmdTitle, "CMD 360 OUT|WP");
  lv_obj_set_style_text_font(cmdTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(cmdTitle, COL_TEXT, 0);
  lv_obj_set_pos(cmdTitle, 12, 6);

  cmdStepsOutLabel = lv_label_create(cmdCard);
  lv_label_set_text(cmdStepsOutLabel, "OUT 0");
  lv_obj_set_style_text_font(cmdStepsOutLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(cmdStepsOutLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_pos(cmdStepsOutLabel, 12, 24);
  lv_obj_set_width(cmdStepsOutLabel, CAL_TOP_W_C - 20);
  lv_label_set_long_mode(cmdStepsOutLabel, LV_LABEL_LONG_MODE_CLIP);

  cmdStepsWpLabel = lv_label_create(cmdCard);
  lv_label_set_text(cmdStepsWpLabel, "WP 0");
  lv_obj_set_style_text_font(cmdStepsWpLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(cmdStepsWpLabel, COL_ACCENT, 0);
  lv_obj_set_pos(cmdStepsWpLabel, 12, 40);
  lv_obj_set_width(cmdStepsWpLabel, CAL_TOP_W_C - 20);
  lv_label_set_long_mode(cmdStepsWpLabel, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* wizardCard = ui_create_post_card(screen, leftX, wizY, wizW, wizH);
  lv_obj_remove_flag(wizardCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(wizardCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* wizTitle = lv_label_create(wizardCard);
  lv_label_set_text(wizTitle, "WIZARD");
  lv_obj_set_style_text_font(wizTitle, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(wizTitle, COL_ACCENT, 0);
  lv_obj_set_pos(wizTitle, wizPad, 6);

  wizardHintLabel = lv_label_create(wizardCard);
  lv_label_set_text(wizardHintLabel,
                    "WP OD in KIN. MOVE 360, APPLY. Then verify MOVE + 2nd meas.");
  lv_obj_set_style_text_font(wizardHintLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(wizardHintLabel, COL_TEXT, 0);
  lv_obj_set_style_text_line_space(wizardHintLabel, 3, 0);
  lv_obj_set_pos(wizardHintLabel, wizPad, 22);
  lv_obj_set_width(wizardHintLabel, wizW - 2 * wizPad);
  lv_label_set_long_mode(wizardHintLabel, LV_LABEL_LONG_MODE_WRAP);

  wizardTrack = lv_obj_create(wizardCard);
  lv_obj_set_pos(wizardTrack, CAL_WIZ_TRACK_X, 50);
  lv_obj_set_size(wizardTrack, CAL_WIZ_TRACK_W, 8);
  lv_obj_set_style_bg_color(wizardTrack, COL_PROGRESS_BG, 0);
  lv_obj_set_style_border_width(wizardTrack, 0, 0);
  lv_obj_set_style_radius(wizardTrack, 3, 0);
  lv_obj_remove_flag(wizardTrack, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(wizardTrack, LV_OBJ_FLAG_CLICKABLE);

  wizardFill = lv_obj_create(wizardTrack);
  lv_obj_set_size(wizardFill, 14, 6);
  lv_obj_set_style_bg_color(wizardFill, COL_ACCENT, 0);
  lv_obj_set_style_border_width(wizardFill, 0, 0);
  lv_obj_set_style_radius(wizardFill, 3, 0);
  lv_obj_align(wizardFill, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_remove_flag(wizardFill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(wizardFill, LV_OBJ_FLAG_CLICKABLE);

  const int dotD = 18;
  const int dotY = 48;
  for (int i = 0; i < 4; i++) {
    wizardDots[i] = lv_obj_create(wizardCard);
    lv_obj_set_size(wizardDots[i], dotD, dotD);
    lv_obj_set_pos(wizardDots[i], WIZ_DOT_CX[i] - leftX - dotD / 2, dotY);
    lv_obj_set_style_bg_color(wizardDots[i], COL_TEXT_VDIM, 0);
    lv_obj_set_style_border_width(wizardDots[i], 0, 0);
    lv_obj_set_style_radius(wizardDots[i], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(wizardDots[i], 0, 0);
    lv_obj_remove_flag(wizardDots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(wizardDots[i], LV_OBJ_FLAG_CLICKABLE);
  }

  lv_obj_t* wz0 = lv_label_create(wizardCard);
  lv_label_set_text(wz0, "ZERO");
  lv_obj_set_style_text_font(wz0, FONT_SMALL, 0);
  lv_obj_set_style_text_color(wz0, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(wz0, 12, 78);
  lv_obj_t* wz1 = lv_label_create(wizardCard);
  lv_label_set_text(wz1, "MOVE 360");
  lv_obj_set_style_text_font(wz1, FONT_SMALL, 0);
  lv_obj_set_style_text_color(wz1, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(wz1, 112, 78);
  lv_obj_t* wz2 = lv_label_create(wizardCard);
  lv_label_set_text(wz2, "MEASURE");
  lv_obj_set_style_text_font(wz2, FONT_SMALL, 0);
  lv_obj_set_style_text_color(wz2, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(wz2, 232, 78);
  lv_obj_t* wz3 = lv_label_create(wizardCard);
  lv_label_set_text(wz3, "SAVE");
  lv_obj_set_style_text_font(wz3, FONT_SMALL, 0);
  lv_obj_set_style_text_color(wz3, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(wz3, 352, 78);

  lv_obj_t* kinCard = ui_create_post_card(screen, rightX, wizY, rightW, wizH);
  lv_obj_remove_flag(kinCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(kinCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* kinTitle = lv_label_create(kinCard);
  lv_label_set_text(kinTitle, "KINEMATICS");
  lv_obj_set_style_text_font(kinTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(kinTitle, COL_ACCENT, 0);
  lv_obj_set_pos(kinTitle, 14, 4);

  lv_obj_t* kg = lv_label_create(kinCard);
  lv_label_set_text(kg, "GEAR RATIO");
  lv_obj_set_style_text_font(kg, FONT_TINY, 0);
  lv_obj_set_style_text_color(kg, COL_TEXT_DIM, 0);
  lv_obj_set_pos(kg, 14, 18);

  kinGearValueLabel = lv_label_create(kinCard);
  lv_label_set_text(kinGearValueLabel, "108:1");
  lv_obj_set_style_text_font(kinGearValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(kinGearValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_pos(kinGearValueLabel, 14, 28);

  lv_obj_t* km = lv_label_create(kinCard);
  lv_label_set_text(km, "MICROSTEP");
  lv_obj_set_style_text_font(km, FONT_TINY, 0);
  lv_obj_set_style_text_color(km, COL_TEXT_DIM, 0);
  lv_obj_set_pos(km, 14, 46);

  kinMicroValueLabel = lv_label_create(kinCard);
  lv_label_set_text(kinMicroValueLabel, "3200");
  lv_obj_set_style_text_font(kinMicroValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(kinMicroValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_pos(kinMicroValueLabel, 14, 56);

  kinGeoLabel = lv_label_create(kinCard);
  lv_label_set_text(kinGeoLabel, "MOVE360 WP300 RULL80");
  lv_obj_set_style_text_font(kinGeoLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(kinGeoLabel, COL_ACCENT, 0);
  lv_obj_set_pos(kinGeoLabel, 14, 76);
  lv_obj_set_width(kinGeoLabel, rightW - 28);
  lv_label_set_long_mode(kinGeoLabel, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* kmo = lv_label_create(kinCard);
  lv_label_set_text(kmo, "MODE");
  lv_obj_set_style_text_font(kmo, FONT_TINY, 0);
  lv_obj_set_style_text_color(kmo, COL_TEXT_DIM, 0);
  lv_obj_set_pos(kmo, 14, 102);

  lv_obj_t* kout = lv_label_create(kinCard);
  lv_label_set_text(kout, "OUTPUT");
  lv_obj_set_style_text_font(kout, FONT_TINY, 0);
  lv_obj_set_style_text_color(kout, COL_ACCENT, 0);
  lv_obj_set_pos(kout, 88, 102);

  const int infoY = CAL_INFO_Y;
  const int infoH = CAL_INFO_H;
  lv_obj_t* measCard = ui_create_post_card(screen, leftX, infoY, wizW, infoH);
  lv_obj_remove_flag(measCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(measCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* mdHead = lv_label_create(measCard);
  lv_label_set_text(mdHead, "MEASURED DEG");
  lv_obj_set_style_text_font(mdHead, FONT_TINY, 0);
  lv_obj_set_style_text_color(mdHead, COL_TEXT_DIM, 0);
  lv_obj_set_pos(mdHead, 14, 4);

  measuredFieldBg = lv_obj_create(measCard);
  lv_obj_set_pos(measuredFieldBg, 14, CAL_MEAS_FIELD_Y);
  lv_obj_set_size(measuredFieldBg, CAL_MEAS_FIELD_W, CAL_MEAS_FIELD_H);
  lv_obj_set_style_bg_color(measuredFieldBg, COL_BG_INPUT, 0);
  lv_obj_set_style_border_color(measuredFieldBg, COL_BORDER, 0);
  lv_obj_set_style_border_width(measuredFieldBg, 1, 0);
  lv_obj_set_style_radius(measuredFieldBg, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(measuredFieldBg, 0, 0);
  lv_obj_remove_flag(measuredFieldBg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(measuredFieldBg, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(measuredFieldBg, measured_tap_cb, LV_EVENT_CLICKED, nullptr);

  measuredTapLabel = lv_label_create(measuredFieldBg);
  lv_label_set_text(measuredTapLabel, "---");
  lv_obj_set_style_text_font(measuredTapLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(measuredTapLabel, COL_TEXT_WHITE, 0);
  lv_obj_align(measuredTapLabel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_remove_flag(measuredTapLabel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* tapHint = lv_label_create(measCard);
  lv_label_set_text(tapHint, "tap field");
  lv_obj_set_style_text_font(tapHint, FONT_TINY, 0);
  lv_obj_set_style_text_color(tapHint, COL_TEXT_VDIM, 0);
  lv_obj_set_width(tapHint, 100);
  lv_obj_set_style_text_align(tapHint, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(tapHint, wizW - 118, CAL_MEAS_FIELD_Y + 8);

  const int applyW = 214;
  const int applyX = wizW - wizPad - applyW;
  applyMeasureBtn =
      ui_create_btn(measCard, applyX, 10, applyW, CAL_APPLY_MEAS_H, "APPLY MEASUREMENT", FONT_SMALL,
                    UI_BTN_ACCENT, apply_measure_cb, nullptr);

  lv_obj_t* formCard = ui_create_post_card(screen, rightX, infoY, rightW, infoH);
  lv_obj_remove_flag(formCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(formCard, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* fmHead = lv_label_create(formCard);
  lv_label_set_text(fmHead, "FORMULA");
  lv_obj_set_style_text_font(fmHead, FONT_TINY, 0);
  lv_obj_set_style_text_color(fmHead, COL_TEXT_DIM, 0);
  lv_obj_set_pos(fmHead, 14, 4);

  lv_obj_t* fmBody = lv_label_create(formCard);
  lv_label_set_text(fmBody, "factor = old x 360 / measured");
  lv_obj_set_style_text_font(fmBody, FONT_SMALL, 0);
  lv_obj_set_style_text_color(fmBody, COL_TEXT, 0);
  lv_obj_set_pos(fmBody, 14, 24);

  const int ay = CAL_ACTION_Y;
  const int ah = CAL_ACTION_BTN_H;
  const int ax0 = leftX + 9;
  run360Btn = ui_create_btn(screen, ax0, ay, CAL_ACTION_MOVE_W, ah, "MOVE 360", FONT_NORMAL, UI_BTN_ACCENT,
                            run_360_cb, nullptr);
  stopMotionBtn = ui_create_btn(screen, ax0 + CAL_ACTION_MOVE_W + 11, ay, CAL_ACTION_STOP_W, ah, "STOP",
                                 FONT_NORMAL, UI_BTN_DANGER, cal_stop_motion_cb, nullptr);

  jogMinusBtn = ui_create_btn(screen, rightX, ay, CAL_JOG_BTN_W, ah, "JOG -", FONT_NORMAL, UI_BTN_NORMAL, nullptr,
                               nullptr);
  lv_obj_add_event_cb(jogMinusBtn, jog_btn_cb, LV_EVENT_PRESSED, (void*)(intptr_t)-1);
  lv_obj_add_event_cb(jogMinusBtn, jog_btn_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(jogMinusBtn, jog_btn_cb, LV_EVENT_PRESS_LOST, nullptr);

  jogPlusBtn = ui_create_btn(screen, rightX + CAL_JOG_BTN_W + CAL_JOG_GAP, ay, CAL_JOG_BTN_W, ah, "JOG +",
                             FONT_NORMAL, UI_BTN_NORMAL, nullptr, nullptr);
  lv_obj_add_event_cb(jogPlusBtn, jog_btn_cb, LV_EVENT_PRESSED, (void*)(intptr_t)1);
  lv_obj_add_event_cb(jogPlusBtn, jog_btn_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(jogPlusBtn, jog_btn_cb, LV_EVENT_PRESS_LOST, nullptr);

  const int resultY = CAL_RESULT_Y;
  const int resultH = CAL_RESULT_H;
  const int resultW = CAL_WIZ_W + 14 + CAL_RIGHT_W;
  resultBar = lv_obj_create(screen);
  lv_obj_set_size(resultBar, resultW, resultH);
  lv_obj_set_pos(resultBar, leftX, resultY);
  ui_style_post_ok(resultBar);
  lv_obj_set_style_radius(resultBar, RADIUS_CARD, 0);
  lv_obj_remove_flag(resultBar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(resultBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(resultBar, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  resultStatusLabel = lv_label_create(resultBar);
  lv_label_set_text(resultStatusLabel, "STEP 1/5");
  lv_obj_set_style_text_font(resultStatusLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
  lv_obj_set_pos(resultStatusLabel, 14, 6);

  resultDetailLabel = lv_label_create(resultBar);
  lv_label_set_text(resultDetailLabel, "Match WP OD; MOVE 360 per KIN (set OD on Step).");
  lv_obj_set_style_text_font(resultDetailLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(resultDetailLabel, COL_TEXT, 0);
  lv_obj_set_style_text_line_space(resultDetailLabel, 0, 0);
  lv_obj_set_pos(resultDetailLabel, 14, 26);
  lv_obj_set_width(resultDetailLabel, resultW - 40);
  lv_label_set_long_mode(resultDetailLabel, LV_LABEL_LONG_MODE_DOTS);

  resultReadyLabel = lv_label_create(resultBar);
  lv_label_set_text(resultReadyLabel, "");
  lv_obj_set_style_text_font(resultReadyLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(resultReadyLabel, COL_GREEN, 0);
  lv_obj_set_width(resultReadyLabel, 130);
  lv_obj_set_style_text_align(resultReadyLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(resultReadyLabel, resultW - 144, 8);
  lv_obj_add_flag(resultReadyLabel, LV_OBJ_FLAG_HIDDEN);

  const int footY = CAL_FOOT_Y;
  const int footH = CAL_FOOT_H;
  const int footGap = 20;
  ui_create_btn(screen, CAL_CARD_LEFT, footY, 180, footH, "<  BACK", SET_BTN_FONT, UI_BTN_NORMAL, back_cb,
                nullptr);
  ui_create_btn(screen, CAL_CARD_LEFT + 180 + footGap, footY, 180, footH, "RESTART", SET_BTN_FONT,
                UI_BTN_NORMAL, restart_cb, nullptr);
  lv_obj_t* saveBtn = ui_create_btn(screen, CAL_CARD_LEFT + 180 + footGap + 180 + footGap, footY, 260, footH,
                                    "SAVE CALIBRATION", SET_BTN_FONT, UI_BTN_ACCENT, save_cb, nullptr);
  lv_obj_set_style_border_width(saveBtn, 1, 0);

  calStep = STEP_ZERO;
  s_measuredDeg = 0.0f;
  s_verifyMeasuredDeg = 0.0f;
  s_verifyMoveSawRunning = false;
  screen_calibration_update();
}

void screen_calibration_invalidate_widgets() {
  zeroValueLabel = nullptr;
  dualStepsDegLabel = nullptr;
  cmdStepsOutLabel = nullptr;
  cmdStepsWpLabel = nullptr;
  kinGearValueLabel = nullptr;
  kinMicroValueLabel = nullptr;
  kinGeoLabel = nullptr;
  wizardHintLabel = nullptr;
  wizardTrack = nullptr;
  wizardFill = nullptr;
  for (int i = 0; i < 4; i++) {
    wizardDots[i] = nullptr;
  }
  measuredFieldBg = nullptr;
  measuredTapLabel = nullptr;
  applyMeasureBtn = nullptr;
  run360Btn = nullptr;
  stopMotionBtn = nullptr;
  jogMinusBtn = nullptr;
  jogPlusBtn = nullptr;
  calEntryKb = nullptr;
  calEntryTa = nullptr;
  calEntryHint = nullptr;
  calEntryClosePending = false;
  resultBar = nullptr;
  resultStatusLabel = nullptr;
  resultDetailLabel = nullptr;
  resultReadyLabel = nullptr;
  calStep = STEP_NONE;
  s_measuredDeg = 0.0f;
  s_verifyMeasuredDeg = 0.0f;
  s_verifyMoveSawRunning = false;
  calMoveTimeoutMs = 0U;
  calMoveSawStepState = false;
}

void screen_calibration_update() {
  if (calEntryClosePending) {
    calibration_purge_entry_ui();
  }

  const SystemState stNow = control_get_state();

  if (calMoveTimeoutMs > 0U) {
    if (stNow == STATE_STEP) {
      calMoveSawStepState = true;
    }
    if ((calStep == STEP_ROTATE || calStep == STEP_VERIFY_ROTATE) && stNow == STATE_STEP &&
        (int32_t)(millis() - calMoveStartMs) > (int32_t)calMoveTimeoutMs) {
      LOG_E("CAL MOVE360 timeout: stopping");
      calMoveTimeoutMs = 0U;
      calMoveSawStepState = false;
      control_stop();
    } else if (calMoveSawStepState && stNow == STATE_IDLE) {
      calMoveTimeoutMs = 0U;
      calMoveSawStepState = false;
    }
  }

  if (calStep == STEP_VERIFY_ROTATE) {
    if (stNow == STATE_STEP) {
      s_verifyMoveSawRunning = true;
    } else if (s_verifyMoveSawRunning && stNow == STATE_IDLE) {
      calStep = STEP_VERIFY_MEASURE;
      s_verifyMoveSawRunning = false;
      measured_refresh_label();
    }
  }

  float factor = calibration_get_factor();
  const float outSteps360 = speed_steps_per_gear_output_rev();
  const long cmdOut360 = calibration_apply_steps((long)(outSteps360 + 0.5f));
  const float outStepsPerDeg = (outSteps360 / 360.0f) * factor;
  const long wpCmd360 = angleToSteps(kCalCommandedDeg);
  const float wpStepsPerDeg = (float)((double)wpCmd360 / (double)kCalCommandedDeg);

  if (zeroValueLabel) {
    char fbuf[20];
    snprintf(fbuf, sizeof(fbuf), "%.4f", (double)factor);
    lv_label_set_text(zeroValueLabel, fbuf);
  }

  if (dualStepsDegLabel) {
    char buf[40];
    snprintf(buf, sizeof(buf), "OUT %.2f\nWP %.2f", (double)outStepsPerDeg, (double)wpStepsPerDeg);
    lv_label_set_text(dualStepsDegLabel, buf);
  }

  if (cmdStepsOutLabel) {
    char ob[24];
    snprintf(ob, sizeof(ob), "OUT %ld", (long)cmdOut360);
    lv_label_set_text(cmdStepsOutLabel, ob);
  }
  if (cmdStepsWpLabel) {
    char wb[24];
    snprintf(wb, sizeof(wb), "WP %ld", (long)wpCmd360);
    lv_label_set_text(cmdStepsWpLabel, wb);
  }

  if (kinGearValueLabel) {
    char gbuf[16];
    snprintf(gbuf, sizeof(gbuf), "%.0f:1", (double)GEAR_RATIO);
    lv_label_set_text(kinGearValueLabel, gbuf);
  }
  if (kinMicroValueLabel) {
    char mbuf[16];
    snprintf(mbuf, sizeof(mbuf), "%lu", (unsigned long)microstep_get_steps_per_rev());
    lv_label_set_text(kinMicroValueLabel, mbuf);
  }
  if (kinGeoLabel) {
    float wpMm = speed_get_workpiece_diameter_mm();
    if (wpMm < 1.0f) {
      wpMm = D_EMNE * 1000.0f;
    }
    int wpi = (int)(wpMm + 0.5f);
    int rull = (int)(D_RULLE * 1000.0f + 0.5f);
    char g[40];
    snprintf(g, sizeof(g), "MOVE360 WP%d RULL%d", wpi, rull);
    lv_label_set_text(kinGeoLabel, g);
  }

  const int resultW = CAL_WIZ_W + 14 + CAL_RIGHT_W;

  if (resultDetailLabel && resultStatusLabel && resultBar) {
    if (resultReadyLabel) {
      lv_obj_add_flag(resultReadyLabel, LV_OBJ_FLAG_HIDDEN);
    }
    if (calStep >= STEP_SAVE) {
      lv_label_set_text(resultStatusLabel, "RESULT SAVED");
      lv_obj_set_style_text_color(resultStatusLabel, COL_GREEN, 0);
      lv_label_set_text(resultDetailLabel, "Stored. MOVE 360 to check or RESTART.");
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_pass(true);
    } else if (calStep == STEP_VERIFY_RESULT) {
      const float err = s_verifyMeasuredDeg - kCalCommandedDeg;
      const float absErr = err < 0.0f ? -err : err;
      const bool pass = (absErr <= kVerifyTolDeg);
      char buf[80];
      snprintf(buf, sizeof(buf), "meas %.2f err %+.2f tol %.1f f %.4f", (double)s_verifyMeasuredDeg,
               (double)err, (double)kVerifyTolDeg, (double)factor);
      lv_obj_set_width(resultDetailLabel, pass ? (resultW - 160) : (resultW - 36));
      lv_label_set_text(resultDetailLabel, buf);
      if (pass) {
        lv_label_set_text(resultStatusLabel, "RESULT PASS");
        lv_obj_set_style_text_color(resultStatusLabel, COL_GREEN, 0);
        style_result_bar_pass(true);
        if (resultReadyLabel) {
          lv_label_set_text(resultReadyLabel, "READY TO SAVE");
          lv_obj_remove_flag(resultReadyLabel, LV_OBJ_FLAG_HIDDEN);
        }
      } else {
        lv_label_set_text(resultStatusLabel, "RESULT FAIL");
        lv_obj_set_style_text_color(resultStatusLabel, COL_RED, 0);
        style_result_bar_pass(false);
      }
    } else if (calStep == STEP_VERIFY_MEASURE) {
      lv_label_set_text(resultStatusLabel, "STEP 4/5");
      lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
      lv_label_set_text(resultDetailLabel, "MEASURED DEG after verify MOVE.");
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_neutral();
    } else if (calStep == STEP_VERIFY_ROTATE) {
      lv_label_set_text(resultStatusLabel, "VERIFY MOVE");
      lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
      lv_label_set_text(resultDetailLabel, "VERIFY MOVE 360 running.");
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_neutral();
    } else if (calStep >= STEP_POST_APPLY) {
      char buf[64];
      snprintf(buf, sizeof(buf), "f %.4f meas1 %.2f -> MOVE 360, verify DEG.", (double)factor,
               (double)s_measuredDeg);
      lv_label_set_text(resultStatusLabel, "STEP 3/5");
      lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
      lv_label_set_text(resultDetailLabel, buf);
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_neutral();
    } else if (calStep == STEP_ROTATE) {
      lv_label_set_text(resultStatusLabel, "STEP 2/5");
      lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
      lv_label_set_text(resultDetailLabel, "MEASURED DEG + APPLY.");
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_neutral();
    } else {
      lv_label_set_text(resultStatusLabel, "STEP 1/5");
      lv_obj_set_style_text_color(resultStatusLabel, COL_ACCENT, 0);
      lv_label_set_text(resultDetailLabel,
                        "Match WP OD; MOVE 360 per KIN (set OD on Step).");
      lv_obj_set_width(resultDetailLabel, resultW - 36);
      style_result_bar_neutral();
    }
  }

  update_wizard();

  const bool idle = (control_get_state() == STATE_IDLE);
  const bool canStart = idle && !safety_inhibit_motion() &&
                        (calStep == STEP_ZERO || calStep == STEP_ROTATE || calStep == STEP_POST_APPLY ||
                         calStep == STEP_VERIFY_RESULT || calStep == STEP_VERIFY_ROTATE);
  const bool canMeasureUi = idle && !safety_inhibit_motion() &&
                            (calStep == STEP_ROTATE || calStep == STEP_VERIFY_MEASURE);
  const bool canApply = idle && !safety_inhibit_motion() && (calStep == STEP_ROTATE) &&
                        (s_measuredDeg >= 0.5f);
  const bool canJog = idle && !safety_inhibit_motion();

  if (run360Btn) {
    if (canStart) {
      lv_obj_remove_state(run360Btn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(run360Btn, LV_STATE_DISABLED);
    }
  }
  if (measuredFieldBg) {
    if (canMeasureUi) {
      lv_obj_remove_state(measuredFieldBg, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(measuredFieldBg, LV_STATE_DISABLED);
    }
  }
  if (applyMeasureBtn) {
    if (canApply) {
      lv_obj_remove_state(applyMeasureBtn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(applyMeasureBtn, LV_STATE_DISABLED);
    }
  }
  if (jogMinusBtn && jogPlusBtn) {
    if (canJog) {
      lv_obj_remove_state(jogMinusBtn, LV_STATE_DISABLED);
      lv_obj_remove_state(jogPlusBtn, LV_STATE_DISABLED);
    } else {
      lv_obj_add_state(jogMinusBtn, LV_STATE_DISABLED);
      lv_obj_add_state(jogPlusBtn, LV_STATE_DISABLED);
    }
  }
}
