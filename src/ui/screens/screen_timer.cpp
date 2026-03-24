// TIG Rotator Controller - Timer Mode Screen
// Matching ui_all_screens.svg: Large timer display + presets + START

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t timerSeconds = 30;
static lv_obj_t* timerLabel = nullptr;
static lv_obj_t* speedValLabel = nullptr;
static lv_obj_t* startBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void preset_event_cb(lv_event_t* e) {
  timerSeconds = (uint32_t)(intptr_t)lv_event_get_user_data(e);
  uint32_t min = timerSeconds / 60;
  uint32_t sec = timerSeconds % 60;
  lv_label_set_text_fmt(timerLabel, "%02d:%02d", (int)min, (int)sec);
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE && timerSeconds > 0) {
    control_start_timer(timerSeconds);
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "■ STOP");
    lv_obj_set_style_border_color(startBtn, COL_RED, 0);
  } else if (state == STATE_TIMER) {
    control_stop();
    lv_obj_t* label = lv_obj_get_child(startBtn, 0);
    lv_label_set_text(label, "▶ START");
    lv_obj_set_style_border_color(startBtn, COL_GREEN, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Large timer, preset row, speed display, start button
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Back button (top-left) ──
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 80, 40);
  lv_obj_set_pos(backBtn, 16, 12);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "TIMER MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // ── Timer display panel (SVG: large "00:30" in cyan box) ──
  lv_obj_t* timerPanel = lv_obj_create(screen);
  lv_obj_set_size(timerPanel, 550, 140);
  lv_obj_align(timerPanel, LV_ALIGN_TOP_MID, 0, 45);
  lv_obj_set_style_bg_color(timerPanel, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_radius(timerPanel, 8, 0);
  lv_obj_set_style_border_width(timerPanel, 1, 0);
  lv_obj_set_style_border_color(timerPanel, COL_ACCENT, 0);

  timerLabel = lv_label_create(timerPanel);
  lv_label_set_text(timerLabel, "00:30");
  lv_obj_set_style_text_font(timerLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timerLabel, COL_ACCENT, 0);
  lv_obj_center(timerLabel);

  // ── Duration preset buttons ──
  const uint32_t presets[] = {10, 30, 60};
  const char* presetLabels[] = {"10s", "30s", "60s"};

  for (int i = 0; i < 3; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 155, 65);
    lv_obj_set_pos(btn, 55 + i * 170, 210);
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_radius(btn, 6, 0);

    if (presets[i] == 30) {
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    } else {
      lv_obj_set_style_border_width(btn, 1, 0);
      lv_obj_set_style_border_color(btn, COL_BORDER_SPD, 0);
    }

    lv_obj_add_event_cb(btn, preset_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)presets[i]);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, presetLabels[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, (presets[i] == 30) ? COL_ACCENT : COL_TEXT_DIM, 0);
    lv_obj_center(lbl);
  }

  // CUSTOM button
  lv_obj_t* customBtn = lv_btn_create(screen);
  lv_obj_set_size(customBtn, 155, 65);
  lv_obj_set_pos(customBtn, 55 + 3 * 170, 210);
  lv_obj_set_style_bg_color(customBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(customBtn, 6, 0);
  lv_obj_set_style_border_width(customBtn, 1, 0);
  lv_obj_set_style_border_color(customBtn, COL_BORDER_SPD, 0);

  lv_obj_t* customLabel = lv_label_create(customBtn);
  lv_label_set_text(customLabel, "CUSTOM");
  lv_obj_set_style_text_color(customLabel, COL_TEXT_DIM, 0);
  lv_obj_center(customLabel);

  // ── Speed display ──
  lv_obj_t* speedLabel = lv_label_create(screen);
  lv_label_set_text(speedLabel, "SPEED:");
  lv_obj_set_style_text_font(speedLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(speedLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(speedLabel, 55, 310);

  speedValLabel = lv_label_create(screen);
  lv_label_set_text(speedValLabel, "2.0 RPM");
  lv_obj_set_style_text_font(speedValLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(speedValLabel, COL_TEXT, 0);
  lv_obj_set_pos(speedValLabel, 140, 306);

  // ── START button ──
  startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, 230, 65);
  lv_obj_set_pos(startBtn, 520, 295);
  lv_obj_set_style_bg_color(startBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(startBtn, 8, 0);
  lv_obj_set_style_border_width(startBtn, 2, 0);
  lv_obj_set_style_border_color(startBtn, COL_GREEN, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "▶ START");
  lv_obj_set_style_text_font(startLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(startLabel, COL_GREEN, 0);
  lv_obj_center(startLabel);

  LOG_I("Screen timer: SVG-matched layout created");
}

void screen_timer_update() {
  if (!screens_is_active(SCREEN_TIMER)) return;
  float rpm = speed_get_target_rpm();
  lv_label_set_text_fmt(speedValLabel, "%.1f RPM", rpm);
}
