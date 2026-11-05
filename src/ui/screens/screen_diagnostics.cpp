#include "../../motor/motor.h"
// TIG Rotator Controller - Diagnostics Screen
// POST mockup #18: twin cards, compact GPIO + runtime, single event strip, BACK only

#include "../screens.h"
#include "../theme.h"
#include "../value_binding.h"
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
static lv_obj_t* driverInfoLabel = nullptr;
static lv_obj_t* eventStripLabel = nullptr;
static uint32_t lastEventLogVersion = UINT32_MAX;
enum DiagnosticRow { ESTOP, ALARM, DIRECTION, PEDAL, STATE, TARGET_RPM, ESTIMATED_RPM, ENABLE, ROW_COUNT };
static UiTextBinding<24> values[ROW_COUNT];
static UiTextBinding<80> driverInfo;
static UiTextBinding<220> eventText;

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static lv_obj_t* add_gpio_row(lv_obj_t* panel, int y, const char* key) {
  lv_obj_t* keyLbl = lv_label_create(panel);
  lv_label_set_text(keyLbl, key);
  lv_obj_set_style_text_font(keyLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(keyLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(keyLbl, 12, y);
  lv_obj_set_width(keyLbl, 200);
  lv_label_set_long_mode(keyLbl, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* valueLbl = lv_label_create(panel);
  lv_label_set_text(valueLbl, "-");
  lv_obj_set_style_text_font(valueLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(valueLbl, COL_TEXT, 0);
  lv_obj_set_pos(valueLbl, 204, y);
  lv_obj_set_width(valueLbl, 144);
  lv_label_set_long_mode(valueLbl, LV_LABEL_LONG_MODE_CLIP);
  return valueLbl;
}

static lv_obj_t* add_rt_row(lv_obj_t* panel, int y, const char* key) { return add_gpio_row(panel, y, key); }

template <size_t Capacity>
static void set_value(UiTextBinding<Capacity>& binding, lv_obj_t* obj, const char* text, lv_color_t color) {
  if (!obj) return;
  binding.set(text);
  if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), color))
    lv_obj_set_style_text_color(obj, color, 0);
}

static void set_pin_value(DiagnosticRow row, lv_obj_t* obj, int pinState, const char* highText, const char* lowText,
                          bool lowIsFault) {
  const bool low = (pinState == LOW);
  const char* text = low ? lowText : highText;
  lv_color_t color = (low && lowIsFault) ? COL_RED : COL_GREEN;
  if (low && !lowIsFault) color = COL_TEXT;
  set_value(values[row], obj, text, color);
}

void screen_diagnostics_create() {
  screen_diagnostics_invalidate_widgets();
  lv_obj_t* screen = screenRoots[SCREEN_DIAGNOSTICS];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_header(screen, "Diagnostics", "", nullptr);

  lv_obj_t* gpioPanel = ui_create_post_card(screen, 24, 94, 368, 224);
  lv_obj_set_scrollable(gpioPanel, false);

  lv_obj_t* gpioTitle = lv_label_create(gpioPanel);
  lv_label_set_text(gpioTitle, "GPIO inputs");
  lv_obj_set_style_text_font(gpioTitle, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(gpioTitle, COL_TEXT, 0);
  lv_obj_set_pos(gpioTitle, 12, 10);

  int gy = 38;
  estopVal = add_gpio_row(gpioPanel, gy, "E-STOP");
  gy += 32;
  almVal = add_gpio_row(gpioPanel, gy, "Driver alarm");
  gy += 32;
  dirSwVal = add_gpio_row(gpioPanel, gy, "Direction switch");
  gy += 32;
  pedalSwVal = add_gpio_row(gpioPanel, gy, "Pedal switch");

  lv_obj_t* rtPanel = ui_create_post_card(screen, 408, 94, 368, 224);
  lv_obj_set_scrollable(rtPanel, false);

  lv_obj_t* rtTitle = lv_label_create(rtPanel);
  lv_label_set_text(rtTitle, "Runtime status");
  lv_obj_set_style_text_font(rtTitle, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(rtTitle, COL_TEXT, 0);
  lv_obj_set_pos(rtTitle, 12, 10);

  int ry = 38;
  stateVal = add_rt_row(rtPanel, ry, "State");
  ry += 32;
  targetRpmVal = add_rt_row(rtPanel, ry, "Target RPM");
  ry += 32;
  actualRpmVal = add_rt_row(rtPanel, ry, "Estimated RPM");
  ry += 32;
  enaVal = add_rt_row(rtPanel, ry, "Enable output");

  driverInfoLabel = lv_label_create(rtPanel);
  lv_obj_set_pos(driverInfoLabel, 12, 180); lv_obj_set_width(driverInfoLabel, 344);
  lv_obj_set_style_text_font(driverInfoLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(driverInfoLabel, COL_TEXT_DIM, 0);
  lv_label_set_long_mode(driverInfoLabel, LV_LABEL_LONG_MODE_DOTS);

  lv_obj_t* eventPanel = ui_create_post_card(screen, 24, 334, 752, 48);
  lv_obj_set_scrollable(eventPanel, false);
  eventStripLabel = lv_label_create(eventPanel);
  lv_label_set_text(eventStripLabel, "-");
  lv_obj_set_style_text_font(eventStripLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(eventStripLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(eventStripLabel, 12, 14);
  lv_obj_set_width(eventStripLabel, 736);
  lv_label_set_long_mode(eventStripLabel, LV_LABEL_LONG_MODE_CLIP);

  ui_create_btn(screen, 24, SET_FOOTER_Y, 152, SET_FOOTER_H, UI_LABEL_BACK, FONT_SUBTITLE, UI_BTN_NORMAL, back_cb,
                nullptr);
  lv_obj_t* rows[] = {estopVal, almVal, dirSwVal, pedalSwVal, stateVal, targetRpmVal, actualRpmVal, enaVal};
  for (size_t i = 0; i < ROW_COUNT; ++i) values[i].bind(rows[i]);
  driverInfo.bind(driverInfoLabel); eventText.bind(eventStripLabel);
  screen_diagnostics_update();
}

void screen_diagnostics_invalidate_widgets() {
  for (auto& value : values) value.reset();
  driverInfo.reset(); eventText.reset();
  estopVal = nullptr;
  almVal = nullptr;
  dirSwVal = nullptr;
  pedalSwVal = nullptr;
  stateVal = nullptr;
  targetRpmVal = nullptr;
  actualRpmVal = nullptr;
  enaVal = nullptr;
  eventStripLabel = nullptr; driverInfoLabel = nullptr;
  lastEventLogVersion = UINT32_MAX;
}

void screen_diagnostics_update() {
  if (!stateVal) return;

  const auto& view = ui_control_view();
  const bool fresh = ui_control_fresh();
  const SystemState state = ui_control_state();

  set_value(values[STATE], stateVal, fresh ? control_state_name(state) : "Status stale",
            !fresh || state == STATE_ESTOP ? COL_RED : state == STATE_IDLE ? COL_GREEN : COL_TEXT);

  set_pin_value(ESTOP, estopVal, digitalRead(PIN_ESTOP), "High / OK", "Low / pressed", true);
  set_pin_value(ALARM, almVal, digitalRead(PIN_DRIVER_ALM), "High / OK", "Low / fault", true);
  set_pin_value(DIRECTION, dirSwVal, digitalRead(PIN_DIR_SWITCH), "High / CW", "Low / CCW", false);
  set_pin_value(PEDAL, pedalSwVal, digitalRead(PIN_PEDAL_SW), "High / open", "Low / pressed", false);
  set_pin_value(ENABLE, enaVal, digitalRead(PIN_ENA), "High / disabled", "Low / enabled", false);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.3f", (double)view.target_rpm);
  set_value(values[TARGET_RPM], targetRpmVal, fresh ? buf : "---", fresh ? COL_TEXT : COL_TEXT_DIM);
  snprintf(buf, sizeof(buf), "%.3f", (double)view.estimated_rpm);
  set_value(values[ESTIMATED_RPM], actualRpmVal, fresh ? buf : "---", fresh ? COL_TEXT : COL_TEXT_DIM);

  MotorDriverInfo info;
  if (driverInfoLabel && motor_read_driver_info(info)) {
    char timing[80];
    snprintf(timing, sizeof(timing), "%s / DIR %lu + %lu us", info.name,
             (unsigned long)info.direction_before_us, (unsigned long)info.direction_after_us);
    set_value(driverInfo, driverInfoLabel, timing, COL_TEXT_DIM);
  }

  uint32_t eventVersion = event_log_version();
  if (eventVersion != lastEventLogVersion && eventStripLabel) {
    EventLogEntry ev[4];
    size_t n = 0;
    if (!event_log_try_snapshot(ev, 4, &n, &lastEventLogVersion)) return;
    char line[220];
    size_t pos = 0;
    line[0] = '\0';
    const uint32_t dropped = event_log_dropped();
    if (dropped) {
      pos = static_cast<size_t>(snprintf(line, sizeof(line), "Dropped %lu | ", (unsigned long)dropped));
    }
    for (size_t i = 0; i < n && i < 3; i++) {
      if (i > 0) {
        int w = snprintf(line + pos, sizeof(line) - pos, "  /  ");
        if (w > 0) pos += (size_t)w;
      }
      uint32_t sec = ev[i].ms / 1000u;
      int w = snprintf(line + pos, sizeof(line) - pos, "%02lu:%02lu %s", (unsigned long)((sec / 60u) % 100u),
                       (unsigned long)(sec % 60u), ev[i].text);
      if (w > 0) pos += (size_t)w;
      if (pos >= sizeof(line)) break;
    }
    if (line[0] == '\0') {
      snprintf(line, sizeof(line), "-");
    }
    eventText.set(line);
  }
}
