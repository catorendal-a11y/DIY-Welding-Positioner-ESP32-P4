#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

static uint32_t timerSeconds = 30;
static lv_obj_t* timerLabel = nullptr;
static lv_obj_t* speedValLabel = nullptr;
static lv_obj_t* startBtn = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* presetBtns[4] = {nullptr};

static void back_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_TIMER || state == STATE_RUNNING || state == STATE_PULSE ||
      state == STATE_STEP || state == STATE_JOG || state == STATE_STOPPING) {
    control_stop();
  }
  screens_show(SCREEN_MENU);
}

static void preset_event_cb(lv_event_t* e) {
  int index = (int)(intptr_t)lv_event_get_user_data(e);
  const uint32_t presets[] = {10, 30, 60};
  timerSeconds = presets[index];
  uint32_t min = timerSeconds / 60;
  uint32_t sec = timerSeconds % 60;
  lv_label_set_text_fmt(timerLabel, "%02d:%02d", (int)min, (int)sec);

  for (int i = 0; i < 3; i++) {
    if (!presetBtns[i]) continue;
    lv_obj_t* lbl = lv_obj_get_child(presetBtns[i], 0);
    if (i == index) {
      lv_obj_set_style_bg_color(presetBtns[i], COL_BTN_ACTIVE, 0);
      lv_obj_set_style_border_width(presetBtns[i], 3, 0);
      lv_obj_set_style_border_color(presetBtns[i], COL_ACCENT, 0);
      lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
    } else {
      lv_obj_set_style_bg_color(presetBtns[i], COL_BTN_FILL, 0);
      lv_obj_set_style_border_width(presetBtns[i], 2, 0);
      lv_obj_set_style_border_color(presetBtns[i], COL_BORDER, 0);
      lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    }
  }
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE && timerSeconds > 0) {
    control_start_timer(timerSeconds);
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "\xE2\x96\xA0 STOP");
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
    lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
    lv_label_set_text(statusLabel, "\xE2\x97\x8F RUNNING");
    lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
  } else if (state == STATE_TIMER) {
    control_stop();
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "\xE2\x96\xB6 START");
    lv_obj_set_style_text_color(label, COL_ACCENT, 0);
    lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
    lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
    lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
  }
}

void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, SW(50), SH(22));
  lv_obj_set_pos(backBtn, SX(16), SY(21));
  lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_set_style_radius(backBtn, 0, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_font(backLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "TIMER MODE");
  lv_obj_set_style_text_font(title, FONT_BODY, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SY(21));

  statusLabel = lv_label_create(header);
  lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
  lv_obj_set_style_text_font(statusLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_RIGHT, -SX(16), SY(21));

  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep, 0, HEADER_H);
  lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);

  lv_obj_t* timerPanel = lv_obj_create(screen);
  lv_obj_set_size(timerPanel, SW(280), SH(70));
  lv_obj_set_pos(timerPanel, SX(40), SY(44));
  lv_obj_set_style_bg_color(timerPanel, COL_PANEL_BG, 0);
  lv_obj_set_style_border_width(timerPanel, 2, 0);
  lv_obj_set_style_border_color(timerPanel, COL_ACCENT, 0);
  lv_obj_set_style_radius(timerPanel, RADIUS_MD, 0);
  lv_obj_set_style_pad_all(timerPanel, 0, 0);

  timerLabel = lv_label_create(timerPanel);
  lv_label_set_text(timerLabel, "00:30");
  lv_obj_set_style_text_font(timerLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(timerLabel, COL_ACCENT, 0);
  lv_obj_center(timerLabel);

  const char* presetLabels[] = {"10s", "30s", "60s", "CUSTOM"};
  const int presetX[] = {SX(16), SX(100), SX(184), SX(268)};

  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    presetBtns[i] = btn;
    lv_obj_set_size(btn, SW(78), SH(34));
    lv_obj_set_pos(btn, presetX[i], SY(124));
    lv_obj_set_style_radius(btn, RADIUS_SM, 0);

    if (i == 1) {
      lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
      lv_obj_set_style_border_width(btn, 3, 0);
      lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    } else {
      lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    }

    if (i < 3) {
      lv_obj_add_event_cb(btn, preset_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    } else {
      lv_obj_add_state(btn, LV_STATE_DISABLED);
    }

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, presetLabels[i]);
    lv_obj_set_style_text_color(lbl, (i == 1) ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(lbl);
  }

  lv_obj_t* sep2 = lv_obj_create(screen);
  lv_obj_set_size(sep2, SCREEN_W, SEP_H);
  lv_obj_set_pos(sep2, 0, SY(168));
  lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep2, 0, 0);
  lv_obj_set_style_pad_all(sep2, 0, 0);
  lv_obj_set_style_radius(sep2, 0, 0);

  lv_obj_t* speedLabel = lv_label_create(screen);
  lv_label_set_text(speedLabel, "SPEED:");
  lv_obj_set_style_text_font(speedLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(speedLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(speedLabel, SX(16), SY(194));

  speedValLabel = lv_label_create(screen);
  lv_label_set_text(speedValLabel, "2.0 RPM");
  lv_obj_set_style_text_font(speedValLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(speedValLabel, COL_TEXT, 0);
  lv_obj_set_pos(speedValLabel, SX(68), SY(194));

  startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, SW(156), SH(36));
  lv_obj_set_pos(startBtn, SX(188), SY(178));
  lv_obj_set_style_bg_color(startBtn, COL_BTN_ACTIVE, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_SM, 0);
  lv_obj_set_style_border_width(startBtn, 3, 0);
  lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "\xE2\x96\xB6 START");
  lv_obj_set_style_text_font(startLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(startLabel, COL_ACCENT, 0);
  lv_obj_center(startLabel);

  LOG_I("Screen timer: SVG-matched layout created");
}

void screen_timer_update() {
  if (!screens_is_active(SCREEN_TIMER)) return;
  float rpm = speed_get_target_rpm();
  char buf[32];
  int whole = (int)rpm;
  int frac = (int)((rpm - whole) * 10);
  if (frac < 0) frac = 0;
  if (frac > 9) frac = 9;
  snprintf(buf, sizeof(buf), "%d.%d RPM", whole, frac);
  lv_label_set_text(speedValLabel, buf);

  SystemState state = control_get_state();
  if (startBtn) {
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    if (state == STATE_TIMER) {
      if (label) lv_label_set_text(label, "\xE2\x96\xA0 STOP");
      if (label) lv_obj_set_style_text_color(label, COL_TEXT, 0);
      lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
      if (statusLabel) {
        lv_label_set_text(statusLabel, "\xE2\x97\x8F RUNNING");
        lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
      }
    } else {
      if (label) lv_label_set_text(label, "\xE2\x96\xB6 START");
      if (label) lv_obj_set_style_text_color(label, COL_ACCENT, 0);
      lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
      if (statusLabel) {
        lv_label_set_text(statusLabel, "\xE2\x97\x8F READY");
        lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
      }
    }
  }
}
