// V5 operating panel. Values are calculated from pulses, not encoder feedback.
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../motor/motor.h"
#include "../../control/control.h"
#include "../../safety/safety.h"
#include <cmath>
LV_FONT_DECLARE(rotator_digits_104);

static lv_obj_t *rpmLabel, *speedCaption, *stateLabel, *sourceLabel, *surfaceLabel;
static lv_obj_t *diameterLabel, *limitLabel, *detailLabel, *speedBar, *modeTitle;
static lv_obj_t *startBtn, *stopBtn, *menuBtn, *minusBtn, *plusBtn, *cwBtn, *ccwBtn;
static lv_obj_t* minimumLabel = nullptr;
static lv_color_t ink() { return lv_color_hex(0x11191C); }
static bool can_edit() { return ui_control_fresh() && ui_control_state() == STATE_IDLE && !safety_inhibit_motion(); }
static void start_cb(lv_event_t*) {
  if (can_edit()) control_start_continuous();
}
static void stop_cb(lv_event_t*) { control_stop(); }
static void menu_cb(lv_event_t*) {
  if (control_get_state() == STATE_IDLE) screens_show(SCREEN_MENU);
}
static void adjust_cb(lv_event_t* e) {
  if (can_edit()) speed_slider_set(speed_get_target_rpm() + (int)(intptr_t)lv_event_get_user_data(e) * 0.01f);
}
static void direction_cb(lv_event_t* e) {
  if (!can_edit() || g_dir_switch_cache.load()) return;
  Direction dir = (Direction)(uintptr_t)lv_event_get_user_data(e);
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  bool invert = g_settings.invert_direction;
  xSemaphoreGive(g_settings_mutex);
  if (invert) dir = dir == DIR_CW ? DIR_CCW : DIR_CW;
  speed_set_direction(dir);
}
static void enabled(lv_obj_t* o, bool yes) {
  if (yes)
    lv_obj_set_disabled(o, false);
  else
    lv_obj_set_disabled(o, true);
}
void screen_main_create() {
  lv_obj_t* s = screenRoots[SCREEN_MAIN];
  lv_obj_clean(s);
  lv_obj_t* h = ui_create_header(s, "Continuous rotation", "SYSTEM READY", &stateLabel);
  modeTitle = lv_obj_get_child(h, 1);
  lv_obj_t* hero = ui_create_post_card(s, 24, 96, 460, 278);
  lv_obj_set_style_bg_color(hero, lv_color_hex(0xFF6B38), 0);
  lv_obj_set_style_border_width(hero, 0, 0);
  speedCaption = ui_create_text(hero, 22, 18, 410, "01 / TARGET SPEED", FONT_NORMAL, ink());
  rpmLabel = ui_create_text(hero, 18, 62, 340, "0.50", &rotator_digits_104, ink());
  lv_obj_set_style_transform_pivot_x(rpmLabel, 0, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, 0, 0);

  ui_create_text(hero, 360, 130, 88, "RPM", FONT_XL, ink());
  speedBar = lv_bar_create(hero);
  lv_obj_set_pos(speedBar, 22, 193);
  lv_obj_set_size(speedBar, 416, 6);
  lv_bar_set_range(speedBar, 0, 1000);
  lv_obj_set_style_bg_color(speedBar, lv_color_hex(0xC64E2B), LV_PART_MAIN);
  lv_obj_set_style_bg_color(speedBar, ink(), LV_PART_INDICATOR);
  for (int i = 0; i < 43; i++) {
    lv_obj_t* tick = lv_obj_create(hero);
    lv_obj_remove_style_all(tick);
    lv_obj_set_pos(tick, 22 + i * 9, 205);
    lv_obj_set_size(tick, 2, i % 5 == 0 ? 15 : 9);
    lv_obj_set_style_bg_color(tick, lv_color_hex(0x66301E), 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
  }
  minimumLabel = ui_create_text(hero, 22, 244, 140, "", FONT_NORMAL, ink());
  limitLabel = ui_create_text(hero, 205, 244, 233, "", FONT_NORMAL, ink());
  lv_obj_set_style_text_align(limitLabel, LV_TEXT_ALIGN_RIGHT, 0);
  cwBtn = ui_create_btn(s, 504, 96, 132, 74, "CW", FONT_XL, UI_BTN_NORMAL, direction_cb,
                        (void*)(uintptr_t)DIR_CW);
  ccwBtn = ui_create_btn(s, 644, 96, 132, 74, "CCW", FONT_XL, UI_BTN_NORMAL, direction_cb,
                         (void*)(uintptr_t)DIR_CCW);
  ui_create_text(s, 504, 192, 272, "SURFACE SPEED / CALC.", FONT_NORMAL, COL_TEXT_DIM);
  surfaceLabel = ui_create_text(s, 504, 219, 272, "", FONT_XXL, COL_TEXT);
  diameterLabel = ui_create_text(s, 504, 256, 272, "", FONT_NORMAL, COL_TEXT_DIM);
  ui_create_separator_line(s, 504, 280, 272, COL_BORDER);
  ui_create_text(s, 504, 294, 80, "SOURCE", FONT_NORMAL, COL_TEXT_DIM);
  sourceLabel = ui_create_text(s, 586, 292, 190, "", FONT_SUBTITLE, COL_TEXT);
  lv_obj_set_style_text_align(sourceLabel, LV_TEXT_ALIGN_RIGHT, 0);
  detailLabel = ui_create_text(s, 504, 330, 272, "Manual rotation", FONT_SUBTITLE, COL_TEXT);
  lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_MODE_WRAP);
  menuBtn = ui_create_btn(s, 24, 408, 72, 56, "MENU", FONT_NORMAL, UI_BTN_NORMAL, menu_cb, nullptr);
  minusBtn = ui_create_btn(s, 108, 408, 94, 56, "-", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)(intptr_t)-1);
  plusBtn = ui_create_btn(s, 214, 408, 94, 56, "+", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)(intptr_t)1);
  startBtn = ui_create_btn(s, 328, 408, 448, 56, "START ROTATION  " LV_SYMBOL_PLAY, FONT_LARGE, UI_BTN_NORMAL,
                           start_cb, nullptr);
  lv_obj_set_style_bg_color(startBtn, ink(), 0);
  lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
  stopBtn = ui_create_btn(s, 328, 408, 448, 56, LV_SYMBOL_STOP "  STOP ROTATION", FONT_LARGE, UI_BTN_DANGER,
                          stop_cb, nullptr);
  lv_obj_set_user_data(startBtn, (void*)(uintptr_t)UI_ACTION_START);
  lv_obj_set_user_data(stopBtn, (void*)(uintptr_t)UI_ACTION_STOP);
  lv_obj_set_hidden(stopBtn, true);
}
void screen_main_update() {
  ui_mark_motion_callback(screenRoots[SCREEN_MAIN], start_cb);
  if (!screens_is_active(SCREEN_MAIN) || !rpmLabel) return;
  const auto& view = ui_control_view();
  const SystemState st = ui_control_state();
  const bool moving = st != STATE_IDLE && st != STATE_ESTOP;
  const bool rangeBlocked = !motor_milli_hz_for_rpm_calibrated(view.target_rpm);
  const bool blocked = rangeBlocked || !ui_control_fresh() || safety_inhibit_motion() || safety_is_estop_locked() || st == STATE_ESTOP;
  float rpm = st == STATE_ESTOP ? 0.0f : moving ? view.estimated_rpm : view.target_rpm;
  float diameter = speed_get_workpiece_diameter_mm();
  if (diameter <= 0) diameter = D_EMNE * 1000.0f;
  char rpmText[16];
  ui_format_rpm(rpmText, sizeof(rpmText), rpm);
  lv_label_set_text(rpmLabel, rpmText);
  lv_label_set_text(speedCaption, moving ? "01 / ESTIMATED SPEED" : "01 / TARGET SPEED");
  lv_label_set_text_fmt(minimumLabel, "MIN %.3f", (double)speed_get_rpm_min());
  lv_label_set_text_fmt(limitLabel, "MAX %.3f RPM", (double)speed_get_rpm_max());
  lv_bar_set_value(speedBar, (int)(1000.0f * rpm / speed_get_rpm_max()), LV_ANIM_OFF);
  lv_label_set_text_fmt(surfaceLabel, "%.0f mm/min", (double)(rpm * diameter * 3.14159265f));
  lv_label_set_text_fmt(diameterLabel, "WORKPIECE DIA %.0f mm", (double)diameter);
  bool cw = view.direction == DIR_CW;
  for (lv_obj_t* b : {cwBtn, ccwBtn}) {
    const bool chosen = (b == cwBtn) == cw;
    lv_obj_set_style_bg_color(b, chosen ? ink() : COL_BTN_BG, 0);
    lv_obj_set_style_border_color(b, chosen ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_border_width(b, chosen ? 2 : 0, 0);
    enabled(b, !moving && !blocked && !g_dir_switch_cache.load());
    lv_obj_set_style_bg_color(b, chosen ? ink() : COL_BTN_BG, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), chosen ? COL_TEXT : COL_TEXT_DIM, 0);
    lv_obj_set_user_data(b, chosen ? nullptr : (void*)(uintptr_t)UI_ACTION_DIRECTION);
  }
  lv_label_set_text(modeTitle, st == STATE_PULSE      ? "Pulse rotation"
                               : st == STATE_STEP     ? "Angle move"
                               : st == STATE_JOG      ? "Jog"
                               : st == STATE_STOPPING ? "Stopping"
                                                      : "Continuous rotation");
  enabled(menuBtn, !moving);
  enabled(minusBtn, !moving && !blocked);
  enabled(plusBtn, !moving && !blocked);
  SpeedInputSource source = view.source;
  lv_label_set_text(sourceLabel, st == STATE_JOG                ? "Jog setting"
                                 : st == STATE_STEP             ? "Step setting"
                                 : source == SPEED_SOURCE_PEDAL ? "Pedal analog"
                                 : source == SPEED_SOURCE_UI    ? "Screen / program"
                                                                : "Panel dial");
  lv_label_set_text(stateLabel, !ui_control_fresh() ? "STATUS UNAVAILABLE" : blocked ? "MOTION LOCKED"
                                : moving ? control_state_name(st)
                                         : "SYSTEM READY");
  lv_obj_set_style_text_color(stateLabel, blocked ? COL_RED : moving ? COL_ACCENT : COL_GREEN, 0);
  lv_label_set_text(detailLabel, rangeBlocked                ? "Speed outside motor range"
                                 : blocked                   ? "Clear fault before restart"
                                 : moving                    ? "STOP ends motion"
                                 : speed_get_pedal_enabled() ? "Pedal control enabled"
                                                             : "Manual rotation");
  if (storage_status() != STORAGE_SAVED)
    lv_label_set_text(
        detailLabel, storage_status() == STORAGE_ERROR ? "SAVE FAILED / retry pending" : "Saving changes...");
  if (moving || !ui_control_fresh()) {
    lv_obj_set_hidden(startBtn, true);
    lv_obj_set_hidden(stopBtn, false);
  } else {
    lv_obj_set_hidden(startBtn, false);
    lv_obj_set_hidden(stopBtn, true);
  }
  enabled(startBtn, !blocked);
  lv_label_set_text(lv_obj_get_child(startBtn, 0),
                    blocked ? "START BLOCKED" : "START ROTATION  " LV_SYMBOL_PLAY);
}
void screen_main_invalidate_widgets() {
  minimumLabel = nullptr;
  rpmLabel = speedCaption = stateLabel = sourceLabel = surfaceLabel = diameterLabel = limitLabel =
      detailLabel = speedBar = modeTitle = nullptr;
  startBtn = stopBtn = menuBtn = minusBtn = plusBtn = cwBtn = ccwBtn = nullptr;
}
