// TIG Rotator Controller - Motor Configuration Screen
// Acceleration, microstepping, speed range settings
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/microstep.h"
#include "../../motor/acceleration.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include "../../control/control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

static const MicrostepSetting microOptions[] = {MICROSTEP_4, MICROSTEP_8, MICROSTEP_16, MICROSTEP_32};
// UI: steps per motor revolution (200 full steps * microstep factor); matches microstep_get_steps_per_rev()
static const char* microSprLabels[] = {"800", "1600", "3200", "6400"};
static constexpr int kMicroOptionCount = sizeof(microOptions) / sizeof(microOptions[0]);
static int selectedMicro = 0;
static lv_obj_t* microBtns[kMicroOptionCount] = {nullptr};
static lv_obj_t* microLabels[kMicroOptionCount] = {nullptr};
static lv_obj_t* microSummaryLbl = nullptr;
static lv_obj_t* accelValueLabel = nullptr;
static lv_obj_t* accelSlider = nullptr;
static lv_obj_t* maxRpmValueLabel = nullptr;
static lv_obj_t* maxRpmSlider = nullptr;
static lv_obj_t* rpmRangeVal = nullptr;
static int motorAccelUi = 10000;
static int motorMaxRpmMilliUi = 3000;

// Max RPM UI: 1..3000 = 0.001 .. 3.000 RPM (matches MIN_RPM / MAX_RPM)
static constexpr int kMaxRpmMilliMin = 1;
static constexpr int kMaxRpmMilliMax = 3000;

// Must match motor/acceleration.cpp ACCEL_MIN / ACCEL_MAX
static constexpr int kAccelMin = 1000;
static constexpr int kAccelMax = 30000;

static const char* accel_bucket_name(int val) {
  if (val < kAccelMin) val = kAccelMin;
  if (val > kAccelMax) val = kAccelMax;
  const int span = kAccelMax - kAccelMin;
  const int t1 = kAccelMin + span / 3;
  const int t2 = kAccelMin + (span * 2) / 3;
  if (val < t1) return "LOW";
  if (val < t2) return "NORMAL";
  return "HIGH";
}

static void motor_config_accel_sync_ui(int val) {
  if (val < kAccelMin) val = kAccelMin;
  if (val > kAccelMax) val = kAccelMax;
  motorAccelUi = val;
  if (accelValueLabel) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%s  %d", accel_bucket_name(val), val);
    lv_label_set_text(accelValueLabel, buf);
  }
  if (accelSlider && (int)lv_slider_get_value(accelSlider) != val) {
    lv_slider_set_value(accelSlider, val, LV_ANIM_OFF);
  }
}

static void motor_config_max_rpm_sync_ui(int milli) {
  if (milli < kMaxRpmMilliMin) milli = kMaxRpmMilliMin;
  if (milli > kMaxRpmMilliMax) milli = kMaxRpmMilliMax;
  motorMaxRpmMilliUi = milli;
  if (maxRpmValueLabel) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%.3f", milli / 1000.0f);
    lv_label_set_text(maxRpmValueLabel, buf);
  }
  if (maxRpmSlider && (int)lv_slider_get_value(maxRpmSlider) != milli) {
    lv_slider_set_value(maxRpmSlider, milli, LV_ANIM_OFF);
  }
  if (rpmRangeVal) {
    char buf[28];
    snprintf(buf, sizeof(buf), "%.3f - %.3f", (double)MIN_RPM, milli / 1000.0);
    lv_label_set_text(rpmRangeVal, buf);
  }
}

static lv_obj_t* saveFeedbackLabel = nullptr;
static bool invertDir = false;
static lv_obj_t* invertToggle = nullptr;
static lv_obj_t* invertToggleLbl = nullptr;
static bool dirSwitchEnabled = false;
static lv_obj_t* idleToggle = nullptr;
static lv_obj_t* idleToggleLbl = nullptr;
static lv_obj_t* statusLabel = nullptr;
static SystemState lastStatusState = (SystemState)-1;

// motorConfigApplyPending is defined in src/app_state.cpp.

