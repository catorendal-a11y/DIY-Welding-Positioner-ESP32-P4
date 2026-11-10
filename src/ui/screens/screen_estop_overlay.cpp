// V5 blocking fault overlay: strong red identity, explicit input and reset state.
#include "../screens.h"
#include "../theme.h"
#include "../../storage/storage.h"
#include "esp_system.h"
#include "../lvgl_hal.h"
#include "../../config.h"
#include "../../safety/safety.h"
#include <cstdio>

static lv_obj_t *overlay = nullptr, *title = nullptr, *reasonLabel = nullptr, *inputLabel = nullptr;
static lv_obj_t *driverLabel = nullptr, *instruction = nullptr, *resetBtn = nullptr;
static lv_obj_t* eraseBtn = nullptr;
static lv_obj_t* resetHint = nullptr;
static lv_obj_t *banner = nullptr, *bannerCaption = nullptr, *bang = nullptr, *inputDot = nullptr, *driverDot = nullptr;
static constexpr uint32_t kBlockedRed = 0xB52C35, kReadyAmber = 0x9A5B00;
static uint32_t erasePressMs = 0;
static constexpr uint32_t kEraseHoldMs = 3000;
static bool visible = false;
static uint32_t lastUpdate = 0;
static void erase_cb(lv_event_t* e);
static void reset_cb(lv_event_t*) {
  if (safety_can_reset_from_overlay()) g_uiResetPending.store(true, std::memory_order_release);
}
static lv_obj_t* panel(lv_obj_t* p, int x, int y, int w, int h, uint32_t color) {
  lv_obj_t* o = ui_create_post_card(p, x, y, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_border_width(o, 0, 0);
  return o;
}
void estop_overlay_create() {
  overlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(overlay);
  lv_obj_set_size(overlay, 800, 480);
  lv_obj_set_pos(overlay, 0, 0);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x201619), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(overlay, false);
  lv_obj_set_clickable(overlay, true);
  lv_obj_set_hidden(overlay, true);
  banner = panel(overlay, 0, 0, 800, 106, kBlockedRed);
  lv_obj_set_style_radius(banner, 0, 0);
  lv_obj_t* icon = panel(banner, 24, 22, 62, 62, 0xFFFFFF);
  bang = ui_create_text(icon, 0, 8, 62, "!", FONT_HUGE, lv_color_hex(kBlockedRed));
  lv_obj_set_style_text_align(bang, LV_TEXT_ALIGN_CENTER, 0);
  bannerCaption = ui_create_text(banner, 108, 19, 660, "MOTION LOCKED", FONT_SUBTITLE, lv_color_hex(0xFFFFFF));
  title = ui_create_text(banner, 108, 48, 668, "EMERGENCY STOP", FONT_XXL, lv_color_hex(0xFFFFFF));
  // The next action leads and the cause explains it: one statement each, never repeated.
  ui_create_text(overlay, 24, 118, 300, "What to do", FONT_NORMAL, lv_color_hex(0xE3CBCD));
  instruction = ui_create_text(overlay, 24, 140, 752, "", FONT_XL, lv_color_hex(0xFFFFFF));
  reasonLabel = ui_create_text(overlay, 24, 178, 752, "", FONT_SUBTITLE, lv_color_hex(0xE3CBCD));
  lv_label_set_long_mode(reasonLabel, LV_LABEL_LONG_MODE_WRAP);
  auto dot = [](lv_obj_t* row) {
    lv_obj_t* o = lv_obj_create(row);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, 14, 14); lv_obj_set_pos(o, 18, 21);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0); lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
  };
  lv_obj_t* input = panel(overlay, 24, 228, 752, 56, 0x342328);
  inputDot = dot(input);
  ui_create_text(input, 46, 16, 320, "Physical E-STOP", FONT_LARGE, lv_color_hex(0xFFFFFF));
  inputLabel = ui_create_text(input, 392, 16, 338, "", FONT_LARGE, lv_color_hex(0xFFACAF));
  lv_obj_set_style_text_align(inputLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t* driver = panel(overlay, 24, 294, 752, 56, 0x342328);
  driverDot = dot(driver);
  ui_create_text(driver, 46, 16, 320, "Driver alarm", FONT_LARGE, lv_color_hex(0xFFFFFF));
  driverLabel = ui_create_text(driver, 392, 16, 338, "", FONT_LARGE, COL_GREEN);
  lv_obj_set_style_text_align(driverLabel, LV_TEXT_ALIGN_RIGHT, 0);
  resetHint = ui_create_text(overlay, 24, 438, 242, "Reset never starts motion.", FONT_NORMAL, lv_color_hex(0xE3CBCD));
  eraseBtn = ui_create_btn(overlay, 24, 408, 248, 56, "Hold 3 s to erase data", FONT_SUBTITLE, UI_BTN_DANGER, nullptr, nullptr);
  for (auto code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(eraseBtn, erase_cb, code, nullptr);
  lv_obj_set_hidden(eraseBtn, true);
  resetBtn =
      ui_create_btn(overlay, 288, 408, 488, 56, "RESET BLOCKED", FONT_BTN, UI_BTN_NORMAL, reset_cb, nullptr);
  lv_obj_set_disabled(resetBtn, true);
}
static void erase_cb(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    erasePressMs = millis() | 1u;
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    erasePressMs = 0;
  } else if (code == LV_EVENT_PRESSING && erasePressMs && millis() - erasePressMs >= kEraseHoldMs) {
    erasePressMs = 0;
    if (storage_format()) esp_restart();
    lv_label_set_text(lv_obj_get_child(eraseBtn, 0), "Erase failed");
  }
}
void estop_overlay_show() {
  dim_reset_activity();
  if (!overlay) return;
  // Input panels are created later on the same top layer. A fault must cover them.
  lv_obj_move_foreground(overlay);
  lv_obj_set_hidden(overlay, false);
  visible = true;
  lastUpdate = millis() - 500;
  estop_overlay_update();
  LOG_E("ESTOP overlay: shown");
}
void estop_overlay_hide() {
  if (overlay) lv_obj_set_hidden(overlay, true);
  visible = false;
}
bool estop_overlay_visible() { return visible; }
void estop_overlay_destroy() {
  if (overlay) lv_obj_delete(overlay);
  overlay = title = reasonLabel = inputLabel = driverLabel = instruction = resetBtn = eraseBtn = resetHint = nullptr;
  banner = bannerCaption = bang = inputDot = driverDot = nullptr;
  erasePressMs = 0;
  visible = false;
}
void estop_overlay_update() {
  if (!visible || millis() - lastUpdate < 250) return;
  lastUpdate = millis();
  const FaultReason reason = safety_get_fault_reason();
  const bool physical = safety_is_estop_active(), driver = safety_is_driver_alarm_latched();
  const bool ready = safety_can_reset_from_overlay();
  const bool estop = reason == FAULT_ESTOP_PRESSED || reason == FAULT_ESTOP_GLITCH;
  // Red: something still blocks the reset. Amber: only the reset itself is missing.
  const lv_color_t tone = lv_color_hex(ready ? kReadyAmber : kBlockedRed);
  lv_obj_set_style_bg_color(banner, tone, 0);
  lv_obj_set_style_text_color(bang, tone, 0);
  lv_label_set_text(bannerCaption, ready ? "MOTION LOCKED - READY TO RESET" : "MOTION LOCKED");
  lv_label_set_text(title, estop ? (physical ? "E-STOP ACTIVE" : "E-STOP RELEASED") :
                           reason == FAULT_PEDAL_INPUT ? "PEDAL INPUT FAULT" :
                           reason == FAULT_DRIVER_ALARM ? "DRIVER ALARM" :
                           reason == FAULT_MOTOR_COMMAND ? "MOTOR COMMAND FAULT" :
                           reason == FAULT_MOTOR_TIMEOUT ? "MOTOR TIMEOUT" :
                           reason == FAULT_NONE ? "MOTION LOCKED" : safety_fault_reason_name(reason));
  char cause[160];
  snprintf(cause, sizeof(cause), "Cause: %s", reason == FAULT_ESTOP_PRESSED && !physical
                                                  ? "Physical E-STOP was pressed and is now released"
                                                  : safety_fault_reason_message(reason));
  lv_label_set_text(reasonLabel, cause);
  lv_obj_set_style_bg_color(inputDot, lv_color_hex(physical ? 0xFFACAF : 0xB9F278), 0);
  lv_obj_set_style_bg_color(driverDot, lv_color_hex(driver ? 0xFFACAF : 0xB9F278), 0);
  lv_label_set_text(inputLabel, physical ? "ACTIVE" : "RELEASED");
  lv_label_set_text(driverLabel, driver ? "ACTIVE" : "CLEAR");
  lv_obj_set_style_text_color(inputLabel, physical ? lv_color_hex(0xFFACAF) : lv_color_hex(0xB9F278), 0);
  lv_obj_set_style_text_color(driverLabel, driver ? lv_color_hex(0xFFACAF) : lv_color_hex(0xB9F278), 0);
  const bool corrupt = reason == FAULT_STORAGE_CORRUPT;
  lv_obj_set_hidden(eraseBtn, !corrupt);
  lv_obj_set_hidden(resetHint, corrupt);
  const bool restart = reason == FAULT_CONTROL_STALE || reason == FAULT_INPUT_STALE || reason == FAULT_HMI_STALE ||
                       g_restartRequired.load(std::memory_order_acquire);
  lv_label_set_text(instruction, ready      ? "Check the machine, then reset."
                                 : physical ? "Release the E-STOP button."
                                 : driver   ? "Clear the driver alarm."
                                 : corrupt  ? "Hold the erase button, or reflash."
                                 : restart  ? "Restart the controller."
                                 : reason == FAULT_PEDAL_INPUT ? "Release the pedal and restore its input."
                                 : reason == FAULT_POT_INPUT   ? "Check the speed pot, then turn the knob up."
                                                               : "Wait - the controller is not ready to reset.");
  lv_obj_t* label = lv_obj_get_child(resetBtn, 0);
  lv_label_set_text(label, ready ? "RESET TO IDLE" : "RESET BLOCKED");
  if (ready)
    lv_obj_set_disabled(resetBtn, false);
  else
    lv_obj_set_disabled(resetBtn, true);
  lv_obj_set_style_bg_color(resetBtn, lv_color_hex(ready ? 0xF5F5F0 : 0x49343A), 0);
  lv_obj_set_style_bg_color(resetBtn, lv_color_hex(0x49343A), LV_STATE_DISABLED);
  lv_obj_set_style_text_color(label, lv_color_hex(ready ? 0x201619 : 0xF1D5D9), 0);
  lv_obj_set_style_opa(resetBtn, LV_OPA_COVER, LV_STATE_DISABLED);
}
