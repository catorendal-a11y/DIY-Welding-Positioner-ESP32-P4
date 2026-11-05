// V5 blocking fault overlay: strong red identity, explicit input and reset state.
#include "../screens.h"
#include "../theme.h"
#include "../../storage/storage.h"
#include "esp_system.h"
#include "../lvgl_hal.h"
#include "../../config.h"
#include "../../safety/safety.h"

static lv_obj_t *overlay = nullptr, *title = nullptr, *reasonLabel = nullptr, *inputLabel = nullptr;
static lv_obj_t *driverLabel = nullptr, *instruction = nullptr, *resetBtn = nullptr;
static lv_obj_t* eraseBtn = nullptr;
static lv_obj_t* resetHint = nullptr;
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
  lv_obj_t* banner = panel(overlay, 0, 0, 800, 106, 0xB52C35);
  lv_obj_set_style_radius(banner, 0, 0);
  lv_obj_t* icon = panel(banner, 24, 22, 62, 62, 0xFFFFFF);
  lv_obj_t* bang = ui_create_text(icon, 0, 8, 62, "!", FONT_HUGE, lv_color_hex(0xB52C35));
  lv_obj_set_style_text_align(bang, LV_TEXT_ALIGN_CENTER, 0);
  ui_create_text(banner, 108, 19, 660, "MOTION LOCKED", FONT_NORMAL, lv_color_hex(0xFFFFFF));
  title = ui_create_text(banner, 108, 48, 668, "EMERGENCY STOP", FONT_XXL, lv_color_hex(0xFFFFFF));
  ui_create_text(overlay, 24, 126, 752, "MOTOR OUTPUT DISABLED", FONT_XL, lv_color_hex(0xFFFFFF));
  reasonLabel = ui_create_text(overlay, 24, 166, 752, "", FONT_SUBTITLE, lv_color_hex(0xE3CBCD));
  lv_obj_t* input = panel(overlay, 24, 204, 752, 60, 0x342328);
  ui_create_text(input, 18, 18, 340, "Physical E-STOP", FONT_LARGE, lv_color_hex(0xFFFFFF));
  inputLabel = ui_create_text(input, 392, 18, 338, "", FONT_LARGE, lv_color_hex(0xFFACAF));
  lv_obj_set_style_text_align(inputLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_t* driver = panel(overlay, 24, 276, 752, 60, 0x342328);
  ui_create_text(driver, 18, 18, 340, "Driver alarm", FONT_LARGE, lv_color_hex(0xFFFFFF));
  driverLabel = ui_create_text(driver, 392, 18, 338, "", FONT_LARGE, COL_GREEN);
  lv_obj_set_style_text_align(driverLabel, LV_TEXT_ALIGN_RIGHT, 0);
  instruction = ui_create_text(overlay, 24, 351, 752, "", FONT_SUBTITLE, lv_color_hex(0xF4DCDD));
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
  erasePressMs = 0;
  visible = false;
}
void estop_overlay_update() {
  if (!visible || millis() - lastUpdate < 250) return;
  lastUpdate = millis();
  const FaultReason reason = safety_get_fault_reason();
  lv_label_set_text(title, reason == FAULT_ESTOP_PRESSED || reason == FAULT_ESTOP_GLITCH ? "E-STOP ACTIVE" :
                           reason == FAULT_PEDAL_INPUT ? "PEDAL INPUT FAULT" :
                           reason == FAULT_DRIVER_ALARM ? "DRIVER ALARM" :
                           reason == FAULT_MOTOR_COMMAND ? "MOTOR COMMAND FAULT" :
                           reason == FAULT_MOTOR_TIMEOUT ? "MOTOR TIMEOUT" :
                           reason == FAULT_NONE ? "MOTION LOCKED" : safety_fault_reason_name(reason));
  lv_label_set_text(reasonLabel, safety_fault_reason_message(reason));
  const bool physical = safety_is_estop_active(), driver = safety_is_driver_alarm_latched();
  lv_label_set_text(inputLabel, physical ? "ACTIVE" : "RELEASED");
  lv_label_set_text(driverLabel, driver ? "ACTIVE" : "CLEAR");
  lv_obj_set_style_text_color(inputLabel, physical ? lv_color_hex(0xFFACAF) : lv_color_hex(0xB9F278), 0);
  lv_obj_set_style_text_color(driverLabel, driver ? lv_color_hex(0xFFACAF) : lv_color_hex(0xB9F278), 0);
  const bool corrupt = reason == FAULT_STORAGE_CORRUPT;
  lv_obj_set_hidden(eraseBtn, !corrupt);
  lv_obj_set_hidden(resetHint, corrupt);
  const bool ready = safety_can_reset_from_overlay();
  lv_label_set_text(instruction, ready ? "Check the machine. Reset returns to idle; a new Start is required."
                                 : reason == FAULT_PEDAL_INPUT
                                     ? "Release the pedal and restore its input before resetting."
                                     : safety_fault_reason_message(reason));
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