static lv_obj_t* motor_cfg_post_row(lv_obj_t* screen, int x, int y, int w, int h) {
  lv_obj_t* row = lv_obj_create(screen);
  lv_obj_set_size(row, w, h);
  lv_obj_set_pos(row, x, y);
  ui_style_post_row(row);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  return row;
}

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static void micro_btn_cb(lv_event_t* e) {
  int idx = (int)(size_t)lv_event_get_user_data(e);
  selectedMicro = idx;
  for (int i = 0; i < kMicroOptionCount; i++) {
    const UiBtnStyle ms = (i == idx) ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(microBtns[i], ms);
    lv_obj_set_style_text_color(microLabels[i], ui_btn_label_color_post(ms), 0);
  }
  if (microSummaryLbl) {
    lv_label_set_text(microSummaryLbl, microSprLabels[idx]);
  }
}

static void accel_pm_cb(lv_event_t* e) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(e);
  motor_config_accel_sync_ui(motorAccelUi + delta * 500);
}

static void max_rpm_pm_cb(lv_event_t* e) {
  const int delta = (int)(intptr_t)lv_event_get_user_data(e);
  motor_config_max_rpm_sync_ui(motorMaxRpmMilliUi + delta);
}

static void max_rpm_slider_cb(lv_event_t* e) {
  (void)e;
  if (!maxRpmSlider) return;
  int v = (int)lv_slider_get_value(maxRpmSlider);
  motor_config_max_rpm_sync_ui(v);
}

static void accel_slider_cb(lv_event_t* e) {
  (void)e;
  if (!accelSlider) return;
  int v = (int)lv_slider_get_value(accelSlider);
  motor_config_accel_sync_ui(v);
}

