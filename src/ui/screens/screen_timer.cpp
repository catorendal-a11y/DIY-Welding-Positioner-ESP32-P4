// TIG Rotator Controller - Countdown Screen
// Layout matches POST mockup: ring card left, parameter stack right, 3-button footer

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include "../../config.h"
#include "../../safety/safety.h"
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static int countdownSec = 3;
static std::atomic<bool> countingDown{false};
static std::atomic<bool> startPending{false};
static uint32_t countdownStartMs = 0;
static int lastDisplayedSec = -1;
static int lastIdleCountdownSec = -1;
static int lastArcEndAngle = -1;
static float lastRpmShown = -1.0f;
static uint32_t pulseStartMs = 0;
static std::atomic<bool> backPending{false};

static lv_obj_t* ringCard = nullptr;
static lv_obj_t* arcRing = nullptr;
static lv_obj_t* bigNumberLabel = nullptr;
static lv_obj_t* startAfterValLbl = nullptr;
static lv_obj_t* rpmValLbl = nullptr;
static lv_obj_t* warnDetailLbl = nullptr;

static void set_countdown_arc_angle(int endAngle, lv_color_t color) {
  if (!arcRing) return;
  if (endAngle < 0) endAngle = 0;
  if (endAngle > 360) endAngle = 360;
  if (endAngle != lastArcEndAngle) {
    lv_arc_set_angles(arcRing, 0, endAngle);
    lastArcEndAngle = endAngle;
  }
  lv_obj_set_style_arc_color(arcRing, color, LV_PART_INDICATOR);
}

static void update_rpm_label_if_changed() {
  if (!rpmValLbl) return;
  const float rpm = speed_get_target_rpm();
  if (fabsf(rpm - lastRpmShown) <= 0.049f) return;
  lastRpmShown = rpm;
  lv_label_set_text_fmt(rpmValLbl, "%.1f", rpm);
}

static void refresh_start_after_label() {
  if (!startAfterValLbl) return;
  lv_label_set_text_fmt(startAfterValLbl, "%ds", countdownSec);
}

static void save_countdown_setting() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.countdown_seconds = (uint8_t)countdownSec;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
}

static lv_color_t countdown_color(int remaining, int total) {
  if (total <= 0) return COL_GREEN;
  int pct = remaining * 100 / total;
  if (pct > 66) return COL_GREEN;
  if (pct > 33) return COL_YELLOW;
  return COL_RED;
}

static void back_event_cb(lv_event_t* e) {
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  backPending.store(true, std::memory_order_release);
}

static void sec_adj_cb(lv_event_t* e) {
  if (countingDown.load(std::memory_order_acquire)) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  countdownSec += delta;
  if (countdownSec < 1) countdownSec = 1;
  if (countdownSec > 10) countdownSec = 10;
  refresh_start_after_label();
}

static void start_event_cb(lv_event_t* e) {
  if (countingDown.load(std::memory_order_acquire)) return;
  if (safety_inhibit_motion()) return;
  if (control_get_state() != STATE_IDLE) return;

  save_countdown_setting();
  countingDown.store(true, std::memory_order_release);
  lastDisplayedSec = -1;
  countdownStartMs = millis();
  speed_slider_set(speed_get_target_rpm());
  if (warnDetailLbl) {
    lv_label_set_text(warnDetailLbl, "Motor remains disabled until zero");
    lv_obj_set_style_text_color(warnDetailLbl, COL_TEXT, 0);
  }
}

static void stop_event_cb(lv_event_t* e) {
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  if (control_get_state() != STATE_IDLE && control_get_state() != STATE_ESTOP) {
    control_stop();
  }
}

static lv_obj_t* timer_make_info_card(lv_obj_t* scr, int y, const char* title, bool isWarn) {
  const int cardH = 90;
  lv_obj_t* c = lv_obj_create(scr);
  lv_obj_set_size(c, 380, cardH);
  lv_obj_set_pos(c, 380, y);
  if (isWarn) {
    lv_obj_set_style_bg_color(c, COL_BG_WARN_PANEL, 0);
    lv_obj_set_style_border_color(c, COL_BORDER_WARN, 0);
  } else {
    lv_obj_set_style_bg_color(c, COL_BG_CARD, 0);
    lv_obj_set_style_border_color(c, COL_BORDER, 0);
  }
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(c, 0, 0);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* t = lv_label_create(c);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(t, isWarn ? COL_YELLOW : COL_TEXT_DIM, 0);
  lv_obj_set_pos(t, 16, 16);
  return c;
}

