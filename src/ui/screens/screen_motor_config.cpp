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
#include <atomic>

static const MicrostepSetting microOptions[3] = {MICROSTEP_8, MICROSTEP_16, MICROSTEP_32};
// NEMA 200 full steps/rev x microstep = driver PULSE/REV (match DM542 DIP table)
static const char* microStrings[3] = {"1600", "3200", "6400"};
static int selectedMicro = 0;
static lv_obj_t* microBtns[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* microLabels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* accelSlider = nullptr;
static lv_obj_t* accelValueLabel = nullptr;
static lv_obj_t* maxRpmSlider = nullptr;
static lv_obj_t* maxRpmValueLabel = nullptr;
static lv_obj_t* rpmRangeVal = nullptr;

// Max RPM UI: 1..3000 = 0.001 .. 3.000 RPM (matches MIN_RPM / MAX_RPM)
static constexpr int kMaxRpmMilliMin = 1;
static constexpr int kMaxRpmMilliMax = 3000;

// Must match motor/acceleration.cpp ACCEL_MIN / ACCEL_MAX
static constexpr int kAccelMin = 1000;
static constexpr int kAccelMax = 30000;

static void motor_config_accel_sync_ui(int val) {
  if (val < kAccelMin) val = kAccelMin;
  if (val > kAccelMax) val = kAccelMax;
  if (accelSlider && lv_slider_get_value(accelSlider) != val) {
    lv_slider_set_value(accelSlider, val, LV_ANIM_OFF);
  }
  if (accelValueLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    lv_label_set_text(accelValueLabel, buf);
  }
}

static void motor_config_max_rpm_sync_ui(int milli) {
  if (milli < kMaxRpmMilliMin) milli = kMaxRpmMilliMin;
  if (milli > kMaxRpmMilliMax) milli = kMaxRpmMilliMax;
  if (maxRpmSlider && lv_slider_get_value(maxRpmSlider) != milli) {
    lv_slider_set_value(maxRpmSlider, milli, LV_ANIM_OFF);
  }
  if (maxRpmValueLabel) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%.3f", milli / 1000.0f);
    lv_label_set_text(maxRpmValueLabel, buf);
  }
  if (rpmRangeVal) {
    char buf[28];
    snprintf(buf, sizeof(buf), "%.3f - %.3f", (double)MIN_RPM, milli / 1000.0);
    lv_label_set_text(rpmRangeVal, buf);
  }
}

static void max_rpm_slider_cb(lv_event_t* e) {
  (void)e;
  if (!maxRpmSlider) return;
  motor_config_max_rpm_sync_ui(lv_slider_get_value(maxRpmSlider));
}

static lv_obj_t* gearLabel = nullptr;
static lv_obj_t* saveFeedbackLabel = nullptr;
static bool invertDir = false;
static lv_obj_t* invertToggle = nullptr;
static lv_obj_t* invertToggleLbl = nullptr;
static bool dirSwitchEnabled = false;
static lv_obj_t* idleToggle = nullptr;
static lv_obj_t* idleToggleLbl = nullptr;
static lv_obj_t* statusLabel = nullptr;