// INVERT pill — poster .card when OFF, .run when ON (same semantics as ui_btn NORMAL / ACCENT)
static void motor_config_apply_invert_pill(bool invertOn) {
  if (!invertToggle || !invertToggleLbl) return;
  if (invertOn) {
    lv_obj_set_style_bg_color(invertToggle, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(invertToggle, COL_ACCENT, 0);
    lv_obj_set_style_border_width(invertToggle, 2, 0);
    lv_obj_set_style_radius(invertToggle, SET_TOGGLE_R, 0);
    lv_label_set_text(invertToggleLbl, "ON");
    lv_obj_set_style_text_color(invertToggleLbl, COL_ACCENT, 0);
  } else {
    lv_obj_set_style_bg_color(invertToggle, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(invertToggle, COL_BORDER, 0);
    lv_obj_set_style_border_width(invertToggle, 1, 0);
    lv_obj_set_style_radius(invertToggle, SET_TOGGLE_R, 0);
    lv_label_set_text(invertToggleLbl, "OFF");
    lv_obj_set_style_text_color(invertToggleLbl, COL_TEXT, 0);
  }
  lv_obj_set_style_text_font(invertToggleLbl, FONT_BTN, 0);
  lv_obj_center(invertToggleLbl);
}

static void invert_toggle_cb(lv_event_t* e) {
  (void)e;
  invertDir = !invertDir;
  motor_config_apply_invert_pill(invertDir);
}

// DIR SW pill — poster ui_screens (.ok when ON: COL_BG_OK + green border; OFF: neutral card)
static void motor_config_apply_dir_sw_pill(bool on) {
  if (!idleToggle || !idleToggleLbl) return;
  if (on) {
    lv_obj_set_style_bg_color(idleToggle, COL_BG_OK, 0);
    lv_obj_set_style_border_color(idleToggle, COL_BORDER_OK, 0);
    lv_obj_set_style_border_width(idleToggle, 1, 0);
    lv_obj_set_style_radius(idleToggle, SET_TOGGLE_R, 0);
    lv_label_set_text(idleToggleLbl, "ON");
    lv_obj_set_style_text_color(idleToggleLbl, COL_GREEN, 0);
  } else {
    lv_obj_set_style_bg_color(idleToggle, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(idleToggle, COL_BORDER, 0);
    lv_obj_set_style_border_width(idleToggle, 1, 0);
    lv_obj_set_style_radius(idleToggle, SET_TOGGLE_R, 0);
    lv_label_set_text(idleToggleLbl, "OFF");
    lv_obj_set_style_text_color(idleToggleLbl, COL_TEXT, 0);
  }
  lv_obj_set_style_text_font(idleToggleLbl, FONT_BTN, 0);
  lv_obj_center(idleToggleLbl);
}

static void idle_toggle_cb(lv_event_t* e) {
  (void)e;
  dirSwitchEnabled = !dirSwitchEnabled;
  motor_config_apply_dir_sw_pill(dirSwitchEnabled);
}

static lv_timer_t* saveNavTimer = nullptr;

static void save_nav_timer_cb(lv_timer_t* timer) {
  saveNavTimer = nullptr;
  screens_show(SCREEN_SETTINGS);
}

static void save_apply_cb(lv_event_t* e) {
  (void)e;
  if (control_get_state() != STATE_IDLE) {
    if (saveFeedbackLabel) {
      lv_label_set_text(saveFeedbackLabel, "Stop motor first");
      lv_obj_set_style_text_color(saveFeedbackLabel, COL_RED, 0);
    }
    return;
  }

  int accelVal = motorAccelUi;
  if (accelVal < kAccelMin) accelVal = kAccelMin;
  if (accelVal > kAccelMax) accelVal = kAccelMax;
  int mi = motorMaxRpmMilliUi;
  if (mi < kMaxRpmMilliMin) mi = kMaxRpmMilliMin;
  if (mi > kMaxRpmMilliMax) mi = kMaxRpmMilliMax;
  float maxRpmVal = mi / 1000.0f;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.microstep = microOptions[selectedMicro];
  g_settings.acceleration = (uint32_t)accelVal;
  g_settings.max_rpm = maxRpmVal;
  g_settings.dir_switch_enabled = dirSwitchEnabled;
  g_settings.invert_direction = invertDir;
  xSemaphoreGive(g_settings_mutex);
  g_dir_switch_cache.store(dirSwitchEnabled, std::memory_order_release);
  storage_save_settings();
  motorConfigApplyPending.store(true, std::memory_order_release);
  if (saveFeedbackLabel) {
    lv_label_set_text(saveFeedbackLabel, "Settings saved!");
    lv_obj_set_style_text_color(saveFeedbackLabel, COL_GREEN, 0);
  }
  if (saveNavTimer) lv_timer_delete(saveNavTimer);
  saveNavTimer = lv_timer_create(save_nav_timer_cb, 800, nullptr);
  lv_timer_set_repeat_count(saveNavTimer, 1);
}

void screen_motor_config_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MOTOR_CONFIG];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  {
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    dirSwitchEnabled = g_settings.dir_switch_enabled;
    invertDir = g_settings.invert_direction;
    motorAccelUi = (int)g_settings.acceleration;
    motorMaxRpmMilliUi = (int)(g_settings.max_rpm * 1000.0f + 0.5f);
    xSemaphoreGive(g_settings_mutex);
  }
  if (motorAccelUi < kAccelMin) motorAccelUi = kAccelMin;
  if (motorAccelUi > kAccelMax) motorAccelUi = kAccelMax;
  if (motorMaxRpmMilliUi < kMaxRpmMilliMin) motorMaxRpmMilliUi = kMaxRpmMilliMin;
  if (motorMaxRpmMilliUi > kMaxRpmMilliMax) motorMaxRpmMilliUi = kMaxRpmMilliMax;

  ui_create_settings_header(screen, "MOTOR CONFIG", "DM542T", COL_TEXT_DIM);

  const int ROW_X = 20;
  const int ROW_W = 760;
  const int ROW_H = 54;
  const int ROW_GAP = 8;
  int y = HEADER_H + 22;

  lv_obj_t* microRow = motor_cfg_post_row(screen, ROW_X, y, ROW_W, ROW_H);
  lv_obj_t* microTitleLbl = lv_label_create(microRow);
  lv_label_set_text(microTitleLbl, "MICROSTEP");
  lv_obj_set_style_text_font(microTitleLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(microTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(microTitleLbl, 22, 18);

  microSummaryLbl = lv_label_create(microRow);
  lv_obj_set_style_text_font(microSummaryLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(microSummaryLbl, COL_TEXT, 0);
  // Keep left of micro buttons (they start ~344px) so value does not paint under 800/1600
  lv_obj_set_pos(microSummaryLbl, 200, 16);
  lv_obj_set_width(microSummaryLbl, 130);
  lv_label_set_long_mode(microSummaryLbl, LV_LABEL_LONG_MODE_CLIP);

  MicrostepSetting currentMicro = microstep_get();
  selectedMicro = 0;
  const int microBtnW = 96;
  const int microBtnH = 36;
  const int microBtnY = (ROW_H - microBtnH) / 2;
  const int microBtnGap = 8;
  const int microBtn0X =
      ROW_W - 8 - kMicroOptionCount * microBtnW - (kMicroOptionCount - 1) * microBtnGap;
  for (int i = 0; i < kMicroOptionCount; i++) {
    if (microOptions[i] == currentMicro) selectedMicro = i;

    microBtns[i] = lv_button_create(microRow);
    lv_obj_set_size(microBtns[i], microBtnW, microBtnH);
    lv_obj_set_pos(microBtns[i], microBtn0X + i * (microBtnW + microBtnGap), microBtnY);
    lv_obj_add_event_cb(microBtns[i], micro_btn_cb, LV_EVENT_CLICKED, (void*)(size_t)i);

    const UiBtnStyle ms = (i == selectedMicro) ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(microBtns[i], ms);

    microLabels[i] = lv_label_create(microBtns[i]);
    lv_label_set_text(microLabels[i], microSprLabels[i]);
    lv_obj_set_style_text_font(microLabels[i], FONT_NORMAL, 0);
    lv_obj_set_style_text_color(microLabels[i], ui_btn_label_color_post(ms), 0);
    lv_obj_center(microLabels[i]);
  }
  lv_label_set_text(microSummaryLbl, microSprLabels[selectedMicro]);

  y += ROW_H + ROW_GAP;

  const int rpmPmColW = BTN_W_PM * 2 + 8 + 8 + 8;
  const int maxRpmRowH = 72;
  lv_obj_t* maxRpmRow = motor_cfg_post_row(screen, ROW_X, y, ROW_W, maxRpmRowH);
  lv_obj_t* maxRpmTitleLbl = lv_label_create(maxRpmRow);
  lv_label_set_text(maxRpmTitleLbl, "MAX RPM");
  lv_obj_set_style_text_font(maxRpmTitleLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(maxRpmTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(maxRpmTitleLbl, 22, 12);

  maxRpmValueLabel = lv_label_create(maxRpmRow);
  lv_obj_set_style_text_font(maxRpmValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(maxRpmValueLabel, COL_TEXT, 0);
  lv_obj_set_pos(maxRpmValueLabel, 500, 10);

  maxRpmSlider = lv_slider_create(maxRpmRow);
  lv_slider_set_range(maxRpmSlider, kMaxRpmMilliMin, kMaxRpmMilliMax);
  lv_obj_set_size(maxRpmSlider, ROW_W - 22 - rpmPmColW, SET_SLIDER_H);
  lv_obj_set_pos(maxRpmSlider, 22, 42);
  lv_obj_add_event_cb(maxRpmSlider, max_rpm_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  motor_config_max_rpm_sync_ui(motorMaxRpmMilliUi);

  lv_obj_t* maxRpmMinus = ui_create_pm_btn(maxRpmRow, ROW_W - BTN_W_PM - 8 - BTN_W_PM - 8, 8, "-", FONT_NORMAL,
                                           UI_BTN_NORMAL, max_rpm_pm_cb, (void*)(intptr_t)-1);
  lv_obj_t* maxRpmPlus =
      ui_create_pm_btn(maxRpmRow, ROW_W - BTN_W_PM - 8, 8, "+", FONT_NORMAL, UI_BTN_ACCENT, max_rpm_pm_cb,
                       (void*)(intptr_t)1);
  lv_obj_move_foreground(maxRpmMinus);
  lv_obj_move_foreground(maxRpmPlus);

  y += maxRpmRowH + ROW_GAP;

  const int accelRowH = 72;
  lv_obj_t* accelRow = motor_cfg_post_row(screen, ROW_X, y, ROW_W, accelRowH);
  lv_obj_t* accelTitleLbl = lv_label_create(accelRow);
  lv_label_set_text(accelTitleLbl, "ACCELERATION");
  lv_obj_set_style_text_font(accelTitleLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(accelTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(accelTitleLbl, 22, 12);

  accelValueLabel = lv_label_create(accelRow);
  lv_obj_set_style_text_font(accelValueLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(accelValueLabel, COL_TEXT, 0);
  lv_obj_set_pos(accelValueLabel, 360, 10);
  lv_obj_set_width(accelValueLabel, 260);
  lv_label_set_long_mode(accelValueLabel, LV_LABEL_LONG_MODE_CLIP);

  accelSlider = lv_slider_create(accelRow);
  lv_slider_set_range(accelSlider, kAccelMin, kAccelMax);
  lv_obj_set_size(accelSlider, ROW_W - 22 - rpmPmColW, SET_SLIDER_H);
  lv_obj_set_pos(accelSlider, 22, 42);
  lv_obj_add_event_cb(accelSlider, accel_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  motor_config_accel_sync_ui(motorAccelUi);

  lv_obj_t* accelMinus = ui_create_pm_btn(accelRow, ROW_W - BTN_W_PM - 8 - BTN_W_PM - 8, 8, "-", FONT_NORMAL,
                                          UI_BTN_NORMAL, accel_pm_cb, (void*)(intptr_t)-1);
  lv_obj_t* accelPlus =
      ui_create_pm_btn(accelRow, ROW_W - BTN_W_PM - 8, 8, "+", FONT_NORMAL, UI_BTN_ACCENT, accel_pm_cb,
                       (void*)(intptr_t)1);
  lv_obj_move_foreground(accelMinus);
  lv_obj_move_foreground(accelPlus);

  y += accelRowH + ROW_GAP;

  lv_obj_t* warnBar = lv_obj_create(screen);
  lv_obj_set_size(warnBar, ROW_W, ROW_H);
  lv_obj_set_pos(warnBar, ROW_X, y);
  ui_style_post_warn(warnBar);
  lv_obj_remove_flag(warnBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(warnBar, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* warnKey = lv_label_create(warnBar);
  lv_label_set_text(warnKey, "APPLY SAFELY");
  lv_obj_set_style_text_font(warnKey, FONT_SMALL, 0);
  lv_obj_set_style_text_color(warnKey, COL_YELLOW, 0);
  lv_obj_set_pos(warnKey, 22, 18);

  lv_obj_t* warnDetail = lv_label_create(warnBar);
  lv_label_set_text(warnDetail, "Changes queued for motor task");
  lv_obj_set_style_text_font(warnDetail, FONT_SMALL, 0);
  lv_obj_set_style_text_color(warnDetail, COL_TEXT, 0);
  lv_obj_set_pos(warnDetail, 200, 18);
  lv_obj_set_width(warnDetail, 540);
  lv_label_set_long_mode(warnDetail, LV_LABEL_LONG_MODE_CLIP);

  y += ROW_H + ROW_GAP;

  lv_obj_t* toggleRow = lv_obj_create(screen);
  lv_obj_set_size(toggleRow, ROW_W, 50);
  lv_obj_set_pos(toggleRow, ROW_X, y);
  ui_style_post_row(toggleRow);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* invertTitleLbl = lv_label_create(toggleRow);
  lv_label_set_text(invertTitleLbl, "INVERT");
  lv_obj_set_style_text_font(invertTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(invertTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(invertTitleLbl, 8, 14);

  invertToggle = lv_obj_create(toggleRow);
  lv_obj_set_size(invertToggle, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_set_pos(invertToggle, 96, 5);
  lv_obj_set_style_radius(invertToggle, SET_TOGGLE_R, 0);
  lv_obj_set_style_pad_all(invertToggle, 0, 0);
  lv_obj_remove_flag(invertToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(invertToggle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(invertToggle, invert_toggle_cb, LV_EVENT_CLICKED, nullptr);

  invertToggleLbl = lv_label_create(invertToggle);
  motor_config_apply_invert_pill(invertDir);

  lv_obj_t* idleTitleLbl = lv_label_create(toggleRow);
  lv_label_set_text(idleTitleLbl, "DIR SW");
  lv_obj_set_style_text_font(idleTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(idleTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(idleTitleLbl, 392, 14);

  idleToggle = lv_obj_create(toggleRow);
  lv_obj_set_size(idleToggle, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_set_pos(idleToggle, 500, 5);
  lv_obj_set_style_radius(idleToggle, SET_TOGGLE_R, 0);
  lv_obj_set_style_border_width(idleToggle, 0, 0);
  lv_obj_set_style_pad_all(idleToggle, 0, 0);
  lv_obj_remove_flag(idleToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(idleToggle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(idleToggle, idle_toggle_cb, LV_EVENT_CLICKED, nullptr);

  idleToggleLbl = lv_label_create(idleToggle);
  motor_config_apply_dir_sw_pill(dirSwitchEnabled);

  y += 50 + 4;

  lv_obj_t* statusRow = lv_obj_create(screen);
  lv_obj_set_size(statusRow, ROW_W, 28);
  lv_obj_set_pos(statusRow, ROW_X, y);
  ui_style_post_row(statusRow);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_CLICKABLE);

  rpmRangeVal = lv_label_create(statusRow);
  lv_obj_set_style_text_font(rpmRangeVal, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmRangeVal, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmRangeVal, 8, 6);
  motor_config_max_rpm_sync_ui(motorMaxRpmMilliUi);

  statusLabel = lv_label_create(statusRow);
  lv_label_set_text(statusLabel, "MOTOR: IDLE");
  lv_obj_set_style_text_font(statusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(statusLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(statusLabel, 380, 6);

  saveFeedbackLabel = lv_label_create(statusRow);
  lv_label_set_text(saveFeedbackLabel, "");
  lv_obj_set_style_text_font(saveFeedbackLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(saveFeedbackLabel, COL_GREEN, 0);
  lv_obj_align(saveFeedbackLabel, LV_ALIGN_RIGHT_MID, -8, 0);

  const int footerY = 428;
  const int footerH = 46;
  ui_create_btn(screen, 20, footerY, 246, footerH, "< BACK", FONT_NORMAL, UI_BTN_NORMAL, back_cb, nullptr);
  ui_create_btn(screen, 534, footerY, 246, footerH, "SAVE & APPLY", FONT_NORMAL, UI_BTN_ACCENT, save_apply_cb, nullptr);
}

void screen_motor_config_invalidate_widgets() {
  for (int i = 0; i < kMicroOptionCount; i++) { microBtns[i] = nullptr; microLabels[i] = nullptr; }
  microSummaryLbl = nullptr;
  accelValueLabel = nullptr;
  maxRpmValueLabel = nullptr;
  maxRpmSlider = nullptr;
  accelSlider = nullptr;
  rpmRangeVal = nullptr;
  saveFeedbackLabel = nullptr;
  invertToggle = nullptr;
  invertToggleLbl = nullptr;
  idleToggle = nullptr;
  idleToggleLbl = nullptr;
  statusLabel = nullptr;
  saveNavTimer = nullptr;
  lastStatusState = (SystemState)-1;
}

void screen_motor_config_update() {
  if (!statusLabel) return;
  SystemState state = control_get_state();
  if (state == lastStatusState) return;
  lastStatusState = state;
  char buf[48];
  snprintf(buf, sizeof(buf), "MOTOR: %s", control_state_name(state));
  lv_label_set_text(statusLabel, buf);
}
