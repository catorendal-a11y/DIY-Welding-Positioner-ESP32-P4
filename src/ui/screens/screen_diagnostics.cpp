// TIG Rotator Controller - Diagnostics Screen
// POST mockup #18: twin cards, compact GPIO + runtime, single event strip, BACK only

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../event_log.h"
#include "../../motor/speed.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

static lv_obj_t* estopVal = nullptr;
static lv_obj_t* almVal = nullptr;
static lv_obj_t* dirSwVal = nullptr;
static lv_obj_t* pedalSwVal = nullptr;
static lv_obj_t* stateVal = nullptr;
static lv_obj_t* targetRpmVal = nullptr;
static lv_obj_t* actualRpmVal = nullptr;
static lv_obj_t* enaVal = nullptr;
static lv_obj_t* eventStripLabel = nullptr;
static uint32_t lastEventLogVersion = UINT32_MAX;

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static lv_obj_t* add_gpio_row(lv_obj_t* panel, int y, const char* key) {
  lv_obj_t* keyLbl = lv_label_create(panel);
  lv_label_set_text(keyLbl, key);
  lv_obj_set_style_text_font(keyLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(keyLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(keyLbl, 12, y);
  lv_obj_set_width(keyLbl, 200);
  lv_label_set_long_mode(keyLbl, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* valueLbl = lv_label_create(panel);
  lv_label_set_text(valueLbl, "-");
  lv_obj_set_style_text_font(valueLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(valueLbl, COL_TEXT, 0);
  lv_obj_set_pos(valueLbl, 204, y);
  lv_obj_set_width(valueLbl, 144);
  lv_label_set_long_mode(valueLbl, LV_LABEL_LONG_MODE_CLIP);
  return valueLbl;
}

static lv_obj_t* add_rt_row(lv_obj_t* panel, int y, const char* key) {
  return add_gpio_row(panel, y, key);
}

static void set_value(lv_obj_t* obj, const char* text, lv_color_t color) {
  if (!obj) return;
  const char* current = lv_label_get_text(obj);
  if (current == nullptr || strcmp(current, text) != 0) {
    lv_label_set_text(obj, text);
  }
  lv_obj_set_style_text_color(obj, color, 0);
}

static void set_pin_value(lv_obj_t* obj, int pinState, const char* highText, const char* lowText, bool lowIsFault) {
  const bool low = (pinState == LOW);
  const char* text = low ? lowText : highText;
  lv_color_t color = (low && lowIsFault) ? COL_RED : COL_GREEN;
  if (low && !lowIsFault) color = COL_ACCENT;
  set_value(obj, text, color);
}

void screen_diagnostics_create() {
  lv_obj_t* screen = screenRoots[SCREEN_DIAGNOSTICS];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_settings_header(screen, "DIAGNOSTICS", "LIVE", COL_GREEN);

  lv_obj_t* gpioPanel = ui_create_post_card(screen, 20, 60, 360, 244);
  lv_obj_remove_flag(gpioPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* gpioTitle = lv_label_create(gpioPanel);
  lv_label_set_text(gpioTitle, "GPIO INPUTS");
  lv_obj_set_style_text_font(gpioTitle, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(gpioTitle, COL_ACCENT, 0);
  lv_obj_set_pos(gpioTitle, 12, 10);

  int gy = 38;
  estopVal = add_gpio_row(gpioPanel, gy, "ESTOP"); gy += 32;
  almVal = add_gpio_row(gpioPanel, gy, "DRIVER ALM"); gy += 32;
  dirSwVal = add_gpio_row(gpioPanel, gy, "DIR SWITCH"); gy += 32;
  pedalSwVal = add_gpio_row(gpioPanel, gy, "PEDAL SW");

  lv_obj_t* rtPanel = ui_create_post_card(screen, 420, 60, 360, 244);
  lv_obj_remove_flag(rtPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* rtTitle = lv_label_create(rtPanel);
  lv_label_set_text(rtTitle, "RUNTIME STATUS");
  lv_obj_set_style_text_font(rtTitle, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rtTitle, COL_ACCENT, 0);
  lv_obj_set_pos(rtTitle, 12, 10);

  int ry = 38;
  stateVal = add_rt_row(rtPanel, ry, "STATE"); ry += 32;
  targetRpmVal = add_rt_row(rtPanel, ry, "TARGET RPM"); ry += 32;
  actualRpmVal = add_rt_row(rtPanel, ry, "ACTUAL RPM"); ry += 32;
  enaVal = add_rt_row(rtPanel, ry, "ENA");

  lv_obj_t* eventPanel = ui_create_post_card(screen, 20, 326, 760, 46);
  lv_obj_remove_flag(eventPanel, LV_OBJ_FLAG_SCROLLABLE);
  eventStripLabel = lv_label_create(eventPanel);
  lv_label_set_text(eventStripLabel, "-");
  lv_obj_set_style_text_font(eventStripLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(eventStripLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(eventStripLabel, 12, 14);
  lv_obj_set_width(eventStripLabel, 736);
  lv_label_set_long_mode(eventStripLabel, LV_LABEL_LONG_MODE_CLIP);

  ui_create_btn(screen, 20, SET_FOOTER_Y, 180, SET_FOOTER_H, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb,
                nullptr);
  screen_diagnostics_update();
}

void screen_diagnostics_invalidate_widgets() {
  estopVal = nullptr;
  almVal = nullptr;
  dirSwVal = nullptr;
  pedalSwVal = nullptr;
  stateVal = nullptr;
  targetRpmVal = nullptr;
  actualRpmVal = nullptr;
  enaVal = nullptr;
  eventStripLabel = nullptr;
  lastEventLogVersion = UINT32_MAX;
}

void screen_diagnostics_update() {
  if (!stateVal) return;

  SystemState state = control_get_state();

  set_value(stateVal, control_state_name(state), state == STATE_IDLE ? COL_GREEN : COL_ACCENT);

  set_pin_value(estopVal, digitalRead(PIN_ESTOP), "HIGH OK", "LOW PRESSED", true);
  set_pin_value(almVal, digitalRead(PIN_DRIVER_ALM), "HIGH OK", "LOW FAULT", true);
  set_pin_value(dirSwVal, digitalRead(PIN_DIR_SWITCH), "HIGH CW", "LOW CCW", false);
  set_pin_value(pedalSwVal, digitalRead(PIN_PEDAL_SW), "HIGH OPEN", "LOW PRESSED", false);
  set_pin_value(enaVal, digitalRead(PIN_ENA), "HIGH DISABLED", "LOW ENABLED", false);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.3f", (double)speed_get_target_rpm());
  set_value(targetRpmVal, buf, COL_TEXT);
  snprintf(buf, sizeof(buf), "%.3f", (double)speed_get_actual_rpm());
  set_value(actualRpmVal, buf, COL_TEXT);

  uint32_t eventVersion = event_log_version();
  if (eventVersion != lastEventLogVersion && eventStripLabel) {
    lastEventLogVersion = eventVersion;
    EventLogEntry ev[4];
    size_t n = event_log_snapshot(ev, 4);
    char line[220];
    size_t pos = 0;
    line[0] = '\0';
    for (size_t i = 0; i < n && i < 3; i++) {
      if (i > 0) {
        int w = snprintf(line + pos, sizeof(line) - pos, "  /  ");
        if (w > 0) pos += (size_t)w;
      }
      uint32_t sec = ev[i].ms / 1000u;
      int w = snprintf(line + pos, sizeof(line) - pos, "%02lu:%02lu %s",
                       (unsigned long)((sec / 60u) % 100u),
                       (unsigned long)(sec % 60u),
                       ev[i].text);
      if (w > 0) pos += (size_t)w;
      if (pos >= sizeof(line)) break;
    }
    if (line[0] == '\0') {
      snprintf(line, sizeof(line), "-");
    }
    lv_label_set_text(eventStripLabel, line);
  }
}