void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  countdownSec = g_settings.countdown_seconds;
  xSemaphoreGive(g_settings_mutex);
  if (countdownSec < 1) countdownSec = 3;
  if (countdownSec > 10) countdownSec = 10;
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  backPending.store(false, std::memory_order_release);
  lastDisplayedSec = -1;
  lastIdleCountdownSec = -1;
  lastArcEndAngle = -1;
  lastRpmShown = -1.0f;

  ui_create_header(screen, "COUNTDOWN", "DELAY START", nullptr);

  ringCard = lv_obj_create(screen);
  lv_obj_set_size(ringCard, 296, 296);
  lv_obj_set_pos(ringCard, 34, 60);
  lv_obj_set_style_bg_color(ringCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(ringCard, COL_BORDER, 0);
  lv_obj_set_style_border_width(ringCard, 1, 0);
  lv_obj_set_style_radius(ringCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(ringCard, 0, 0);
  lv_obj_remove_flag(ringCard, LV_OBJ_FLAG_SCROLLABLE);

  const int arcSz = 224;
  const int arcPad = (296 - arcSz) / 2;
  arcRing = lv_arc_create(ringCard);
  lv_obj_set_size(arcRing, arcSz, arcSz);
  lv_obj_set_pos(arcRing, arcPad, arcPad);
  lv_arc_set_bg_angles(arcRing, 0, 360);
  lv_arc_set_angles(arcRing, 0, 360);
  lastArcEndAngle = 360;
  lv_obj_set_style_arc_color(arcRing, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcRing, 14, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arcRing, COL_GREEN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcRing, 12, LV_PART_INDICATOR);
  lv_obj_remove_flag(arcRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(arcRing, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_remove_style(arcRing, nullptr, LV_PART_KNOB);

  bigNumberLabel = lv_label_create(ringCard);
  lv_label_set_text(bigNumberLabel, "3");
  lv_obj_set_style_text_font(bigNumberLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(bigNumberLabel, COL_ACCENT, 0);
  lv_obj_set_style_text_align(bigNumberLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bigNumberLabel, 296);
  lv_obj_align(bigNumberLabel, LV_ALIGN_CENTER, 0, -16);
  lv_obj_set_style_transform_pivot_x(bigNumberLabel, 148, 0);
  lv_obj_set_style_transform_pivot_y(bigNumberLabel, 24, 0);

  lv_obj_t* secCaption = lv_label_create(ringCard);
  lv_label_set_text(secCaption, "SECONDS");
  lv_obj_set_style_text_font(secCaption, FONT_LARGE, 0);
  lv_obj_set_style_text_color(secCaption, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_align(secCaption, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(secCaption, 296);
  lv_obj_align(secCaption, LV_ALIGN_CENTER, 0, 22);

  lv_obj_t* afterCard = timer_make_info_card(screen, 58, "START AFTER", false);
  startAfterValLbl = lv_label_create(afterCard);
  refresh_start_after_label();
  lv_obj_set_style_text_font(startAfterValLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(startAfterValLbl, COL_ACCENT, 0);
  lv_obj_set_style_text_align(startAfterValLbl, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_width(startAfterValLbl, 132);
  lv_obj_align(startAfterValLbl, LV_ALIGN_LEFT_MID, 108, 4);

  // Keep +/- clear of "%ds" text (was RIGHT + -64: suffix sat under + button)
  ui_create_btn(afterCard, 268, 22, 52, 44, "-", FONT_SUBTITLE, UI_BTN_NORMAL, sec_adj_cb, (void*)(intptr_t)(-1));
  ui_create_btn(afterCard, 326, 22, 52, 44, "+", FONT_SUBTITLE, UI_BTN_ACCENT, sec_adj_cb, (void*)(intptr_t)(1));

  lv_obj_t* rpmCard = timer_make_info_card(screen, 162, "TARGET RPM", false);
  rpmValLbl = lv_label_create(rpmCard);
  lv_label_set_text_fmt(rpmValLbl, "%.1f", speed_get_target_rpm());
  lastRpmShown = speed_get_target_rpm();
  lv_obj_set_style_text_font(rpmValLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmValLbl, COL_TEXT, 0);
  lv_obj_set_style_text_align(rpmValLbl, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(rpmValLbl, 180);
  lv_obj_align(rpmValLbl, LV_ALIGN_RIGHT_MID, -16, 4);

  lv_obj_t* warnCard = timer_make_info_card(screen, 266, "SAFETY HOLD", true);
  warnDetailLbl = lv_label_create(warnCard);
  lv_label_set_text(warnDetailLbl, "Motor remains disabled until zero");
  lv_obj_set_style_text_font(warnDetailLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(warnDetailLbl, COL_TEXT, 0);
  lv_obj_set_pos(warnDetailLbl, 16, 44);
  lv_obj_set_width(warnDetailLbl, 348);
  lv_label_set_long_mode(warnDetailLbl, LV_LABEL_LONG_MODE_WRAP);

  ui_create_action_bar_three(screen, 20, 408, 52, 16, 244,
                             "<  BACK", back_event_cb, UI_BTN_NORMAL,
                             "> START", start_event_cb, UI_BTN_ACCENT,
                             "X STOP", stop_event_cb, UI_BTN_DANGER,
                             nullptr, nullptr, nullptr);

  LOG_I("Screen countdown: POST layout created");
}

void screen_timer_invalidate_widgets() {
  ringCard = nullptr;
  arcRing = nullptr;
  bigNumberLabel = nullptr;
  startAfterValLbl = nullptr;
  rpmValLbl = nullptr;
  warnDetailLbl = nullptr;
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  backPending.store(false, std::memory_order_release);
  lastDisplayedSec = -1;
  lastIdleCountdownSec = -1;
  lastArcEndAngle = -1;
  lastRpmShown = -1.0f;
}

void screen_timer_update() {
  if (!screens_is_active(SCREEN_TIMER)) return;

  if (backPending.load(std::memory_order_acquire)) {
    backPending.store(false, std::memory_order_release);
    countingDown.store(false, std::memory_order_release);
    startPending.store(false, std::memory_order_release);
    save_countdown_setting();
    screens_request_show(SCREEN_MAIN);
    return;
  }

  if (startPending.load(std::memory_order_acquire)) {
    startPending.store(false, std::memory_order_release);
    countingDown.store(false, std::memory_order_release);
    if (safety_inhibit_motion() || control_get_state() != STATE_IDLE) {
      if (warnDetailLbl) {
        lv_label_set_text(warnDetailLbl, "START BLOCKED");
        lv_obj_set_style_text_color(warnDetailLbl, COL_RED, 0);
      }
      return;
    }
    if (control_start_continuous()) {
      screens_request_show(SCREEN_MAIN);
    } else if (warnDetailLbl) {
      lv_label_set_text(warnDetailLbl, "START BLOCKED");
      lv_obj_set_style_text_color(warnDetailLbl, COL_RED, 0);
    }
    return;
  }

  if (countingDown.load(std::memory_order_acquire)) {
    uint32_t elapsedMs = millis() - countdownStartMs;
    int remaining = countdownSec - (int)(elapsedMs / 1000);

    if (remaining <= 0) {
      if (bigNumberLabel) {
        lv_label_set_text(bigNumberLabel, "GO");
        lv_obj_set_style_text_color(bigNumberLabel, COL_GREEN, 0);
        lv_obj_set_style_transform_zoom(bigNumberLabel, 384, 0);
        lv_obj_update_layout(bigNumberLabel);
        lv_obj_set_style_transform_pivot_x(bigNumberLabel, lv_obj_get_width(bigNumberLabel) / 2, 0);
        lv_obj_set_style_transform_pivot_y(bigNumberLabel, lv_obj_get_height(bigNumberLabel) / 2, 0);
      }
      if (warnDetailLbl) {
        lv_label_set_text(warnDetailLbl, "Starting rotation");
        lv_obj_set_style_text_color(warnDetailLbl, COL_GREEN, 0);
      }
      if (arcRing) {
        set_countdown_arc_angle(360, COL_GREEN);
      }
      startPending.store(true, std::memory_order_release);
      return;
    }

    lv_color_t col = countdown_color(remaining, countdownSec);

    if (remaining != lastDisplayedSec) {
      lastDisplayedSec = remaining;
      pulseStartMs = millis();
    }

    uint32_t pulseElapsed = millis() - pulseStartMs;
    int zoom = (pulseElapsed < 150) ? 384 : 256;

    if (bigNumberLabel) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", remaining);
      lv_label_set_text(bigNumberLabel, buf);
      lv_obj_set_style_text_color(bigNumberLabel, col, 0);
      lv_obj_set_style_transform_zoom(bigNumberLabel, zoom, 0);
      lv_obj_update_layout(bigNumberLabel);
      lv_obj_set_style_transform_pivot_x(bigNumberLabel, lv_obj_get_width(bigNumberLabel) / 2, 0);
      lv_obj_set_style_transform_pivot_y(bigNumberLabel, lv_obj_get_height(bigNumberLabel) / 2, 0);
    }

    if (warnDetailLbl) {
      lv_label_set_text(warnDetailLbl, "Motor remains disabled until zero");
      lv_obj_set_style_text_color(warnDetailLbl, COL_TEXT, 0);
    }

    if (arcRing) {
      int pct = remaining * 100 / countdownSec;
      set_countdown_arc_angle(pct * 360 / 100, col);
    }
  } else {
    if (lastDisplayedSec != -1 || lastIdleCountdownSec != countdownSec) {
      lastDisplayedSec = -1;
      lastIdleCountdownSec = countdownSec;

      if (bigNumberLabel) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", countdownSec);
        lv_label_set_text(bigNumberLabel, buf);
        lv_obj_set_style_text_color(bigNumberLabel, COL_GREEN, 0);
        lv_obj_set_style_transform_zoom(bigNumberLabel, 256, 0);
        lv_obj_update_layout(bigNumberLabel);
        lv_obj_set_style_transform_pivot_x(bigNumberLabel, lv_obj_get_width(bigNumberLabel) / 2, 0);
        lv_obj_set_style_transform_pivot_y(bigNumberLabel, lv_obj_get_height(bigNumberLabel) / 2, 0);
      }
      if (warnDetailLbl) {
        lv_label_set_text(warnDetailLbl, "Motor remains disabled until zero");
        lv_obj_set_style_text_color(warnDetailLbl, COL_TEXT, 0);
      }
      set_countdown_arc_angle(360, COL_GREEN);
    }
  }

  update_rpm_label_if_changed();
}
