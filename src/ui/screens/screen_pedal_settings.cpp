// TIG Rotator Controller - Pedal Settings Screen
// POST mockup #17: compact top card + two status rows + warn strip

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include <Arduino.h>
#include "../../control/control.h"

static lv_obj_t* pedalToggle = nullptr;
static lv_obj_t* pedalToggleLbl = nullptr;
static lv_obj_t* gpioVal = nullptr;
static lv_obj_t* adsVal = nullptr;

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static void set_value(lv_obj_t* obj, const char* text, lv_color_t color) {
  if (!obj) return;
  lv_label_set_text(obj, text);
  lv_obj_set_style_text_color(obj, color, 0);
}

static void update_toggle() {
  if (!pedalToggle || !pedalToggleLbl) return;
  const bool enabled = speed_get_pedal_enabled();
  if (enabled) {
    ui_style_post_ok(pedalToggle);
    lv_obj_set_style_radius(pedalToggle, 14, 0);
  } else {
    lv_obj_set_style_bg_color(pedalToggle, COL_TOGGLE_OFF, 0);
    lv_obj_set_style_border_color(pedalToggle, COL_BORDER_ROW, 0);
    lv_obj_set_style_border_width(pedalToggle, 1, 0);
  }
  lv_obj_set_clickable(pedalToggle, true);
  if (control_get_state() != STATE_IDLE || (!enabled && digitalRead(PIN_PEDAL_SW) == LOW))
    lv_obj_add_state(pedalToggle, LV_STATE_DISABLED);
  else
    lv_obj_remove_state(pedalToggle, LV_STATE_DISABLED);
  lv_label_set_text(pedalToggleLbl, enabled ? "ON" : "OFF");
  lv_obj_set_style_text_color(pedalToggleLbl, enabled ? COL_GREEN : COL_TEXT_DIM, 0);
}

static void pedal_toggle_cb(lv_event_t* e) {
  (void)e;
  if (control_get_state() != STATE_IDLE) return;
  const bool enabled = !speed_get_pedal_enabled();
  if (enabled && digitalRead(PIN_PEDAL_SW) == LOW) return;
  speed_set_pedal_enabled(enabled);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.pedal_enabled = enabled;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();

  screen_pedal_settings_update();
}

void screen_pedal_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PEDAL_SETTINGS];
  lv_obj_clean(screen);
  ui_create_header(screen, "Foot pedal", "OPERATOR INPUT", nullptr);
  lv_obj_t* mode = ui_create_post_card(screen, 24, 94, 752, 76);
  ui_create_text(mode, 18, 24, 420, "Pedal start / stop control", FONT_LARGE, COL_TEXT);
  pedalToggle =
      ui_create_btn(mode, 560, 10, 174, 56, "OFF", FONT_BTN, UI_BTN_NORMAL, pedal_toggle_cb, nullptr);
  pedalToggleLbl = lv_obj_get_child(pedalToggle, 0);
  lv_obj_t* gpio = ui_create_post_card(screen, 24, 186, 752, 76);
  ui_create_text(gpio, 18, 24, 360, "Start switch / GPIO33", FONT_LARGE, COL_TEXT);
  gpioVal = ui_create_text(gpio, 420, 26, 314, "", FONT_SUBTITLE, COL_TEXT);
  lv_obj_set_style_text_align(gpioVal, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t* adc = ui_create_post_card(screen, 24, 278, 752, 76);
  ui_create_text(adc, 18, 24, 360, "Analog speed input", FONT_LARGE, COL_TEXT);
  adsVal = ui_create_text(adc, 420, 26, 314, "", FONT_SUBTITLE, COL_TEXT);
  lv_obj_set_style_text_align(adsVal, LV_TEXT_ALIGN_RIGHT, 0);
  ui_create_text(screen, 24, 368, 752, "Release the pedal before enabling control.", FONT_SUBTITLE,
                 COL_TEXT_DIM);
  ui_create_btn(screen, 24, 408, 152, 56, "<  BACK", FONT_BTN, UI_BTN_NORMAL, back_cb, nullptr);
  screen_pedal_settings_update();
}

void screen_pedal_settings_invalidate_widgets() {
  pedalToggle = nullptr;
  pedalToggleLbl = nullptr;
  gpioVal = nullptr;
  adsVal = nullptr;
}

void screen_pedal_settings_update() {
  if (!gpioVal) return;

  update_toggle();

  const bool pressed = (digitalRead(PIN_PEDAL_SW) == LOW);
  set_value(gpioVal, pressed ? "LOW PRESSED" : "HIGH OPEN", pressed ? COL_ACCENT : COL_GREEN);

  if (speed_get_pedal_enabled() && !speed_pedal_input_healthy()) {
    set_value(adsVal, "INPUT FAULT / START BLOCKED", COL_RED);
  } else if (speed_ads1115_pedal_present()) {
    set_value(adsVal, speed_pedal_analog_available() ? "ACTIVE" : "PRESENT", COL_GREEN);
  } else {
    set_value(adsVal, "ADS1115 not detected", COL_TEXT_DIM);
  }
}
