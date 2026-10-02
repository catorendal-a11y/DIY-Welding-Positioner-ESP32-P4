// TIG Rotator Controller - Display Settings Screen
// Brightness, dim timeout, accent color selection
#include "../screens.h"
#include "../theme.h"
#include "../display.h"
#include "../../config.h"
#include "../../mirror/usb_mirror.h"
#include "../../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <atomic>

static lv_obj_t* brightnessSlider = nullptr;
static lv_obj_t* brightnessValueLabel = nullptr;
static lv_obj_t* dimBtn = nullptr;
static lv_obj_t* dimBtnLabel = nullptr;
static lv_obj_t* schemeBtn = nullptr;
static lv_obj_t* schemeBtnLabel = nullptr;
static lv_obj_t* themeBtn = nullptr;
static lv_obj_t* themeBtnLabel = nullptr;
#if ENABLE_USB_UI_MIRROR
static lv_obj_t* mirrorBtn = nullptr;
static lv_obj_t* mirrorBtnLabel = nullptr;
#endif
static lv_obj_t* infoLabel = nullptr;

static const int dimTimeouts[] = {0, 30, 60, 120, 300};
static const char* dimStrings[] = {"OFF", "30s", "1m", "2m", "5m"};
static const int dimCount = 5;
static int currentDimIdx = 0;

static bool displayScreenActive = false;
static bool ignoreSliderCb = false;
static void update_info_text();
#if ENABLE_USB_UI_MIRROR
static void update_mirror_label();
#endif

void screen_display_mark_dirty() { displayScreenActive = true; }

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

static void back_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }

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

static void scheme_cycle_cb(lv_event_t* e) {
  (void)e;
  uint8_t cur = 0;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  cur = g_settings.color_scheme;
  xSemaphoreGive(g_settings_mutex);
  const uint8_t next = (uint8_t)((cur + 1) % theme_get_scheme_count());
  theme_set_scheme(next);
  if (schemeBtnLabel) {
    lv_label_set_text(schemeBtnLabel, theme_get_scheme_name(next));
  }
  themeRefreshPending.store(true, std::memory_order_release);
}

static void save_cb(lv_event_t* e) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.dim_timeout = dimTimeouts[currentDimIdx];
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
}

#if ENABLE_USB_UI_MIRROR
static void mirror_toggle_cb(lv_event_t* e) {
  (void)e;
  if (!usb_mirror_is_connected()) {
    usb_mirror_set_armed(false);
  } else {
    usb_mirror_set_armed(!usb_mirror_is_armed());
  }
  update_mirror_label();
}

static void update_mirror_label() {
  if (!mirrorBtnLabel) return;
  if (usb_mirror_is_armed()) {
    lv_label_set_text(mirrorBtnLabel, "ARMED");
  } else if (usb_mirror_is_connected()) {
    lv_label_set_text(mirrorBtnLabel, "LINK");
  } else {
    lv_label_set_text(mirrorBtnLabel, "NO LINK");
  }
}
#endif

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
  uint16_t dimSec;
  uint8_t scheme, accent;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  dimSec = g_settings.dim_timeout;
  scheme = g_settings.color_scheme;
  accent = g_settings.accent_color;
  xSemaphoreGive(g_settings_mutex);
  currentDimIdx = 0;
  for (int i = 0; i < dimCount; ++i)
    if (dimTimeouts[i] == dimSec) currentDimIdx = i;
  ui_create_header(screen, "Display settings", "SCREEN & USB", nullptr);
  ui_create_text(screen, 24, 94, 400, "BRIGHTNESS", FONT_SUBTITLE, COL_TEXT_DIM);
  brightnessValueLabel = ui_create_text(screen, 656, 94, 120, "", FONT_LARGE, COL_TEXT);
  lv_obj_set_style_text_align(brightnessValueLabel, LV_TEXT_ALIGN_RIGHT, 0);
  brightnessSlider = lv_slider_create(screen);
  lv_obj_set_pos(brightnessSlider, 36, 132);
  lv_obj_set_size(brightnessSlider, 728, 16);
  lv_slider_set_range(brightnessSlider, 20, 100);
  ui_style_slider(brightnessSlider);
  lv_obj_set_ext_click_area(brightnessSlider, 16);
  lv_obj_add_event_cb(brightnessSlider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  update_slider_from_settings();
  auto setting = [screen](int y, const char* title, const char* value, lv_event_cb_t cb) {
    lv_obj_t* row = ui_create_post_card(screen, 24, y, 752, 52);
    ui_create_text(row, 16, 16, 470, title, FONT_SUBTITLE, COL_TEXT);
    return ui_create_btn(row, 554, 2, 194, 48, value, FONT_BTN, UI_BTN_NORMAL, cb, nullptr);
  };
  dimBtn = setting(172, "Dim after", dimStrings[currentDimIdx], dim_cycle_cb);
  dimBtnLabel = lv_obj_get_child(dimBtn, 0);
  schemeBtn = setting(230, "Appearance", theme_get_scheme_name(scheme), scheme_cycle_cb);
  schemeBtnLabel = lv_obj_get_child(schemeBtn, 0);
  themeBtn = setting(288, "Accent color", theme_get_name(accent), theme_cycle_cb);
  themeBtnLabel = lv_obj_get_child(themeBtn, 0);
#if ENABLE_USB_UI_MIRROR
  mirrorBtn = setting(346, "USB remote control", "NO LINK", mirror_toggle_cb);
  mirrorBtnLabel = lv_obj_get_child(mirrorBtn, 0);
  update_mirror_label();
  infoLabel = nullptr;
#else
  infoLabel = ui_create_text(screen, 24, 358, 752, "", FONT_SUBTITLE, COL_TEXT_DIM);
  update_info_text();
#endif
  ui_create_btn(screen, 24, 408, 152, 56, "BACK", FONT_BTN, UI_BTN_NORMAL, back_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "SAVE", FONT_BTN, UI_BTN_ACCENT, save_cb, nullptr);
}

void screen_display_invalidate_widgets() {
  brightnessSlider = nullptr;
  brightnessValueLabel = nullptr;
  dimBtn = nullptr;
  dimBtnLabel = nullptr;
  schemeBtn = nullptr;
  schemeBtnLabel = nullptr;
  themeBtn = nullptr;
  themeBtnLabel = nullptr;
#if ENABLE_USB_UI_MIRROR
  mirrorBtn = nullptr;
  mirrorBtnLabel = nullptr;
#endif
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
#if ENABLE_USB_UI_MIRROR
  update_mirror_label();
#endif
}
