// TIG Rotator Controller - Pedal Settings Screen
// POST mockup #17: compact top card + two status rows + warn strip

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../storage/storage.h"
#include <Arduino.h>

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
  lv_label_set_text(pedalToggleLbl, enabled ? "ON" : "OFF");
  lv_obj_set_style_text_color(pedalToggleLbl, enabled ? COL_GREEN : COL_TEXT_DIM, 0);
}

static void pedal_toggle_cb(lv_event_t* e) {
  (void)e;
  const bool enabled = !speed_get_pedal_enabled();
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
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_settings_header(screen, "PEDAL SETTINGS", "", COL_TEXT_DIM);

  lv_obj_t* topCard = ui_create_post_card(screen, 20, 62, 760, 78);
  lv_obj_remove_flag(topCard, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* title = lv_label_create(topCard);
  lv_label_set_text(title, "FOOT PEDAL START/STOP");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 22, 28);

  pedalToggle = lv_obj_create(topCard);
  lv_obj_set_size(pedalToggle, 92, 28);
  lv_obj_set_pos(pedalToggle, 630, 24);
  lv_obj_set_style_radius(pedalToggle, 14, 0);
  lv_obj_set_style_pad_all(pedalToggle, 0, 0);
  lv_obj_remove_flag(pedalToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pedalToggle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(pedalToggle, pedal_toggle_cb, LV_EVENT_CLICKED, nullptr);

  pedalToggleLbl = lv_label_create(pedalToggle);
  lv_obj_set_style_text_font(pedalToggleLbl, FONT_SMALL, 0);
  lv_obj_center(pedalToggleLbl);

  lv_obj_t* rowGpio = lv_obj_create(screen);
  lv_obj_set_size(rowGpio, 760, 62);
  lv_obj_set_pos(rowGpio, 20, 164);
  ui_style_post_row(rowGpio);
  lv_obj_remove_flag(rowGpio, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* k1 = lv_label_create(rowGpio);
  lv_label_set_text(k1, "SWITCH GPIO33");
  lv_obj_set_style_text_font(k1, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(k1, COL_TEXT_DIM, 0);
  lv_obj_align(k1, LV_ALIGN_LEFT_MID, 18, 0);

  gpioVal = lv_label_create(rowGpio);
  lv_label_set_text(gpioVal, "-");
  lv_obj_set_style_text_font(gpioVal, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(gpioVal, COL_TEXT, 0);
  lv_obj_align(gpioVal, LV_ALIGN_RIGHT_MID, -18, 0);
  lv_obj_set_width(gpioVal, 320);
  lv_label_set_long_mode(gpioVal, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(gpioVal, LV_TEXT_ALIGN_RIGHT, 0);

  lv_obj_t* rowAds = lv_obj_create(screen);
  lv_obj_set_size(rowAds, 760, 62);
  lv_obj_set_pos(rowAds, 20, 246);
  ui_style_post_row(rowAds);
  lv_obj_remove_flag(rowAds, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* k2 = lv_label_create(rowAds);
  lv_label_set_text(k2, "ANALOG PEDAL");
  lv_obj_set_style_text_font(k2, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(k2, COL_TEXT_DIM, 0);
  lv_obj_align(k2, LV_ALIGN_LEFT_MID, 18, 0);

  adsVal = lv_label_create(rowAds);
  lv_label_set_text(adsVal, "-");
  lv_obj_set_style_text_font(adsVal, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(adsVal, COL_TEXT, 0);
  lv_obj_align(adsVal, LV_ALIGN_RIGHT_MID, -18, 0);
  lv_obj_set_width(adsVal, 420);
  lv_label_set_long_mode(adsVal, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_set_style_text_align(adsVal, LV_TEXT_ALIGN_RIGHT, 0);

  lv_obj_t* noteBar = lv_obj_create(screen);
  lv_obj_set_size(noteBar, 760, 34);
  lv_obj_set_pos(noteBar, 20, 330);
  ui_style_post_warn(noteBar);
  lv_obj_remove_flag(noteBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* note = lv_label_create(noteBar);
  lv_label_set_text(note,
                    "Pedal cannot start if ESTOP, ALM or RPM block is active.");
  lv_obj_set_style_text_font(note, FONT_TINY, 0);
  lv_obj_set_style_text_color(note, COL_YELLOW, 0);
  lv_obj_align(note, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_set_width(note, 720);
  lv_label_set_long_mode(note, LV_LABEL_LONG_MODE_CLIP);

  ui_create_btn(screen, 20, 414, 180, 50, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb, nullptr);
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

  if (speed_ads1115_pedal_present()) {
    set_value(adsVal, speed_pedal_analog_available() ? "ACTIVE" : "PRESENT",
              COL_GREEN);
  } else {
    set_value(adsVal, "ADS1115 optional", COL_TEXT_DIM);
  }
}
