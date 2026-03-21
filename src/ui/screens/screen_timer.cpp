// TIG Rotator Controller - Timer Mode Screen (T-24)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t timerMin = 0;
static uint32_t timerSec = 30;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE) {
    uint32_t duration = timerMin * 60 + timerSec;
    if (duration > 0) {
      control_start_timer(duration);
    }
  } else if (state == STATE_TIMER) {
    control_stop();
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];

  // Header
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, STATUS_BAR_H);
  lv_obj_set_style_bg_color(backBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_set_style_text_color(backLabel, COL_ACCENT, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "⏱ TIMER MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // Duration input panel
  lv_obj_t* durPanel = lv_obj_create(screen);
  lv_obj_set_size(durPanel, 456, 100);
  lv_obj_set_pos(durPanel, 12, 44);
  lv_obj_set_style_bg_color(durPanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(durPanel, RADIUS_CARD, 0);

  lv_obj_t* durLabel = lv_label_create(durPanel);
  lv_label_set_text(durLabel, "DURATION");
  lv_obj_set_style_text_color(durLabel, COL_TEXT_LABEL, 0);
  lv_obj_align(durLabel, LV_ALIGN_TOP_MID, 0, 8);

  // Time display - "00 : 30"
  lv_obj_t* timeLabel = lv_label_create(screen);
  lv_label_set_text(timeLabel, "00 : 30");
  lv_obj_set_style_text_font(timeLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(timeLabel, COL_TEXT, 0);
  lv_obj_align(timeLabel, LV_ALIGN_CENTER, 0, 20);

  // Progress bar
  lv_obj_t* bar = lv_bar_create(screen);
  lv_obj_set_size(bar, 432, 14);
  lv_obj_align(bar, LV_ALIGN_CENTER, 0, 60);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, COL_BG_INPUT, 0);
  lv_obj_set_style_bg_color(bar, COL_ACCENT, LV_PART_INDICATOR);

  // START/STOP buttons
  lv_obj_t* startBtn = lv_btn_create(screen);
  lv_obj_set_size(startBtn, 220, 52);
  lv_obj_set_pos(startBtn, 12, 264);
  lv_obj_set_style_bg_color(startBtn, COL_GREEN_DARK, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "▶ START TIMER");
  lv_obj_center(startLabel);

  lv_obj_t* stopBtn = lv_btn_create(screen);
  lv_obj_set_size(stopBtn, 220, 52);
  lv_obj_set_pos(stopBtn, 248, 264);
  lv_obj_set_style_bg_color(stopBtn, COL_RED, 0);
  lv_obj_set_style_radius(stopBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(stopBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "■ STOP");
  lv_obj_center(stopLabel);

  LOG_I("Screen timer: created");
}

void screen_timer_update() {
  // Update countdown and progress bar
  // TODO: Implement full update logic
}
