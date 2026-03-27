#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static float currentAngle = 90.0f;
static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* stepBtn = nullptr;
static lv_obj_t* stopBtn = nullptr;
static lv_obj_t* presetBtns[5] = {nullptr};
static int activePresetIndex = 1;

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void preset_event_cb(lv_event_t* e) {
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  activePresetIndex = index;

  const float angles[] = {45.0f, 90.0f, 180.0f, 360.0f};
  if (index < 4) {
    currentAngle = angles[index];
    lv_label_set_text_fmt(angleLabel, "%d\xC2\xB0", (int)currentAngle);
  }

  for (int i = 0; i < 5; i++) {
    if (!presetBtns[i]) continue;
    lv_obj_t* lbl = lv_obj_get_child(presetBtns[i], 0);
    if (i == index) {
      lv_obj_set_style_border_width(presetBtns[i], 3, 0);
      lv_obj_set_style_border_color(presetBtns[i], COL_ACCENT, 0);
      lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
    } else {
      lv_obj_set_style_border_width(presetBtns[i], 2, 0);
      lv_obj_set_style_border_color(presetBtns[i], COL_BORDER, 0);
      lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    }
  }
}

static void rpm_minus_cb(lv_event_t* e) {
  float rpm = speed_get_target_rpm();
  if (rpm > MIN_RPM) speed_slider_set(rpm - 0.5f);
}

static void rpm_plus_cb(lv_event_t* e) {
  float rpm = speed_get_target_rpm();
  if (rpm < MAX_RPM) speed_slider_set(rpm + 0.5f);
}

static void step_event_cb(lv_event_t* e) {
  SystemState s = control_get_state();
  if (s != STATE_IDLE && s != STATE_STEP) return;
  control_start_step(currentAngle);
}

static void stop_event_cb(lv_event_t* e) {
  control_stop();
}

