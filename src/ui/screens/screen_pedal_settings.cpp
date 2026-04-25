// TIG Rotator Controller - Pedal Settings Screen
// Foot pedal arm/disarm and live pedal input status
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include <Arduino.h>

static lv_obj_t* pedalToggle = nullptr;
static lv_obj_t* pedalToggleLbl = nullptr;
static lv_obj_t* gpioVal = nullptr;
static lv_obj_t* armedVal = nullptr;
static lv_obj_t* adsVal = nullptr;
static lv_obj_t* analogVal = nullptr;
static lv_obj_t* savedVal = nullptr;

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static void set_value(lv_obj_t* obj, const char* text, lv_color_t color) {
  if (!obj) return;
  lv_label_set_text(obj, text);
  lv_obj_set_style_text_color(obj, color, 0);
}

static lv_obj_t* create_status_row(lv_obj_t* parent, int y, const char* key) {
  lv_obj_t* keyLbl = lv_label_create(parent);
  lv_label_set_text(keyLbl, key);
  lv_obj_set_style_text_font(keyLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(keyLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(keyLbl, 18, y);
  lv_obj_set_width(keyLbl, 260);
  lv_label_set_long_mode(keyLbl, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* valueLbl = lv_label_create(parent);
  lv_label_set_text(valueLbl, "-");
  lv_obj_set_style_text_font(valueLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(valueLbl, COL_TEXT, 0);
  lv_obj_set_pos(valueLbl, 320, y - 2);
  lv_obj_set_width(valueLbl, 420);
  lv_label_set_long_mode(valueLbl, LV_LABEL_LONG_MODE_CLIP);
  return valueLbl;
}

static void update_toggle() {
  if (!pedalToggle || !pedalToggleLbl) return;
  const bool enabled = speed_get_pedal_enabled();
  lv_obj_set_style_bg_color(pedalToggle, enabled ? COL_GREEN : COL_TOGGLE_OFF, 0);
  lv_label_set_text(pedalToggleLbl, enabled ? "ON" : "OFF");
}

static void pedal_toggle_cb(lv_event_t* e) {
  (void)e;
  const bool enabled = !speed_get_pedal_enabled();
  speed_set_pedal_enabled(enabled);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.pedal_enabled = enabled;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();

  set_value(savedVal, "Saved", COL_GREEN);
  screen_pedal_settings_update();
}

void screen_pedal_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PEDAL_SETTINGS];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_settings_header(screen, "PEDAL SETTINGS");

  lv_obj_t* panel = lv_obj_create(screen);
  lv_obj_set_size(panel, 768, 300);
  lv_obj_set_pos(panel, 16, 48);
  lv_obj_set_style_bg_color(panel, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(panel, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, RADIUS_ROW, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* title = lv_label_create(panel);
  lv_label_set_text(title, "FOOT PEDAL START/STOP");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 18, 14);

  pedalToggle = lv_obj_create(panel);
  lv_obj_set_size(pedalToggle, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_set_pos(pedalToggle, 648, 10);
  lv_obj_set_style_radius(pedalToggle, SET_TOGGLE_R, 0);
  lv_obj_set_style_border_width(pedalToggle, 0, 0);
  lv_obj_set_style_pad_all(pedalToggle, 0, 0);
  lv_obj_remove_flag(pedalToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pedalToggle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pedalToggle, pedal_toggle_cb, LV_EVENT_CLICKED, nullptr);

  pedalToggleLbl = lv_label_create(pedalToggle);
  lv_obj_set_style_text_font(pedalToggleLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(pedalToggleLbl, COL_TEXT_WHITE, 0);
  lv_obj_center(pedalToggleLbl);

  int y = 72;
  armedVal = create_status_row(panel, y, "Pedal armed"); y += 42;
  gpioVal = create_status_row(panel, y, "GPIO33 switch"); y += 42;
  adsVal = create_status_row(panel, y, "ADS1115 analog ADC"); y += 42;
  analogVal = create_status_row(panel, y, "Analog speed source"); y += 42;
  savedVal = create_status_row(panel, y, "Storage");

  lv_obj_t* note = lv_label_create(screen);
  lv_label_set_text(note,
                    "GPIO33 works alone as a start/stop pedal. ADS1115 is optional and only adds analog speed input.");
  lv_obj_set_style_text_font(note, FONT_BODY, 0);
  lv_obj_set_style_text_color(note, COL_TEXT_DIM, 0);
  lv_obj_set_pos(note, 18, 366);
  lv_obj_set_width(note, 764);
  lv_label_set_long_mode(note, LV_LABEL_LONG_MODE_WRAP);

  ui_create_btn(screen, 16, SET_FOOTER_Y, 170, SET_FOOTER_H, "BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb, nullptr);
  screen_pedal_settings_update();
}

void screen_pedal_settings_invalidate_widgets() {
  pedalToggle = nullptr;
  pedalToggleLbl = nullptr;
  gpioVal = nullptr;
  armedVal = nullptr;
  adsVal = nullptr;
  analogVal = nullptr;
  savedVal = nullptr;
}

void screen_pedal_settings_update() {
  if (!armedVal) return;

  update_toggle();
  set_value(armedVal, speed_get_pedal_enabled() ? "YES" : "NO",
            speed_get_pedal_enabled() ? COL_GREEN : COL_TEXT_DIM);

  const bool pressed = (digitalRead(PIN_PEDAL_SW) == LOW);
  set_value(gpioVal, pressed ? "LOW PRESSED" : "HIGH OPEN", pressed ? COL_ACCENT : COL_GREEN);

  set_value(adsVal, speed_ads1115_pedal_present() ? "PRESENT" : "NOT FOUND",
            speed_ads1115_pedal_present() ? COL_GREEN : COL_TEXT_DIM);
  set_value(analogVal, speed_pedal_analog_available() ? "ACTIVE" : "OFF",
            speed_pedal_analog_available() ? COL_GREEN : COL_TEXT_DIM);

  if (savedVal && lv_label_get_text(savedVal)[0] == '-') {
    set_value(savedVal, "Ready", COL_TEXT_DIM);
  }
}
