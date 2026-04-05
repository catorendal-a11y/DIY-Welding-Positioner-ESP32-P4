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
static const char* microStrings[3] = {"1/8", "1/16", "1/32"};
static int selectedMicro = 0;
static lv_obj_t* microBtns[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* microLabels[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* accelSlider = nullptr;
static lv_obj_t* accelValueLabel = nullptr;
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
  int val = lv_slider_get_value(accelSlider);
  char buf[24];
  snprintf(buf, sizeof(buf), "%d steps/s2", val);
  lv_label_set_text(accelValueLabel, buf);
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
  g_settings.acceleration = (uint32_t)lv_slider_get_value(accelSlider);
  g_settings.dir_switch_enabled = dirSwitchEnabled;
  g_settings.invert_direction = invertDir;
  g_dir_switch_cache.store(dirSwitchEnabled, std::memory_order_release);
  storage_save_settings();
  motorConfigApplyPending = true;
  if (saveFeedbackLabel) {
    lv_label_set_text(saveFeedbackLabel, "Settings saved!");
    lv_obj_set_style_text_color(saveFeedbackLabel, COL_GREEN, 0);
  }
  if (saveNavTimer) lv_timer_delete(saveNavTimer);
  saveNavTimer = lv_timer_create(save_nav_timer_cb, 800, nullptr);
  lv_timer_set_repeat_count(saveNavTimer, 1);
}

static void style_slider(lv_obj_t* slider) {
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x222222), 0);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 0, 0);
  lv_obj_set_style_radius(slider, 4, 0);
  lv_obj_set_style_pad_all(slider, 0, 0);
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

  int y = 42;

  lv_obj_t* microRow = lv_obj_create(screen);
  lv_obj_set_size(microRow, CONTENT_W, 48);
  lv_obj_set_pos(microRow, PX, y);
  lv_obj_set_style_bg_color(microRow, COL_BG, 0);
  lv_obj_set_style_border_width(microRow, 0, 0);
  lv_obj_set_style_radius(microRow, 0, 0);
  lv_obj_set_style_pad_all(microRow, 0, 0);
  lv_obj_remove_flag(microRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(microRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* microTitleLbl = lv_label_create(microRow);
  lv_label_set_text(microTitleLbl, "MICROSTEP");
  lv_obj_set_style_text_font(microTitleLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(microTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(microTitleLbl, LV_ALIGN_LEFT_MID, 0, 0);

  MicrostepSetting currentMicro = microstep_get();
  for (int i = 0; i < 3; i++) {
    if (microOptions[i] == currentMicro) selectedMicro = i;

    microBtns[i] = lv_button_create(microRow);
    lv_obj_set_size(microBtns[i], 100, 36);
    lv_obj_set_pos(microBtns[i], 140 + i * 110, 6);
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

  y += 56;

  lv_obj_t* accelRow = lv_obj_create(screen);
  lv_obj_set_size(accelRow, CONTENT_W, 48);
  lv_obj_set_pos(accelRow, PX, y);
  lv_obj_set_style_bg_color(accelRow, COL_BG, 0);
  lv_obj_set_style_border_width(accelRow, 0, 0);
  lv_obj_set_style_radius(accelRow, 0, 0);
  lv_obj_set_style_pad_all(accelRow, 0, 0);
  lv_obj_remove_flag(accelRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(accelRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* accelTitleLbl = lv_label_create(accelRow);
  lv_label_set_text(accelTitleLbl, "ACCELERATION");
  lv_obj_set_style_text_font(accelTitleLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(accelTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(accelTitleLbl, LV_ALIGN_LEFT_MID, 0, 0);

  accelValueLabel = lv_label_create(accelRow);
  char accelBuf[24];
  snprintf(accelBuf, sizeof(accelBuf), "%d steps/s2", acceleration_get());
  lv_label_set_text(accelValueLabel, accelBuf);
  lv_obj_set_style_text_font(accelValueLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(accelValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_align(accelValueLabel, LV_ALIGN_RIGHT_MID, -180, 0);

  accelSlider = lv_slider_create(accelRow);
  lv_obj_set_size(accelSlider, 200, 20);
  lv_obj_align(accelSlider, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_slider_set_range(accelSlider, 1000, 20000);
  lv_slider_set_value(accelSlider, (int)acceleration_get(), LV_ANIM_OFF);
  style_slider(accelSlider);
  lv_obj_add_event_cb(accelSlider, accel_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  y += 56;

  lv_obj_t* infoRow = lv_obj_create(screen);
  lv_obj_set_size(infoRow, CONTENT_W, 44);
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
  lv_label_set_text(gearTitleLbl, "GEAR RATIO");
  lv_obj_set_style_text_font(gearTitleLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(gearTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(gearTitleLbl, LV_ALIGN_LEFT_MID, 12, 0);

  gearLabel = lv_label_create(infoRow);
  char gearBuf[20];
  snprintf(gearBuf, sizeof(gearBuf), "%.0f:1", GEAR_RATIO);
  lv_label_set_text(gearLabel, gearBuf);
  lv_obj_set_style_text_font(gearLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(gearLabel, COL_TEXT, 0);
  lv_obj_align(gearLabel, LV_ALIGN_LEFT_MID, 140, 0);

  lv_obj_t* rpmRangeTitleLbl = lv_label_create(infoRow);
  lv_label_set_text(rpmRangeTitleLbl, "RPM RANGE");
  lv_obj_set_style_text_font(rpmRangeTitleLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(rpmRangeTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(rpmRangeTitleLbl, LV_ALIGN_LEFT_MID, 280, 0);

  lv_obj_t* rpmRangeVal = lv_label_create(infoRow);
  char rpmBuf[24];
  snprintf(rpmBuf, sizeof(rpmBuf), "%.2f - %.1f", MIN_RPM, MAX_RPM);
  lv_label_set_text(rpmRangeVal, rpmBuf);
  lv_obj_set_style_text_font(rpmRangeVal, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmRangeVal, COL_TEXT, 0);
  lv_obj_align(rpmRangeVal, LV_ALIGN_LEFT_MID, 400, 0);

  y += 52;

  lv_obj_t* invertRow = lv_obj_create(screen);
  lv_obj_set_size(invertRow, CONTENT_W, 48);
  lv_obj_set_pos(invertRow, PX, y);
  lv_obj_set_style_bg_color(invertRow, COL_BG, 0);
  lv_obj_set_style_border_width(invertRow, 0, 0);
  lv_obj_set_style_radius(invertRow, 0, 0);
  lv_obj_set_style_pad_all(invertRow, 0, 0);
  lv_obj_remove_flag(invertRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(invertRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* invertTitleLbl = lv_label_create(invertRow);
  lv_label_set_text(invertTitleLbl, "INVERT DIRECTION");
  lv_obj_set_style_text_font(invertTitleLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(invertTitleLbl, COL_TEXT, 0);
  lv_obj_align(invertTitleLbl, LV_ALIGN_LEFT_MID, 8, 0);

  invertToggle = lv_obj_create(invertRow);
  lv_obj_set_size(invertToggle, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_align(invertToggle, LV_ALIGN_RIGHT_MID, -8, 0);
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

  y += 56;

  lv_obj_t* idleRow = lv_obj_create(screen);
  lv_obj_set_size(idleRow, CONTENT_W, 48);
  lv_obj_set_pos(idleRow, PX, y);
  lv_obj_set_style_bg_color(idleRow, COL_BG, 0);
  lv_obj_set_style_border_width(idleRow, 0, 0);
  lv_obj_set_style_radius(idleRow, 0, 0);
  lv_obj_set_style_pad_all(idleRow, 0, 0);
  lv_obj_remove_flag(idleRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(idleRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* idleTitleLbl = lv_label_create(idleRow);
  lv_label_set_text(idleTitleLbl, "DIR SWITCH");
  lv_obj_set_style_text_font(idleTitleLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(idleTitleLbl, COL_TEXT, 0);
  lv_obj_align(idleTitleLbl, LV_ALIGN_LEFT_MID, 8, 0);

  idleToggle = lv_obj_create(idleRow);
  lv_obj_set_size(idleToggle, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_align(idleToggle, LV_ALIGN_RIGHT_MID, -8, 0);
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

  y += 56;

  lv_obj_t* warnBar = lv_obj_create(screen);
  lv_obj_set_size(warnBar, CONTENT_W, 28);
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
  lv_label_set_text(warnLbl, "! Stop motor before changing microstep");
  lv_obj_set_style_text_font(warnLbl, FONT_MED, 0);
  lv_obj_set_style_text_color(warnLbl, COL_RED, 0);
  lv_obj_align(warnLbl, LV_ALIGN_LEFT_MID, 8, 0);

  y += 32;

  lv_obj_t* statusRow = lv_obj_create(screen);
  lv_obj_set_size(statusRow, CONTENT_W, 28);
  lv_obj_set_pos(statusRow, PX, y);
  lv_obj_set_style_bg_color(statusRow, COL_BG_DIM, 0);
  lv_obj_set_style_border_width(statusRow, 0, 0);
  lv_obj_set_style_radius(statusRow, 0, 0);
  lv_obj_set_style_pad_all(statusRow, 0, 0);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_CLICKABLE);

  statusLabel = lv_label_create(statusRow);
  lv_label_set_text(statusLabel, "MOTOR: IDLE");
  lv_obj_set_style_text_font(statusLabel, FONT_MED, 0);
  lv_obj_set_style_text_color(statusLabel, COL_TEXT_DIM, 0);
  lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 8, 0);

  saveFeedbackLabel = lv_label_create(statusRow);
  lv_label_set_text(saveFeedbackLabel, "");
  lv_obj_set_style_text_font(saveFeedbackLabel, FONT_MED, 0);
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