void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, SW(50), SH(22));
  lv_obj_set_pos(backBtn, SX(16), SY(21));
  lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_set_style_radius(backBtn, 0, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLbl = lv_label_create(backBtn);
  lv_label_set_text(backLbl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_font(backLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(backLbl, COL_TEXT_DIM, 0);
  lv_obj_center(backLbl);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "STEP MODE");
  lv_obj_set_style_text_font(title, FONT_BODY, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SY(21));

  lv_obj_t* status = lv_label_create(header);
  lv_label_set_text(status, "● READY");
  lv_obj_set_style_text_font(status, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(status, COL_ACCENT, 0);
  lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -SX(16), SY(21));

  lv_obj_t* sep1 = lv_obj_create(screen);
  lv_obj_set_size(sep1, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep1, 0, HEADER_H);
  lv_obj_set_style_bg_color(sep1, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep1, 0, 0);
  lv_obj_set_style_pad_all(sep1, 0, 0);
  lv_obj_clear_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* anglePanel = lv_obj_create(screen);
  lv_obj_set_size(anglePanel, SW(150), SH(60));
  lv_obj_set_pos(anglePanel, SX(16), SY(40));
  lv_obj_set_style_bg_color(anglePanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(anglePanel, RADIUS_MD, 0);
  lv_obj_set_style_border_width(anglePanel, 2, 0);
  lv_obj_set_style_border_color(anglePanel, COL_ACCENT, 0);
  lv_obj_set_style_pad_all(anglePanel, 0, 0);
  lv_obj_clear_flag(anglePanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* angleTitle = lv_label_create(anglePanel);
  lv_label_set_text(angleTitle, "TARGET ANGLE");
  lv_obj_set_style_text_font(angleTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(angleTitle, COL_TEXT_DIM, 0);
  lv_obj_align(angleTitle, LV_ALIGN_TOP_LEFT, SX(10), SY(5));

  angleLabel = lv_label_create(anglePanel);
  lv_label_set_text(angleLabel, "90°");
  lv_obj_set_style_text_font(angleLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(angleLabel, COL_ACCENT, 0);
  lv_obj_align(angleLabel, LV_ALIGN_BOTTOM_LEFT, SX(10), -SY(5));

  lv_obj_t* rpmPanel = lv_obj_create(screen);
  lv_obj_set_size(rpmPanel, SW(166), SH(60));
  lv_obj_set_pos(rpmPanel, SX(178), SY(40));
  lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_radius(rpmPanel, RADIUS_MD, 0);
  lv_obj_set_style_border_width(rpmPanel, 2, 0);
  lv_obj_set_style_border_color(rpmPanel, COL_BORDER, 0);
  lv_obj_set_style_pad_all(rpmPanel, 0, 0);
  lv_obj_clear_flag(rpmPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rpmTitle = lv_label_create(rpmPanel);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_align(rpmTitle, LV_ALIGN_TOP_LEFT, SX(10), SY(5));

  rpmLabel = lv_label_create(rpmPanel);
  lv_label_set_text(rpmLabel, "2.0");
  lv_obj_set_style_text_font(rpmLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_BOTTOM_LEFT, SX(10), -SY(5));

  lv_obj_t* presetsLabel = lv_label_create(screen);
  lv_label_set_text(presetsLabel, "PRESETS");
  lv_obj_set_style_text_font(presetsLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(presetsLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(presetsLabel, SX(16), SY(118));

  const char* presetNames[] = {"45°", "90°", "180°", "360°", "CUSTOM"};
  const int presetX[] = {16, 82, 148, 214, 280};

  for (int i = 0; i < 5; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    presetBtns[i] = btn;
    lv_obj_set_size(btn, SW(60), SH(28));
    lv_obj_set_pos(btn, SX(presetX[i]), SY(124));
    lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
    lv_obj_set_style_radius(btn, RADIUS_SM, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_add_event_cb(btn, preset_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, presetNames[i]);
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_center(lbl);
  }

  lv_obj_set_style_border_width(presetBtns[1], 3, 0);
  lv_obj_set_style_border_color(presetBtns[1], COL_ACCENT, 0);
  lv_obj_set_style_text_color(lv_obj_get_child(presetBtns[1], 0), COL_ACCENT, 0);

  lv_obj_t* sep2 = lv_obj_create(screen);
  lv_obj_set_size(sep2, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep2, 0, SY(160));
  lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep2, 0, 0);
  lv_obj_set_style_pad_all(sep2, 0, 0);
  lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rpmMinusBtn = lv_btn_create(screen);
  lv_obj_set_size(rpmMinusBtn, SW(76), SH(32));
  lv_obj_set_pos(rpmMinusBtn, SX(16), SY(168));
  lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(rpmMinusBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(rpmMinusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_minus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* rpmMinusLbl = lv_label_create(rpmMinusBtn);
  lv_label_set_text(rpmMinusLbl, "RPM " LV_SYMBOL_DOWN);
  lv_obj_set_style_text_color(rpmMinusLbl, COL_TEXT, 0);
  lv_obj_center(rpmMinusLbl);

  lv_obj_t* rpmPlusBtn = lv_btn_create(screen);
  lv_obj_set_size(rpmPlusBtn, SW(76), SH(32));
  lv_obj_set_pos(rpmPlusBtn, SX(100), SY(168));
  lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(rpmPlusBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(rpmPlusBtn, 2, 0);
  lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_plus_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* rpmPlusLbl = lv_label_create(rpmPlusBtn);
  lv_label_set_text(rpmPlusLbl, "RPM " LV_SYMBOL_UP);
  lv_obj_set_style_text_color(rpmPlusLbl, COL_TEXT, 0);
  lv_obj_center(rpmPlusLbl);

  stepBtn = lv_btn_create(screen);
  lv_obj_set_size(stepBtn, SW(96), SH(32));
  lv_obj_set_pos(stepBtn, SX(184), SY(168));
  lv_obj_set_style_bg_color(stepBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(stepBtn, RADIUS_MD, 0);
  lv_obj_set_style_border_width(stepBtn, 3, 0);
  lv_obj_set_style_border_color(stepBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(stepBtn, step_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stepLbl = lv_label_create(stepBtn);
  lv_label_set_text(stepLbl, "▶ STEP");
  lv_obj_set_style_text_font(stepLbl, FONT_BODY, 0);
  lv_obj_set_style_text_color(stepLbl, COL_ACCENT, 0);
  lv_obj_center(stepLbl);

  stopBtn = lv_btn_create(screen);
  lv_obj_set_size(stopBtn, SW(52), SH(32));
  lv_obj_set_pos(stopBtn, SX(288), SY(168));
  lv_obj_set_style_bg_color(stopBtn, COL_BTN_FILL, 0);
  lv_obj_set_style_radius(stopBtn, RADIUS_MD, 0);
  lv_obj_set_style_border_width(stopBtn, 3, 0);
  lv_obj_set_style_border_color(stopBtn, COL_RED, 0);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stopLbl = lv_label_create(stopBtn);
  lv_label_set_text(stopLbl, "■ STOP");
  lv_obj_set_style_text_font(stopLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stopLbl, COL_RED, 0);
  lv_obj_center(stopLbl);

  lv_obj_t* hint = lv_label_create(screen);
  lv_label_set_text(hint, "Select preset → Set RPM → Press STEP");
  lv_obj_set_style_text_font(hint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(hint, COL_TEXT_DIM, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, SY(220));
}

void screen_step_update() {
  if (!screens_is_active(SCREEN_STEP)) return;

  float rpm = speed_get_target_rpm();
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", rpm);
  lv_label_set_text(rpmLabel, buf);
}