std::atomic<bool> motorConfigApplyPending{false};

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static void micro_btn_cb(lv_event_t* e) {
  int idx = (int)(size_t)lv_event_get_user_data(e);
  selectedMicro = idx;
  for (int i = 0; i < 3; i++) {
    lv_obj_set_style_bg_color(microBtns[i], i == idx ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_color(microBtns[i], i == idx ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_border_width(microBtns[i], i == idx ? 2 : 1, 0);
    lv_obj_set_style_text_color(microLabels[i], i == idx ? COL_ACCENT : COL_TEXT, 0);
  }
}

static void accel_slider_cb(lv_event_t* e) {
  (void)e;
  if (!accelSlider || !accelValueLabel) return;
  motor_config_accel_sync_ui(lv_slider_get_value(accelSlider));
}

static void invert_toggle_cb(lv_event_t* e) {
  (void)e;
  invertDir = !invertDir;
  lv_obj_set_style_bg_color(invertToggle, invertDir ? COL_ACCENT : COL_TOGGLE_OFF, 0);
  lv_label_set_text(invertToggleLbl, invertDir ? "ON" : "OFF");
}

static void idle_toggle_cb(lv_event_t* e) {
  (void)e;
  dirSwitchEnabled = !dirSwitchEnabled;
  lv_obj_set_style_bg_color(idleToggle, dirSwitchEnabled ? COL_GREEN : COL_TOGGLE_OFF, 0);
  lv_label_set_text(idleToggleLbl, dirSwitchEnabled ? "ON" : "OFF");
}

static lv_timer_t* saveNavTimer = nullptr;

static void save_nav_timer_cb(lv_timer_t* timer) {
  saveNavTimer = nullptr;
  screens_show(SCREEN_SETTINGS);
}

static void save_apply_cb(lv_event_t* e) {
  g_settings.microstep = microOptions[selectedMicro];
  int accelVal = lv_slider_get_value(accelSlider);
  if (accelVal < kAccelMin) accelVal = kAccelMin;
  if (accelVal > kAccelMax) accelVal = kAccelMax;
  g_settings.acceleration = (uint32_t)accelVal;
  if (maxRpmSlider) {
    int mi = lv_slider_get_value(maxRpmSlider);
    if (mi < kMaxRpmMilliMin) mi = kMaxRpmMilliMin;
    if (mi > kMaxRpmMilliMax) mi = kMaxRpmMilliMax;
    g_settings.max_rpm = mi / 1000.0f;
  }
  g_settings.dir_switch_enabled = dirSwitchEnabled;
  g_settings.invert_direction = invertDir;
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

// Track must read clearly on COL_BG (0x05); border + opa match working display brightness slider.
static void style_slider(lv_obj_t* slider) {
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x3A3A3A), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, COL_BORDER, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, COL_TEXT_DIM, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, 10, LV_PART_KNOB);
}

void screen_motor_config_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MOTOR_CONFIG];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  dirSwitchEnabled = g_settings.dir_switch_enabled;
  invertDir = g_settings.invert_direction;

  const int PX = 16;
  const int CONTENT_W = SCREEN_W - 2 * PX;

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, SET_HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "MOTOR CONFIG");
  lv_obj_set_style_text_font(title, SET_HEADER_FONT, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PX, 6);

  constexpr int GAP_Y = 5;
  int y = SET_HEADER_H + GAP_Y;

  lv_obj_t* microRow = lv_obj_create(screen);
  lv_obj_set_size(microRow, CONTENT_W, 46);
  lv_obj_set_pos(microRow, PX, y);
  lv_obj_set_style_bg_color(microRow, COL_BG, 0);
  lv_obj_set_style_border_width(microRow, 0, 0);
  lv_obj_set_style_radius(microRow, 0, 0);
  lv_obj_set_style_pad_all(microRow, 0, 0);
  lv_obj_remove_flag(microRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(microRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* microTitleLbl = lv_label_create(microRow);
  lv_label_set_text(microTitleLbl, "PULSE/REV");
  lv_obj_set_style_text_font(microTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(microTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(microTitleLbl, 0, 0);
  lv_obj_set_width(microTitleLbl, 118);
  lv_label_set_long_mode(microTitleLbl, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_align(microTitleLbl, LV_ALIGN_LEFT_MID, 0, 0);

  MicrostepSetting currentMicro = microstep_get();
  selectedMicro = 0;
  for (int i = 0; i < 3; i++) {
    if (microOptions[i] == currentMicro) selectedMicro = i;

    microBtns[i] = lv_button_create(microRow);
    lv_obj_set_size(microBtns[i], 108, 36);
    lv_obj_set_pos(microBtns[i], 124 + i * 116, 5);
    lv_obj_set_style_radius(microBtns[i], RADIUS_BTN, 0);
    lv_obj_set_style_shadow_width(microBtns[i], 0, 0);
    lv_obj_set_style_pad_all(microBtns[i], 0, 0);
    lv_obj_add_event_cb(microBtns[i], micro_btn_cb, LV_EVENT_CLICKED, (void*)(size_t)i);

    if (i == selectedMicro) {
      lv_obj_set_style_bg_color(microBtns[i], COL_BG_ACTIVE, 0);
      lv_obj_set_style_border_color(microBtns[i], COL_ACCENT, 0);
      lv_obj_set_style_border_width(microBtns[i], 2, 0);
    } else {
      lv_obj_set_style_bg_color(microBtns[i], COL_BTN_BG, 0);
      lv_obj_set_style_border_color(microBtns[i], COL_BORDER, 0);
      lv_obj_set_style_border_width(microBtns[i], 1, 0);
    }

    microLabels[i] = lv_label_create(microBtns[i]);
    lv_label_set_text(microLabels[i], microStrings[i]);
    lv_obj_set_style_text_font(microLabels[i], FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(microLabels[i], i == selectedMicro ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(microLabels[i]);
  }

  y += 46 + GAP_Y;

  lv_obj_t* accelRow = lv_obj_create(screen);
  lv_obj_set_size(accelRow, CONTENT_W, 72);
  lv_obj_set_pos(accelRow, PX, y);
  lv_obj_set_style_bg_color(accelRow, COL_BG, 0);
  lv_obj_set_style_border_width(accelRow, 0, 0);
  lv_obj_set_style_radius(accelRow, 0, 0);
  lv_obj_set_style_pad_all(accelRow, 0, 0);
  lv_obj_remove_flag(accelRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(accelRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* accelTitleLbl = lv_label_create(accelRow);
  lv_label_set_text(accelTitleLbl, "ACCELERATION");
  lv_obj_set_style_text_font(accelTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(accelTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(accelTitleLbl, 8, 4);

  lv_obj_t* accelUnitLbl = lv_label_create(accelRow);
  lv_label_set_text(accelUnitLbl, "steps/s2");
  lv_obj_set_style_text_font(accelUnitLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(accelUnitLbl, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(accelUnitLbl, 8, 22);

  accelValueLabel = lv_label_create(accelRow);
  lv_obj_set_style_text_font(accelValueLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(accelValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_align(accelValueLabel, LV_ALIGN_TOP_RIGHT, -8, 8);

  accelSlider = lv_slider_create(accelRow);
  lv_obj_set_size(accelSlider, CONTENT_W - 16, 26);
  lv_obj_set_pos(accelSlider, 8, 40);
  lv_slider_set_range(accelSlider, kAccelMin, kAccelMax);
  lv_slider_set_value(accelSlider, (int)acceleration_get(), LV_ANIM_OFF);
  motor_config_accel_sync_ui((int)acceleration_get());
  style_slider(accelSlider);
  lv_obj_add_event_cb(accelSlider, accel_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  y += 72 + GAP_Y;

  lv_obj_t* maxRpmRow = lv_obj_create(screen);
  lv_obj_set_size(maxRpmRow, CONTENT_W, 72);
  lv_obj_set_pos(maxRpmRow, PX, y);
  lv_obj_set_style_bg_color(maxRpmRow, COL_BG, 0);
  lv_obj_set_style_border_width(maxRpmRow, 0, 0);
  lv_obj_set_style_radius(maxRpmRow, 0, 0);
  lv_obj_set_style_pad_all(maxRpmRow, 0, 0);
  lv_obj_remove_flag(maxRpmRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(maxRpmRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* maxRpmTitleLbl = lv_label_create(maxRpmRow);
  lv_label_set_text(maxRpmTitleLbl, "MAX RPM");
  lv_obj_set_style_text_font(maxRpmTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(maxRpmTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(maxRpmTitleLbl, 8, 4);

  lv_obj_t* maxRpmUnitLbl = lv_label_create(maxRpmRow);
  lv_label_set_text(maxRpmUnitLbl, "0.001 - 3.0");
  lv_obj_set_style_text_font(maxRpmUnitLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(maxRpmUnitLbl, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(maxRpmUnitLbl, 8, 22);

  maxRpmValueLabel = lv_label_create(maxRpmRow);
  lv_obj_set_style_text_font(maxRpmValueLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(maxRpmValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_align(maxRpmValueLabel, LV_ALIGN_TOP_RIGHT, -8, 8);

  maxRpmSlider = lv_slider_create(maxRpmRow);
  {
    const int sliderW = CONTENT_W - 16;
    const int sliderH = 26;
    lv_obj_set_size(maxRpmSlider, sliderW, sliderH);
    lv_obj_set_pos(maxRpmSlider, 8, 40);
    lv_slider_set_range(maxRpmSlider, kMaxRpmMilliMin, kMaxRpmMilliMax);
    int mi = (int)(g_settings.max_rpm * 1000.0f + 0.5f);
    if (mi < kMaxRpmMilliMin) mi = kMaxRpmMilliMin;
    if (mi > kMaxRpmMilliMax) mi = kMaxRpmMilliMax;
    lv_slider_set_value(maxRpmSlider, mi, LV_ANIM_OFF);
  }
  style_slider(maxRpmSlider);
  lv_obj_add_event_cb(maxRpmSlider, max_rpm_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  motor_config_max_rpm_sync_ui(lv_slider_get_value(maxRpmSlider));

  y += 72 + GAP_Y;

  lv_obj_t* infoRow = lv_obj_create(screen);
  lv_obj_set_size(infoRow, CONTENT_W, 40);
  lv_obj_set_pos(infoRow, PX, y);
  lv_obj_set_style_bg_color(infoRow, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(infoRow, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(infoRow, 1, 0);
  lv_obj_set_style_radius(infoRow, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(infoRow, 0, 0);
  lv_obj_set_style_pad_all(infoRow, 0, 0);
  lv_obj_remove_flag(infoRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(infoRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* gearTitleLbl = lv_label_create(infoRow);
  lv_label_set_text(gearTitleLbl, "GEAR");
  lv_obj_set_style_text_font(gearTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(gearTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(gearTitleLbl, 10, 10);

  gearLabel = lv_label_create(infoRow);
  char gearBuf[20];
  snprintf(gearBuf, sizeof(gearBuf), "1:%.0f", (double)GEAR_RATIO);
  lv_label_set_text(gearLabel, gearBuf);
  lv_obj_set_style_text_font(gearLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(gearLabel, COL_TEXT, 0);
  lv_obj_set_pos(gearLabel, 64, 8);

  lv_obj_t* rpmRangeTitleLbl = lv_label_create(infoRow);
  lv_label_set_text(rpmRangeTitleLbl, "RPM");
  lv_obj_set_style_text_font(rpmRangeTitleLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(rpmRangeTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmRangeTitleLbl, 210, 10);

  rpmRangeVal = lv_label_create(infoRow);
  char rpmBuf[28];
  snprintf(rpmBuf, sizeof(rpmBuf), "%.3f - %.3f", (double)MIN_RPM, (double)g_settings.max_rpm);
  lv_label_set_text(rpmRangeVal, rpmBuf);
  lv_obj_set_style_text_font(rpmRangeVal, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rpmRangeVal, COL_TEXT, 0);
  lv_obj_set_pos(rpmRangeVal, 268, 8);
  lv_obj_set_width(rpmRangeVal, CONTENT_W - 276);
  lv_label_set_long_mode(rpmRangeVal, LV_LABEL_LONG_MODE_CLIP);

  if (maxRpmSlider) motor_config_max_rpm_sync_ui(lv_slider_get_value(maxRpmSlider));

  y += 40 + GAP_Y;

  lv_obj_t* toggleRow = lv_obj_create(screen);
  lv_obj_set_size(toggleRow, CONTENT_W, 50);
  lv_obj_set_pos(toggleRow, PX, y);
  lv_obj_set_style_bg_color(toggleRow, COL_BG, 0);
  lv_obj_set_style_border_width(toggleRow, 0, 0);
  lv_obj_set_style_radius(toggleRow, 0, 0);
  lv_obj_set_style_pad_all(toggleRow, 0, 0);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_SCROLLABLE);
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
  lv_obj_set_style_border_width(invertToggle, 0, 0);
  lv_obj_set_style_pad_all(invertToggle, 0, 0);
  lv_obj_remove_flag(invertToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(invertToggle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(invertToggle, invertDir ? COL_ACCENT : COL_TOGGLE_OFF, 0);
  lv_obj_add_event_cb(invertToggle, invert_toggle_cb, LV_EVENT_CLICKED, nullptr);

  invertToggleLbl = lv_label_create(invertToggle);
  lv_label_set_text(invertToggleLbl, invertDir ? "ON" : "OFF");
  lv_obj_set_style_text_font(invertToggleLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(invertToggleLbl, COL_TEXT_WHITE, 0);
  lv_obj_center(invertToggleLbl);

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
  lv_obj_set_style_bg_color(idleToggle, dirSwitchEnabled ? COL_GREEN : COL_TOGGLE_OFF, 0);
  lv_obj_add_event_cb(idleToggle, idle_toggle_cb, LV_EVENT_CLICKED, nullptr);

  idleToggleLbl = lv_label_create(idleToggle);
  lv_label_set_text(idleToggleLbl, dirSwitchEnabled ? "ON" : "OFF");
  lv_obj_set_style_text_font(idleToggleLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(idleToggleLbl, COL_TEXT_WHITE, 0);
  lv_obj_center(idleToggleLbl);

  y += 50 + GAP_Y;

  lv_obj_t* warnBar = lv_obj_create(screen);
  lv_obj_set_size(warnBar, CONTENT_W, 32);
  lv_obj_set_pos(warnBar, PX, y);
  lv_obj_set_style_bg_color(warnBar, COL_BG_DANGER, 0);
  lv_obj_set_style_border_color(warnBar, COL_BORDER_DNG, 0);
  lv_obj_set_style_border_width(warnBar, 1, 0);
  lv_obj_set_style_radius(warnBar, RADIUS_BTN, 0);
  lv_obj_set_style_shadow_width(warnBar, 0, 0);
  lv_obj_set_style_pad_all(warnBar, 0, 0);
  lv_obj_remove_flag(warnBar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(warnBar, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* warnLbl = lv_label_create(warnBar);
  lv_label_set_text(warnLbl, "! Stop motor before microstep change");
  lv_obj_set_style_text_font(warnLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(warnLbl, COL_RED, 0);
  lv_obj_align(warnLbl, LV_ALIGN_LEFT_MID, 8, 0);

  y += 32 + GAP_Y;

  lv_obj_t* statusRow = lv_obj_create(screen);
  lv_obj_set_size(statusRow, CONTENT_W, 32);
  lv_obj_set_pos(statusRow, PX, y);
  lv_obj_set_style_bg_color(statusRow, COL_BG_DIM, 0);
  lv_obj_set_style_border_width(statusRow, 0, 0);
  lv_obj_set_style_radius(statusRow, 0, 0);
  lv_obj_set_style_pad_all(statusRow, 0, 0);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_CLICKABLE);

  statusLabel = lv_label_create(statusRow);
  lv_label_set_text(statusLabel, "MOTOR: IDLE");
  lv_obj_set_style_text_font(statusLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(statusLabel, COL_TEXT_DIM, 0);
  lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 8, 0);

  saveFeedbackLabel = lv_label_create(statusRow);
  lv_label_set_text(saveFeedbackLabel, "");
  lv_obj_set_style_text_font(saveFeedbackLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(saveFeedbackLabel, COL_GREEN, 0);
  lv_obj_align(saveFeedbackLabel, LV_ALIGN_RIGHT_MID, -8, 0);

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = 170;
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

  lv_obj_t* saveBtn = lv_button_create(screen);
  lv_obj_set_size(saveBtn, btnW + 80, footerH);
  lv_obj_set_pos(saveBtn, PX + btnW + gap, footerY);
  lv_obj_set_style_bg_color(saveBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(saveBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(saveBtn, 2, 0);
  lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(saveBtn, 0, 0);
  lv_obj_set_style_pad_all(saveBtn, 0, 0);
  lv_obj_add_event_cb(saveBtn, save_apply_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* sLbl = lv_label_create(saveBtn);
  lv_label_set_text(sLbl, "SAVE & APPLY");
  lv_obj_set_style_text_font(sLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(sLbl, COL_ACCENT, 0);
  lv_obj_center(sLbl);
}

void screen_motor_config_invalidate_widgets() {
  for (int i = 0; i < 3; i++) { microBtns[i] = nullptr; microLabels[i] = nullptr; }
  accelSlider = nullptr;
  accelValueLabel = nullptr;
  maxRpmSlider = nullptr;
  maxRpmValueLabel = nullptr;
  rpmRangeVal = nullptr;
  gearLabel = nullptr;
  saveFeedbackLabel = nullptr;
  invertToggle = nullptr;
  invertToggleLbl = nullptr;
  idleToggle = nullptr;
  idleToggleLbl = nullptr;
  statusLabel = nullptr;
  saveNavTimer = nullptr;
}

void screen_motor_config_update() {
  if (!statusLabel) return;
  SystemState st = control_get_state();
  const char* stateStr = (st == STATE_IDLE) ? "IDLE" : (st == STATE_ESTOP) ? "ESTOP" : "RUNNING";
  char buf[32];
  snprintf(buf, sizeof(buf), "MOTOR: %s", stateStr);
  lv_label_set_text(statusLabel, buf);
}
