// TIG Rotator Controller - Diagnostics Screen
// Live GPIO, safety, and motion status for field troubleshooting
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../safety/safety.h"
#include <Arduino.h>

static lv_obj_t* stateVal = nullptr;
static lv_obj_t* faultVal = nullptr;
static lv_obj_t* inhibitVal = nullptr;
static lv_obj_t* estopVal = nullptr;
static lv_obj_t* almVal = nullptr;
static lv_obj_t* dirSwVal = nullptr;
static lv_obj_t* pedalSwVal = nullptr;
static lv_obj_t* enaVal = nullptr;
static lv_obj_t* dirPinVal = nullptr;
static lv_obj_t* targetRpmVal = nullptr;
static lv_obj_t* actualRpmVal = nullptr;
static lv_obj_t* directionVal = nullptr;
static lv_obj_t* overrideVal = nullptr;
static lv_obj_t* pedalArmVal = nullptr;
static lv_obj_t* adsVal = nullptr;
static lv_obj_t* analogVal = nullptr;

static void back_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_SETTINGS);
}

static lv_obj_t* create_panel(lv_obj_t* parent, int x, int y, int w, int h, const char* title) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_size(panel, w, h);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_style_bg_color(panel, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(panel, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, RADIUS_ROW, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* titleLbl = lv_label_create(panel);
  lv_label_set_text(titleLbl, title);
  lv_obj_set_style_text_font(titleLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(titleLbl, COL_ACCENT, 0);
  lv_obj_set_pos(titleLbl, 12, 10);
  return panel;
}

static lv_obj_t* create_status_row(lv_obj_t* parent, int y, const char* key) {
  lv_obj_t* keyLbl = lv_label_create(parent);
  lv_label_set_text(keyLbl, key);
  lv_obj_set_style_text_font(keyLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(keyLbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(keyLbl, 12, y);
  lv_obj_set_width(keyLbl, 154);
  lv_label_set_long_mode(keyLbl, LV_LABEL_LONG_MODE_CLIP);

  lv_obj_t* valueLbl = lv_label_create(parent);
  lv_label_set_text(valueLbl, "-");
  lv_obj_set_style_text_font(valueLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(valueLbl, COL_TEXT, 0);
  lv_obj_set_pos(valueLbl, 176, y);
  lv_obj_set_width(valueLbl, 178);
  lv_label_set_long_mode(valueLbl, LV_LABEL_LONG_MODE_CLIP);
  return valueLbl;
}

static void set_value(lv_obj_t* obj, const char* text, lv_color_t color) {
  if (!obj) return;
  lv_label_set_text(obj, text);
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

  ui_create_settings_header(screen, "DIAGNOSTICS");

  lv_obj_t* gpioPanel = create_panel(screen, 16, 42, 374, 344, "GPIO INPUTS");
  lv_obj_t* runtimePanel = create_panel(screen, 410, 42, 374, 344, "RUNTIME STATUS");

  int y = 44;
  estopVal = create_status_row(gpioPanel, y, "ESTOP GPIO34"); y += 30;
  almVal = create_status_row(gpioPanel, y, "DM542T ALM GPIO32"); y += 30;
  dirSwVal = create_status_row(gpioPanel, y, "DIR SW GPIO29"); y += 30;
  pedalSwVal = create_status_row(gpioPanel, y, "PEDAL SW GPIO33"); y += 30;
  enaVal = create_status_row(gpioPanel, y, "ENA GPIO52"); y += 30;
  dirPinVal = create_status_row(gpioPanel, y, "DIR GPIO51"); y += 30;
  adsVal = create_status_row(gpioPanel, y, "ADS1115 0x48"); y += 30;
  analogVal = create_status_row(gpioPanel, y, "PEDAL ANALOG");

  y = 44;
  stateVal = create_status_row(runtimePanel, y, "STATE"); y += 30;
  faultVal = create_status_row(runtimePanel, y, "FAULT"); y += 30;
  inhibitVal = create_status_row(runtimePanel, y, "MOTION BLOCK"); y += 30;
  targetRpmVal = create_status_row(runtimePanel, y, "TARGET RPM"); y += 30;
  actualRpmVal = create_status_row(runtimePanel, y, "ACTUAL RPM"); y += 30;
  directionVal = create_status_row(runtimePanel, y, "DIRECTION"); y += 30;
  overrideVal = create_status_row(runtimePanel, y, "PROGRAM DIR"); y += 30;
  pedalArmVal = create_status_row(runtimePanel, y, "PEDAL ARMED");

  lv_obj_t* hint = lv_label_create(screen);
  lv_label_set_text(hint, "Use this page when START is blocked: ESTOP/ALM must be OK and MOTION BLOCK must be NO.");
  lv_obj_set_style_text_font(hint, FONT_BODY, 0);
  lv_obj_set_style_text_color(hint, COL_TEXT_DIM, 0);
  lv_obj_set_pos(hint, 18, 397);
  lv_obj_set_width(hint, 764);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_CLIP);

  ui_create_btn(screen, 16, SET_FOOTER_Y, 170, SET_FOOTER_H, "BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb, nullptr);
  screen_diagnostics_update();
}

void screen_diagnostics_invalidate_widgets() {
  stateVal = nullptr;
  faultVal = nullptr;
  inhibitVal = nullptr;
  estopVal = nullptr;
  almVal = nullptr;
  dirSwVal = nullptr;
  pedalSwVal = nullptr;
  enaVal = nullptr;
  dirPinVal = nullptr;
  targetRpmVal = nullptr;
  actualRpmVal = nullptr;
  directionVal = nullptr;
  overrideVal = nullptr;
  pedalArmVal = nullptr;
  adsVal = nullptr;
  analogVal = nullptr;
}

void screen_diagnostics_update() {
  if (!stateVal) return;

  set_value(stateVal, control_get_state_string(), control_get_state() == STATE_IDLE ? COL_GREEN : COL_ACCENT);

  FaultReason reason = safety_get_fault_reason();
  set_value(faultVal, safety_fault_reason_name(reason), reason == FAULT_NONE ? COL_GREEN : COL_RED);
  set_value(inhibitVal, safety_inhibit_motion() ? "YES" : "NO", safety_inhibit_motion() ? COL_RED : COL_GREEN);

  set_pin_value(estopVal, digitalRead(PIN_ESTOP), "HIGH OK", "LOW PRESSED", true);
  set_pin_value(almVal, digitalRead(PIN_DRIVER_ALM), "HIGH OK", "LOW FAULT", true);
  set_pin_value(dirSwVal, digitalRead(PIN_DIR_SWITCH), "HIGH CW", "LOW CCW", false);
  set_pin_value(pedalSwVal, digitalRead(PIN_PEDAL_SW), "HIGH OPEN", "LOW PRESSED", false);
  set_pin_value(enaVal, digitalRead(PIN_ENA), "HIGH DISABLED", "LOW ENABLED", false);
  set_pin_value(dirPinVal, digitalRead(PIN_DIR), "HIGH CW", "LOW CCW", false);

  set_value(adsVal, speed_ads1115_pedal_present() ? "PRESENT" : "NOT FOUND",
            speed_ads1115_pedal_present() ? COL_GREEN : COL_TEXT_DIM);
  set_value(analogVal, speed_pedal_analog_available() ? "ACTIVE" : "OFF",
            speed_pedal_analog_available() ? COL_GREEN : COL_TEXT_DIM);
  set_value(directionVal, speed_get_direction() == DIR_CW ? "CW" : "CCW", COL_ACCENT);
  set_value(overrideVal, speed_program_direction_override_active() ? "ACTIVE" : "OFF",
            speed_program_direction_override_active() ? COL_ACCENT : COL_TEXT_DIM);
  set_value(pedalArmVal, speed_get_pedal_enabled() ? "YES" : "NO",
            speed_get_pedal_enabled() ? COL_GREEN : COL_TEXT_DIM);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.3f", (double)speed_get_target_rpm());
  set_value(targetRpmVal, buf, COL_TEXT);
  snprintf(buf, sizeof(buf), "%.3f", (double)speed_get_actual_rpm());
  set_value(actualRpmVal, buf, COL_TEXT);
}
