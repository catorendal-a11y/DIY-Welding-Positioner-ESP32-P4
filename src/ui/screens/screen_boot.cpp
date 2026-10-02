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

static const char* postNames[] = {"ESP32-P4 core init",  "Display + LVGL", "Touch + storage",
                                  "DM542T driver check", "Safety inputs",  "Pedal + speed input",
                                  "Ready handoff"};

static const char* postDetails[] = {"core0/core1", "800x480",  "config loaded", "ENA high",
                                    "ESTOP/ALM",   "ADC/GPIO", "main screen"};

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

void screen_boot_increment(int delta) { screen_boot_set_progress(currentProgress + delta, nullptr); }

void screen_boot_create() {
  lv_obj_t* screen = screenRoots[SCREEN_BOOT];
  lv_obj_clean(screen);
  ui_create_header(screen, "TIG / ROTATOR", "STARTUP", nullptr);
  ui_create_text(screen, 24, 100, 360, "Preparing the", FONT_XXL, COL_TEXT);
  ui_create_text(screen, 24, 142, 360, "next weld.", FONT_XXL, COL_TEXT);
  ui_create_text(screen, 24, 204, 368, "Initializing the operator interface.", FONT_SUBTITLE, COL_TEXT_DIM);
  ui_create_text(screen, 24, 238, 368, "Motion starts only on a new request.", FONT_SUBTITLE, COL_TEXT_DIM);
  statusLabel = ui_create_text(screen, 24, 294, 368, "Starting...", FONT_SUBTITLE, COL_ACCENT);
  progressBar = lv_bar_create(screen);
  lv_obj_set_pos(progressBar, 24, 336);
  lv_obj_set_size(progressBar, 368, 8);
  lv_bar_set_range(progressBar, 0, 100);
  lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
  percentLabel = ui_create_text(screen, 24, 358, 368, "0%", FONT_SUBTITLE, COL_TEXT_DIM);
  const char* keys[] = {"E-STOP INPUT", "DRIVER ALARM", "ENABLE OUTPUT", "MOTOR OUTPUT"};
  lv_obj_t** vals[] = {&estopValueLabel, &almValueLabel, &enaValueLabel, &motorValueLabel};
  for (int i = 0; i < 4; ++i) {
    lv_obj_t* card = ui_create_post_card(screen, 424, 94 + i * 72, 352, 60);
    ui_create_text(card, 16, 8, 320, keys[i], FONT_NORMAL, COL_TEXT_DIM);
    *vals[i] = ui_create_text(card, 16, 30, 320, "--", FONT_SUBTITLE, COL_TEXT);
  }
  stepLabel = stepNameLabel = lastLogLabel = nullptr;
  for (int i = 0; i < 7; ++i) postRows[i] = postStateLabels[i] = postDetailLabels[i] = nullptr;
  ui_create_text(screen, 24, 430, 752,
                 "Live input levels shown at right. Startup progress is not a hardware self-test.",
                 FONT_NORMAL, COL_TEXT_DIM);
  screen_boot_set_progress(0, "Starting interface");
}

void screen_boot_update(int percent, const char* status) { screen_boot_set_progress(percent, status); }
