// TIG Rotator Controller - Display Settings Screen
// Brightness, dim timeout, accent color selection
#include "../screens.h"
#include "../theme.h"
#include "../display.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

static lv_obj_t* brightnessSlider = nullptr;
static lv_obj_t* brightnessValueLabel = nullptr;
static lv_obj_t* dimBtn = nullptr;
static lv_obj_t* dimBtnLabel = nullptr;
static lv_obj_t* themeBtn = nullptr;
static lv_obj_t* themeBtnLabel = nullptr;
static lv_obj_t* infoLabel = nullptr;

static const int dimTimeouts[] = { 0, 30, 60, 120, 300 };
static const char* dimStrings[] = { "OFF", "30s", "1m", "2m", "5m" };
static const int dimCount = 5;
static int currentDimIdx = 0;

static bool displayScreenActive = false;
static bool ignoreSliderCb = false;
static void update_info_text();

void screen_display_mark_dirty() {
  displayScreenActive = true;
}

static void update_slider_from_settings() {
  if (!brightnessSlider || !brightnessValueLabel) return;
  ignoreSliderCb = true;
  uint8_t br = 150;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  br = g_settings.brightness;
  xSemaphoreGive(g_settings_mutex);
  int brightPct = (int)((uint32_t)br * 100 / 255);
  if (brightPct < 20) brightPct = 20;
  lv_slider_set_value(brightnessSlider, brightPct, LV_ANIM_OFF);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", brightPct);
  lv_label_set_text(brightnessValueLabel, buf);
  ignoreSliderCb = false;
}

static void back_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

static void brightness_slider_cb(lv_event_t* e) {
  if (ignoreSliderCb) return;
  if (!brightnessValueLabel || !brightnessSlider) return;
  int val = lv_slider_get_value(brightnessSlider);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", val);
  lv_label_set_text(brightnessValueLabel, buf);
  uint8_t b = (uint8_t)(val * 255 / 100);
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.brightness = b;
  xSemaphoreGive(g_settings_mutex);
  display_set_brightness(b);
}

static void dim_cycle_cb(lv_event_t* e) {
  currentDimIdx = (currentDimIdx + 1) % dimCount;
  if (dimBtnLabel) {
    lv_label_set_text(dimBtnLabel, dimStrings[currentDimIdx]);
  }
  update_info_text();
}

static std::atomic<bool> themeRefreshPending{false};

static void theme_cycle_cb(lv_event_t* e) {
  uint8_t count = theme_get_count();
  uint8_t next = 0;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  next = (uint8_t)((g_settings.accent_color + 1) % count);
  xSemaphoreGive(g_settings_mutex);
  theme_set_color(next);
  if (themeBtnLabel) {
    lv_label_set_text(themeBtnLabel, theme_get_name(next));
  }
  themeRefreshPending.store(true, std::memory_order_release);
}

static void save_cb(lv_event_t* e) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.dim_timeout = dimTimeouts[currentDimIdx];
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
}

static void update_info_text() {
  if (!infoLabel) return;
  char buf[64];
  if (dimTimeouts[currentDimIdx] == 0) {
    snprintf(buf, sizeof(buf), "Auto-dim is disabled");
  } else {
    snprintf(buf, sizeof(buf), "Display dims after %s of inactivity", dimStrings[currentDimIdx]);
  }
  lv_label_set_text(infoLabel, buf);
}

// Uses shared ui_style_slider() from screens.cpp.

