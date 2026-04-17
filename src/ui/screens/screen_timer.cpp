// TIG Rotator Controller - Countdown Screen
// 3-2-1 countdown before rotation starts, gives welder time to position
// Visual: progress ring, pulsing number, color transition green->yellow->red

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include "../../config.h"
#include "../../safety/safety.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static int countdownSec = 3;
// Touch event callbacks and screen_timer_update() both run on lvglTask (Core 1), but
// keep atomics to match project-wide standard and make memory ordering explicit.
static std::atomic<bool> countingDown{false};
static std::atomic<bool> startPending{false};
static uint32_t countdownStartMs = 0;
static int countdownRemaining = 0;
static int lastDisplayedSec = -1;
static uint32_t pulseStartMs = 0;
static std::atomic<bool> backPending{false};

static lv_obj_t* arcRing = nullptr;
static lv_obj_t* bigNumberLabel = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* secLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// COLOR HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static lv_color_t countdown_color(int remaining, int total) {
  if (total <= 0) return COL_GREEN;
  int pct = remaining * 100 / total;
  if (pct > 66) return COL_GREEN;
  if (pct > 33) return COL_YELLOW;
  return COL_RED;
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
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
  if (secLabel) lv_label_set_text_fmt(secLabel, "%d sec", countdownSec);
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  float rpm = speed_get_target_rpm();
  rpm += delta * 0.1f;
  if (rpm < MIN_RPM) rpm = MIN_RPM;
  if (rpm > speed_get_rpm_max()) rpm = speed_get_rpm_max();
  speed_slider_set(rpm);
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);
}

static void start_event_cb(lv_event_t* e) {
  if (countingDown.load(std::memory_order_acquire)) return;
  if (safety_inhibit_motion()) return;
  if (control_get_state() != STATE_IDLE) return;

  countingDown.store(true, std::memory_order_release);
  countdownRemaining = countdownSec;
  lastDisplayedSec = -1;
  countdownStartMs = millis();
  speed_slider_set(speed_get_target_rpm());
}

