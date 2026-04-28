// Boot Screen - Industrial POST startup status

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static lv_obj_t* progressBar = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* percentLabel = nullptr;
static lv_obj_t* stepLabel = nullptr;
static lv_obj_t* stepNameLabel = nullptr;
static lv_obj_t* lastLogLabel = nullptr;
static lv_obj_t* postRows[7] = {};
static lv_obj_t* postStateLabels[7] = {};
static lv_obj_t* postDetailLabels[7] = {};
static lv_obj_t* estopValueLabel = nullptr;
static lv_obj_t* almValueLabel = nullptr;
static lv_obj_t* enaValueLabel = nullptr;
static lv_obj_t* motorValueLabel = nullptr;
static int currentProgress = 0;

static const char* postNames[] = {
  "ESP32-P4 core init",
  "Display + LVGL",
  "Touch + storage",
  "DM542T driver check",
  "Safety inputs",
  "Pedal + speed input",
  "Ready handoff"
};

static const char* postDetails[] = {
  "core0/core1",
  "800x480",
  "config loaded",
  "ENA high",
  "ESTOP/ALM",
  "ADC/GPIO",
  "main screen"
};

static lv_obj_t* make_box(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                          lv_color_t bg, lv_color_t border, lv_coord_t radius, lv_coord_t borderWidth) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_style_bg_color(obj, bg, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, border, 0);
  lv_obj_set_style_border_width(obj, borderWidth, 0);
  lv_obj_set_style_radius(obj, radius, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  return obj;
}

static lv_obj_t* make_label(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, const char* text,
                            const lv_font_t* font, lv_color_t color, lv_coord_t width = 0) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_obj_set_pos(label, x, y);
  if (width > 0) {
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
  }
  return label;
}

static void set_label(lv_obj_t* label, const char* text, lv_color_t color) {
  if (!label) return;
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, color, 0);
}

static int progress_to_step(int percent) {
  if (percent >= 100) return 7;
  if (percent >= 95) return 6;
  if (percent >= 90) return 5;
  if (percent >= 70) return 4;
  if (percent >= 50) return 3;
  if (percent >= 30) return 2;
  if (percent >= 10) return 1;
  return 0;
}

static void refresh_post_rows(int percent) {
  const int activeCount = progress_to_step(percent);

  for (int i = 0; i < 7; i++) {
    if (!postRows[i] || !postStateLabels[i] || !postDetailLabels[i]) continue;

    if (i < activeCount) {
      lv_obj_set_style_bg_color(postRows[i], COL_BG_DIM, 0);
      lv_obj_set_style_border_color(postRows[i], COL_BORDER_ROW, 0);
      set_label(postStateLabels[i], "OK", COL_GREEN);
      lv_obj_set_style_text_color(postDetailLabels[i], COL_TEXT_DIM, 0);
    } else if (i == activeCount && percent < 100) {
      lv_obj_set_style_bg_color(postRows[i], COL_BG_ACTIVE, 0);
      lv_obj_set_style_border_color(postRows[i], COL_ACCENT, 0);
      set_label(postStateLabels[i], "RUN", COL_ACCENT);
      lv_obj_set_style_text_color(postDetailLabels[i], COL_TEXT_DIM, 0);
    } else {
      lv_obj_set_style_bg_color(postRows[i], COL_BG_DIM, 0);
      lv_obj_set_style_border_color(postRows[i], COL_BORDER_ROW, 0);
      set_label(postStateLabels[i], "WAIT", COL_TEXT_VDIM);
      lv_obj_set_style_text_color(postDetailLabels[i], COL_TEXT_VDIM, 0);
    }
  }
}

static void refresh_pin_status() {
  const bool estopClear = (digitalRead(PIN_ESTOP) == HIGH);
  const bool almOk = (digitalRead(PIN_DRIVER_ALM) == HIGH);
  const bool enaHigh = (digitalRead(PIN_ENA) == HIGH);

  set_label(estopValueLabel, estopClear ? "CLEAR" : "ACTIVE", estopClear ? COL_GREEN : COL_RED);
  set_label(almValueLabel, almOk ? "OK" : "FAULT", almOk ? COL_GREEN : COL_RED);
  set_label(enaValueLabel, enaHigh ? "HIGH" : "LOW", enaHigh ? COL_YELLOW : COL_RED);
  set_label(motorValueLabel, enaHigh ? "DISABLED" : "ENABLED", enaHigh ? COL_TEXT : COL_RED);
}