void screen_display_create() {
  lv_obj_t* screen = screenRoots[SCREEN_DISPLAY];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;
  const int CONTENT_W = SCREEN_W - 2 * PX;

  {
    uint8_t dimSec = 60;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    dimSec = g_settings.dim_timeout;
    xSemaphoreGive(g_settings_mutex);
    for (int i = 0; i < dimCount; i++) {
      if (dimTimeouts[i] == dimSec) {
        currentDimIdx = i;
        break;
      }
    }
  }

  ui_create_settings_header(screen, "DISPLAY SETTINGS", "", COL_TEXT_DIM);

  int y = 48;
  ui_create_post_row(screen, PX, y, CONTENT_W, SET_ROW_H);

  lv_obj_t* brightTitleLbl = lv_label_create(screen);
  lv_label_set_text(brightTitleLbl, "BRIGHTNESS");
  lv_obj_set_style_text_font(brightTitleLbl, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(brightTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(brightTitleLbl, PX + 8, y + 14);

  int brightPct = 60;
  {
    uint8_t br = 150;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    br = g_settings.brightness;
    xSemaphoreGive(g_settings_mutex);
    brightPct = (int)((uint32_t)br * 100 / 255);
  }
  brightnessValueLabel = lv_label_create(screen);
  char brightBuf[16];
  snprintf(brightBuf, sizeof(brightBuf), "%d%%", brightPct);
  lv_label_set_text(brightnessValueLabel, brightBuf);
  lv_obj_set_style_text_font(brightnessValueLabel, FONT_BTN, 0);
  lv_obj_set_style_text_color(brightnessValueLabel, COL_TEXT_WHITE, 0);
  lv_obj_set_pos(brightnessValueLabel, PX + 120, y + 12);

  brightnessSlider = lv_slider_create(screen);
  lv_obj_set_size(brightnessSlider, 300, 24);
  lv_obj_set_pos(brightnessSlider, PX + 200, y + 12);
  lv_slider_set_range(brightnessSlider, 20, 100);
  int initPct = brightPct < 20 ? 20 : brightPct;
  lv_slider_set_value(brightnessSlider, initPct, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(brightnessSlider, COL_SLIDER_TRACK, 0);
  lv_obj_set_style_bg_color(brightnessSlider, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(brightnessSlider, COL_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_border_color(brightnessSlider, COL_SLIDER_BORDER, 0);
  lv_obj_set_style_border_color(brightnessSlider, COL_KNOB_BORDER, LV_PART_KNOB);
  lv_obj_set_style_border_width(brightnessSlider, 2, 0);
  lv_obj_set_style_border_width(brightnessSlider, 2, LV_PART_KNOB);
  lv_obj_set_style_border_width(brightnessSlider, 0, LV_PART_INDICATOR);
  lv_obj_set_style_radius(brightnessSlider, 4, 0);
  lv_obj_set_style_radius(brightnessSlider, 9, LV_PART_KNOB);
  lv_obj_set_style_pad_top(brightnessSlider, -4, LV_PART_KNOB);
  lv_obj_set_style_pad_bottom(brightnessSlider, -4, LV_PART_KNOB);
  lv_obj_set_style_pad_all(brightnessSlider, 0, 0);
  lv_obj_add_event_cb(brightnessSlider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);

  y += 48;

  lv_obj_t* dimRow = lv_obj_create(screen);
  lv_obj_set_size(dimRow, CONTENT_W, SET_ROW_H);
  lv_obj_set_pos(dimRow, PX, y);
  ui_style_post_row(dimRow);
  lv_obj_remove_flag(dimRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* dimTitleLbl = lv_label_create(dimRow);
  lv_label_set_text(dimTitleLbl, "DIM TIMEOUT");
  lv_obj_set_style_text_font(dimTitleLbl, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(dimTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(dimTitleLbl, LV_ALIGN_LEFT_MID, 12, 0);

  dimBtn = ui_create_btn(dimRow, CONTENT_W - 130, 8, SET_CYCLE_W, SET_CYCLE_H,
                         dimStrings[currentDimIdx], FONT_BTN, UI_BTN_NORMAL, dim_cycle_cb, nullptr);
  dimBtnLabel = lv_obj_get_child(dimBtn, 0);

  y += SET_ROW_H + 10;

  lv_obj_t* themeRow = lv_obj_create(screen);
  lv_obj_set_size(themeRow, CONTENT_W, SET_ROW_H);
  lv_obj_set_pos(themeRow, PX, y);
  ui_style_post_row(themeRow);
  lv_obj_remove_flag(themeRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* themeTitleLbl = lv_label_create(themeRow);
  lv_label_set_text(themeTitleLbl, "ACCENT COLOR");
  lv_obj_set_style_text_font(themeTitleLbl, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(themeTitleLbl, COL_TEXT_DIM, 0);
  lv_obj_align(themeTitleLbl, LV_ALIGN_LEFT_MID, 12, 0);

  uint8_t accentIdxForBtn = 0;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  accentIdxForBtn = g_settings.accent_color;
  xSemaphoreGive(g_settings_mutex);
  themeBtn = ui_create_btn(themeRow, CONTENT_W - 130, 8, SET_CYCLE_W, SET_CYCLE_H,
                           theme_get_name(accentIdxForBtn), FONT_BTN, UI_BTN_NORMAL, theme_cycle_cb, nullptr);
  themeBtnLabel = lv_obj_get_child(themeBtn, 0);

  y += SET_ROW_H + 10;

  lv_obj_t* infoBar = lv_obj_create(screen);
  lv_obj_set_size(infoBar, CONTENT_W, 36);
  lv_obj_set_pos(infoBar, PX, y);
  ui_style_post_row(infoBar);
  lv_obj_remove_flag(infoBar, LV_OBJ_FLAG_CLICKABLE);

  infoLabel = lv_label_create(infoBar);
  lv_obj_set_style_text_font(infoLabel, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(infoLabel, COL_TEXT_DIM, 0);
  lv_obj_align(infoLabel, LV_ALIGN_LEFT_MID, 8, 0);
  update_info_text();

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = 160;
  int gap = 8;

  ui_create_action_bar(screen, PX, footerY, footerH, gap, btnW, btnW + 60,
                        "BACK", back_cb, "SAVE", UI_BTN_ACCENT, save_cb);

  LOG_I("Screen display: created");
}

void screen_display_invalidate_widgets() {
  brightnessSlider = nullptr;
  brightnessValueLabel = nullptr;
  dimBtn = nullptr;
  dimBtnLabel = nullptr;
  themeBtn = nullptr;
  themeBtnLabel = nullptr;
  infoLabel = nullptr;
  displayScreenActive = false;
}

void screen_display_update() {
  if (themeRefreshPending.load(std::memory_order_acquire)) {
    themeRefreshPending.store(false, std::memory_order_release);
    screens_request_theme_reinit();
    return;
  }
  if (displayScreenActive) {
    displayScreenActive = false;
    update_slider_from_settings();
  }
}