static void stop_event_cb(lv_event_t* e) {
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  if (control_get_state() != STATE_IDLE && control_get_state() != STATE_ESTOP) {
    control_stop();
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];
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

  ui_create_header(screen, "COUNTDOWN");

  // ── Arc ring (240x240, centered) ──
  const int ringSize = 240;
  const int ringX = (SCREEN_W - ringSize) / 2;
  const int ringY = 30;

  arcRing = lv_arc_create(screen);
  lv_obj_set_size(arcRing, ringSize, ringSize);
  lv_obj_set_pos(arcRing, ringX, ringY);
  lv_arc_set_range(arcRing, 0, 100);
  lv_arc_set_value(arcRing, 100);
  lv_arc_set_bg_angles(arcRing, 0, 360);
  lv_arc_set_angles(arcRing, 0, 360);
  lv_obj_set_style_arc_color(arcRing, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcRing, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arcRing, COL_GREEN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcRing, 8, LV_PART_INDICATOR);
  lv_obj_remove_flag(arcRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(arcRing, 0, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcRing, 0, LV_PART_KNOB);

  // ── Big number label (centered inside arc) ──
  bigNumberLabel = lv_label_create(screen);
  lv_label_set_text(bigNumberLabel, "3");
  lv_obj_set_style_text_font(bigNumberLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(bigNumberLabel, COL_GREEN, 0);
  lv_obj_set_style_text_align(bigNumberLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bigNumberLabel, ringSize);
  lv_obj_set_pos(bigNumberLabel, ringX, ringY + (ringSize - 44) / 2 - 4);
  lv_obj_set_style_transform_pivot_x(bigNumberLabel, ringSize / 2, 0);
  lv_obj_set_style_transform_pivot_y(bigNumberLabel, 22, 0);

  // ── Status label below arc ──
  statusLabel = lv_label_create(screen);
  lv_label_set_text(statusLabel, "READY");
  lv_obj_set_style_text_font(statusLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(statusLabel, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_align(statusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(statusLabel, 400);
  lv_obj_set_pos(statusLabel, 200, ringY + ringSize + 8);

  // ── Controls row (y=310) ──
  const int rowY = 320;

  lv_obj_t* secTitle = lv_label_create(screen);
  lv_label_set_text(secTitle, "DELAY");
  lv_obj_set_style_text_font(secTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(secTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(secTitle, 30, rowY + 12);

  secLabel = lv_label_create(screen);
  lv_label_set_text_fmt(secLabel, "%d sec", countdownSec);
  lv_obj_set_style_text_font(secLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(secLabel, COL_TEXT_BRIGHT, 0);
  lv_obj_set_pos(secLabel, 120, rowY + 10);

  ui_create_btn(screen, 220, rowY, 64, 52, "-", FONT_XL, UI_BTN_NORMAL, sec_adj_cb, (void*)(intptr_t)(-1));
  ui_create_btn(screen, 284, rowY, 64, 52, "+", FONT_XL, UI_BTN_NORMAL, sec_adj_cb, (void*)(intptr_t)(1));

  // ── RPM row (right side) ──
  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, 460, rowY + 12);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", speed_get_target_rpm());
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT_BRIGHT, 0);
  lv_obj_set_pos(rpmLabel, 510, rowY + 10);

  ui_create_btn(screen, 580, rowY, 64, 52, "-", FONT_XL, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)(-1));
  ui_create_btn(screen, 644, rowY, 64, 52, "+", FONT_XL, UI_BTN_NORMAL, rpm_adj_cb, (void*)(intptr_t)(1));

  // ── Bottom bar: BACK + START + STOP ──
  const int barY = 428;
  const int barH = 48;
  const int barBtnW = 256;
  const int barGap = 8;
  const int barX = 8;

  ui_create_action_bar_three(screen, barX, barY, barH, barGap, barBtnW,
                             "<  BACK", back_event_cb, UI_BTN_NORMAL,
                             "> START", start_event_cb, UI_BTN_ACCENT,
                             "[] STOP", stop_event_cb, UI_BTN_DANGER,
                             nullptr, nullptr, nullptr);

  LOG_I("Screen countdown: created with progress ring");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_invalidate_widgets() {
  arcRing = nullptr;
  bigNumberLabel = nullptr;
  statusLabel = nullptr;
  rpmLabel = nullptr;
  secLabel = nullptr;
  countingDown.store(false, std::memory_order_release);
  startPending.store(false, std::memory_order_release);
  backPending.store(false, std::memory_order_release);
}

void screen_timer_update() {
  if (!screens_is_active(SCREEN_TIMER)) return;

  if (backPending.load(std::memory_order_acquire)) {
    backPending.store(false, std::memory_order_release);
    countingDown.store(false, std::memory_order_release);
    startPending.store(false, std::memory_order_release);
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    g_settings.countdown_seconds = (uint8_t)countdownSec;
    xSemaphoreGive(g_settings_mutex);
    storage_save_settings();
    screens_request_show(SCREEN_MAIN);
    return;
  }

  // Handle pending start (countdown reached 0)
  if (startPending.load(std::memory_order_acquire)) {
    startPending.store(false, std::memory_order_release);
    countingDown.store(false, std::memory_order_release);
    control_start_continuous();
    screens_request_show(SCREEN_MAIN);
    return;
  }

  if (countingDown.load(std::memory_order_acquire)) {
    // Count elapsed milliseconds and floor-divide to whole seconds so the big
    // number ticks N, N-1, ... 1 and hits 0 at the exact (countdownSec * 1000)ms mark.
    uint32_t elapsedMs = millis() - countdownStartMs;
    int remaining = countdownSec - (int)(elapsedMs / 1000);

    if (remaining <= 0) {
      // GO!
      if (bigNumberLabel) {
        lv_label_set_text(bigNumberLabel, "GO");
        lv_obj_set_style_text_color(bigNumberLabel, COL_GREEN, 0);
        lv_obj_set_style_transform_zoom(bigNumberLabel, 384, 0);
      }
      if (statusLabel) {
        lv_label_set_text(statusLabel, "STARTING");
        lv_obj_set_style_text_color(statusLabel, COL_GREEN, 0);
      }
      if (arcRing) {
        lv_arc_set_value(arcRing, 100);
        lv_arc_set_angles(arcRing, 0, 360);
        lv_obj_set_style_arc_color(arcRing, COL_GREEN, LV_PART_INDICATOR);
      }
      startPending.store(true, std::memory_order_release);
      return;
    }

    countdownRemaining = remaining;
    lv_color_t col = countdown_color(remaining, countdownSec);

    // Pulse on second change
    if (remaining != lastDisplayedSec) {
      lastDisplayedSec = remaining;
      pulseStartMs = millis();
    }

    // Zoom: 150% for 150ms after change, then 100%
    uint32_t pulseElapsed = millis() - pulseStartMs;
    int zoom = (pulseElapsed < 150) ? 384 : 256;

    if (bigNumberLabel) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", remaining);
      lv_label_set_text(bigNumberLabel, buf);
      lv_obj_set_style_text_color(bigNumberLabel, col, 0);
      lv_obj_set_style_transform_zoom(bigNumberLabel, zoom, 0);
    }

    if (statusLabel) {
      lv_label_set_text(statusLabel, "GET READY");
      lv_obj_set_style_text_color(statusLabel, col, 0);
    }

    if (arcRing) {
      int pct = remaining * 100 / countdownSec;
      lv_arc_set_value(arcRing, pct);
      lv_arc_set_angles(arcRing, 0, pct * 360 / 100);
      lv_obj_set_style_arc_color(arcRing, col, LV_PART_INDICATOR);
    }
  } else {
    // Idle — show countdown value with green ring
    if (lastDisplayedSec != -1) {
      lastDisplayedSec = -1;
    }

    if (bigNumberLabel) {
      char buf[4];
      snprintf(buf, sizeof(buf), "%d", countdownSec);
      lv_label_set_text(bigNumberLabel, buf);
      lv_obj_set_style_text_color(bigNumberLabel, COL_GREEN, 0);
      lv_obj_set_style_transform_zoom(bigNumberLabel, 256, 0);
    }
    if (statusLabel) {
      lv_label_set_text(statusLabel, "READY");
      lv_obj_set_style_text_color(statusLabel, COL_TEXT_DIM, 0);
    }
    if (arcRing) {
      lv_arc_set_value(arcRing, 100);
      lv_arc_set_angles(arcRing, 0, 360);
      lv_obj_set_style_arc_color(arcRing, COL_GREEN, LV_PART_INDICATOR);
    }
  }

  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", speed_get_target_rpm());
}