void screen_boot_set_progress(int percent, const char* message) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  currentProgress = percent;

  if (progressBar) lv_bar_set_value(progressBar, percent, LV_ANIM_ON);

  if (percentLabel) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(percentLabel, buf);
  }

  const int step = progress_to_step(percent);
  if (stepLabel) {
    char buf[16];
    int shownStep = (percent >= 100) ? 7 : (step + 1);
    snprintf(buf, sizeof(buf), "STEP %d/7", shownStep);
    lv_label_set_text(stepLabel, buf);
  }

  if (statusLabel && message) {
    lv_label_set_text(statusLabel, message);
  }

  if (stepNameLabel && message) {
    lv_label_set_text(stepNameLabel, message);
  }

  if (lastLogLabel && message) {
    char buf[80];
    snprintf(buf, sizeof(buf), "> %s", message);
    lv_label_set_text(lastLogLabel, buf);
  }

  refresh_post_rows(percent);
  refresh_pin_status();
}

void screen_boot_increment(int delta) {
  screen_boot_set_progress(currentProgress + delta, nullptr);
}

void screen_boot_create() {
  lv_obj_t* screen = screenRoots[SCREEN_BOOT];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  make_box(screen, 0, 0, SCREEN_W, BOOT_HEADER_H, COL_BG_HEADER, COL_BG_HEADER, 0, 0);
  ui_create_separator_line(screen, 0, BOOT_HEADER_H, SCREEN_W, COL_BORDER);
  ui_add_post_header_accent(screen);
  make_label(screen, 16, 9, "TIG-ROTATOR", FONT_BTN, COL_ACCENT);
  make_label(screen, 178, 12, "BOOT POST", FONT_NORMAL, COL_GREEN);
  make_label(screen, 646, 5, "FW", FONT_TINY, COL_TEXT_DIM);
  make_label(screen, 670, 5, FW_VERSION, FONT_TINY, COL_TEXT);
  make_label(screen, 646, 21, "MODE", FONT_TINY, COL_TEXT_DIM);
  make_label(screen, 690, 21, "SAFE INIT", FONT_TINY, COL_GREEN);

  make_box(screen, BOOT_PAD, BOOT_TITLE_Y, SCREEN_W - BOOT_PAD * 2, BOOT_TITLE_H,
           COL_BG_CARD_ALT, COL_BORDER, RADIUS_CARD, 1);
  make_label(screen, 32, 67, "SYSTEM STARTUP", FONT_TINY, COL_TEXT_DIM);
  statusLabel = make_label(screen, 32, 87, "RUNNING HARDWARE CHECKS", FONT_XL, COL_GREEN, 480);
  make_box(screen, 612, 66, 144, 28, COL_BG_ROW, COL_GREEN, 14, 1);
  lv_obj_t* motorSafeLabel = make_label(screen, 624, 73, "MOTOR DISABLED", FONT_TINY, COL_GREEN, 120);
  lv_obj_set_style_text_align(motorSafeLabel, LV_TEXT_ALIGN_CENTER, 0);

  make_box(screen, BOOT_LEFT_X, BOOT_LEFT_Y, BOOT_LEFT_W, BOOT_LEFT_H,
           COL_BG_CARD_ALT, COL_BORDER, RADIUS_CARD, 1);
  make_label(screen, 32, 140, "POWER-ON SELF TEST", FONT_NORMAL, COL_ACCENT);
  ui_create_separator_line(screen, 32, 162, 424, COL_BORDER);

  for (int i = 0; i < 7; i++) {
    const lv_coord_t y = BOOT_ROW_Y + i * BOOT_ROW_GAP;
    postRows[i] = make_box(screen, BOOT_ROW_X, y, BOOT_ROW_W, BOOT_ROW_H,
                           COL_BG_DIM, COL_BORDER_ROW, RADIUS_BTN, 1);
    postStateLabels[i] = make_label(screen, 44, y + 5, "WAIT", FONT_TINY, COL_TEXT_VDIM, 38);
    make_label(screen, 84, y + 5, postNames[i], FONT_TINY, COL_TEXT, 220);
    postDetailLabels[i] = make_label(screen, 344, y + 5, postDetails[i], FONT_TINY, COL_TEXT_DIM, 100);
  }

  make_box(screen, BOOT_RIGHT_X, BOOT_RIGHT_Y, BOOT_RIGHT_W, BOOT_RIGHT_H,
           COL_BG_CARD_ALT, COL_BORDER, RADIUS_CARD, 1);
  make_label(screen, 504, 140, "LIVE MACHINE STATUS", FONT_NORMAL, COL_ACCENT);
  ui_create_separator_line(screen, 504, 162, 264, COL_BORDER);

  make_label(screen, 504, 184, "ESTOP", FONT_TINY, COL_TEXT_DIM);
  estopValueLabel = make_label(screen, 650, 184, "--", FONT_TINY, COL_GREEN, 92);
  lv_obj_set_style_text_align(estopValueLabel, LV_TEXT_ALIGN_CENTER, 0);

  make_label(screen, 504, 216, "DRIVER ALM", FONT_TINY, COL_TEXT_DIM);
  almValueLabel = make_label(screen, 650, 216, "--", FONT_TINY, COL_GREEN, 92);
  lv_obj_set_style_text_align(almValueLabel, LV_TEXT_ALIGN_CENTER, 0);

  make_label(screen, 504, 248, "ENABLE PIN", FONT_TINY, COL_TEXT_DIM);
  enaValueLabel = make_label(screen, 650, 248, "--", FONT_TINY, COL_YELLOW, 92);
  lv_obj_set_style_text_align(enaValueLabel, LV_TEXT_ALIGN_CENTER, 0);

  make_label(screen, 504, 280, "MOTOR STATE", FONT_TINY, COL_TEXT_DIM);
  motorValueLabel = make_label(screen, 650, 280, "--", FONT_TINY, COL_TEXT, 120);

  make_label(screen, 504, 312, "RPM RANGE", FONT_TINY, COL_TEXT_DIM);
  char rpmBuf[32];
  snprintf(rpmBuf, sizeof(rpmBuf), "%.3f - %.3f", (double)MIN_RPM, (double)speed_get_rpm_max());
  make_label(screen, 650, 312, rpmBuf, FONT_TINY, COL_TEXT, 120);

  make_label(screen, 504, 344, "GEAR RATIO", FONT_TINY, COL_TEXT_DIM);
  char gearBuf[24];
  snprintf(gearBuf, sizeof(gearBuf), "%.0f:1", (double)GEAR_RATIO);
  make_label(screen, 650, 344, gearBuf, FONT_TINY, COL_TEXT, 80);

  make_box(screen, BOOT_BOTTOM_X, BOOT_BOTTOM_Y, BOOT_BOTTOM_W, BOOT_BOTTOM_H,
           COL_BG_CARD_ALT, COL_BORDER, RADIUS_CARD, 1);
  stepLabel = make_label(screen, 32, 407, "STEP 0/7", FONT_TINY, COL_TEXT_DIM);
  stepNameLabel = make_label(screen, 104, 407, "BOOT SEQUENCE", FONT_TINY, COL_ACCENT, 360);
  percentLabel = make_label(screen, 700, 407, "0%", FONT_TINY, COL_TEXT_DIM, 68);
  lv_obj_set_style_text_align(percentLabel, LV_TEXT_ALIGN_RIGHT, 0);

  progressBar = lv_bar_create(screen);
  lv_obj_set_size(progressBar, BOOT_PROGRESS_W, BOOT_PROGRESS_H);
  lv_obj_set_pos(progressBar, BOOT_PROGRESS_X, BOOT_PROGRESS_Y);
  lv_bar_set_range(progressBar, 0, 100);
  lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
  lv_obj_set_style_base_dir(progressBar, LV_BASE_DIR_LTR, 0);
  lv_obj_set_style_pad_all(progressBar, 0, 0);
  lv_obj_set_style_bg_color(progressBar, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_INDICATOR);

  lastLogLabel = make_label(screen, 32, 448, "> ENA kept HIGH until safe idle", FONT_NORMAL, COL_TEXT, 720);

  refresh_pin_status();
  screen_boot_set_progress(0, "WAITING FOR BOOT SEQUENCE");
  LOG_I("Screen boot: industrial POST layout created");
}

void screen_boot_update(int percent, const char* status) {
  screen_boot_set_progress(percent, status);
}
