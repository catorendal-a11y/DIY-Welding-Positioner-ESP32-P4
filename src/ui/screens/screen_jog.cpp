// Jog Mode Screen - POST-style hold-to-run control

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../tabular_font.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static lv_obj_t* cwHoldBtn = nullptr;
static lv_obj_t* ccwHoldBtn = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmBar = nullptr;
static lv_obj_t* jogStopBtn = nullptr;

static void remember_speed() {
  const float rpm = control_get_jog_speed();
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const bool changed = g_settings.jog_rpm != rpm;
  g_settings.jog_rpm = rpm;
  xSemaphoreGive(g_settings_mutex);
  if (changed) storage_save_settings();
}

static void back_event_cb(lv_event_t* e) {
  remember_speed();
  control_stop_jog();
  screens_show(SCREEN_MAIN);
}

static void stop_event_cb(lv_event_t* e) {
  control_stop_jog();
  control_stop();
}

static int rpm_to_pct(float rpm) {
  float mx = speed_get_rpm_max();
  float span = mx - MIN_RPM;
  if (span < 1e-6f) span = 1e-6f;
  int pct = (int)((rpm - MIN_RPM) * 100.0f / span + 0.5f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

static void set_jog_rpm(float rpm) {
  ui_set_rpm_unit(rpmLabel, rpm);
  if (rpmBar) lv_bar_set_value(rpmBar, rpm_to_pct(rpm), LV_ANIM_OFF);
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)lv_event_get_user_data(e);
  float currentRpm = control_get_jog_speed();
  if (delta > 0)
    currentRpm += ui_rpm_increment(currentRpm);
  else
    currentRpm -= ui_rpm_increment(currentRpm);
  float mx = speed_get_rpm_max();
  if (currentRpm < MIN_RPM) currentRpm = MIN_RPM;
  if (currentRpm > mx) currentRpm = mx;
  control_set_jog_speed(currentRpm);
  set_jog_rpm(currentRpm);
}

static void style_hold_btn(lv_obj_t* btn, bool active) {
  if (!btn) return;
  ui_btn_style_post(btn, active ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  lv_obj_set_style_radius(btn, RADIUS_CARD, 0);
  lv_obj_t* title = lv_obj_get_child(btn, 0);
  const lv_color_t color = ui_btn_label_color_post(active ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  if (title) lv_obj_set_style_text_color(title, color, 0);
  lv_obj_t* icon = lv_obj_get_child(btn, 2);
  if (icon) lv_obj_set_style_text_color(icon, color, 0);
}

static void cw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSING) {
    control_renew_jog();
  } else if (code == LV_EVENT_PRESSED) {
    style_hold_btn(cwHoldBtn, control_start_jog_cw());
    style_hold_btn(ccwHoldBtn, false);
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    style_hold_btn(cwHoldBtn, false);
  }
}

static void ccw_hold_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSING) {
    control_renew_jog();
  } else if (code == LV_EVENT_PRESSED) {
    style_hold_btn(cwHoldBtn, false);
    style_hold_btn(ccwHoldBtn, control_start_jog_ccw());
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    control_stop_jog();
    style_hold_btn(ccwHoldBtn, false);
  }
}

static lv_obj_t* create_hold_btn(lv_obj_t* parent, int x, int y, int w, int h, const char* title, Direction direction,
                                 lv_event_cb_t cb) {
  const bool active = false;
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  ui_btn_style_post(btn, active ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  lv_obj_set_style_radius(btn, RADIUS_CARD, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSING, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_PRESS_LOST, nullptr);

  lv_obj_t* titleLbl = lv_label_create(btn);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(titleLbl, ui_btn_label_color_post(active ? UI_BTN_ACCENT : UI_BTN_NORMAL), 0);
  lv_obj_align(titleLbl, LV_ALIGN_CENTER, 0, -18);

  lv_obj_t* hintLbl = lv_label_create(btn);
  lv_label_set_text(hintLbl, "Hold to run");
  lv_obj_set_style_text_font(hintLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(hintLbl, COL_TEXT_VDIM, 0);
  lv_obj_align(hintLbl, LV_ALIGN_CENTER, 0, 28);

  lv_obj_t* icon = ui_create_rotation_icon(btn, 34, ui_btn_label_color_post(UI_BTN_NORMAL), direction);
  lv_obj_align(icon, LV_ALIGN_CENTER, 100, -18);
  return btn;
}

void screen_jog_create() {
  lv_obj_t* screen = screenRoots[SCREEN_JOG];
  lv_obj_clean(screen);
  ui_create_header(screen, "Jog", "", nullptr);
  lv_obj_t* card = ui_create_post_card(screen, 24, 94, 536, 82);
  ui_create_text(card, 16, 10, 272, "Jog speed", FONT_SUBTITLE, COL_TEXT_DIM);
  rpmLabel = ui_create_text(card, 16, 36, 320, "", FONT_XXL, COL_TEXT);
  rpmBar = nullptr;
  ui_create_btn(screen, 576, 94, 92, 82, "-", FONT_XL, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)-1);
  ui_create_btn(screen, 684, 94, 92, 82, "+", FONT_XL, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)1);
  ccwHoldBtn = create_hold_btn(screen, 24, 196, 368, 126, "Hold CCW", DIR_CCW, ccw_hold_event_cb);
  cwHoldBtn = create_hold_btn(screen, 408, 196, 368, 126, "Hold CW", DIR_CW, cw_hold_event_cb);
  ui_create_text(screen, 24, 348, 752, "Motion lasts only while you hold a direction button.", FONT_SUBTITLE,
                 COL_TEXT_DIM);
  ui_create_btn(screen, 24, 408, 152, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  jogStopBtn = ui_create_btn(screen, 192, 408, 584, 56, UI_LABEL_STOP, FONT_LARGE, UI_BTN_DANGER, stop_event_cb, nullptr);
  lv_obj_set_hidden(jogStopBtn, true);
  set_jog_rpm(control_get_jog_speed());
}

void screen_jog_invalidate_widgets() {
  cwHoldBtn = nullptr;
  ccwHoldBtn = nullptr;
  rpmLabel = nullptr;
  rpmBar = nullptr;
  jogStopBtn = nullptr;
}

void screen_jog_update() {
  ui_mark_motion_callback(screenRoots[SCREEN_JOG], cw_hold_event_cb); ui_mark_motion_callback(screenRoots[SCREEN_JOG], ccw_hold_event_cb);
  if (!screens_is_active(SCREEN_JOG)) return;

  SystemState state = ui_control_state();
  lv_obj_set_hidden(jogStopBtn, (state == STATE_IDLE || state == STATE_ESTOP) && ui_control_fresh());
  if (state == STATE_JOG) {
    float actualRpm = ui_control_view().estimated_rpm;
    set_jog_rpm(actualRpm);
  } else {
    set_jog_rpm(control_get_jog_speed());
  }
}
