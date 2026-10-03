#pragma once
#include "screens.h"
#include "theme.h"
#include "text_metrics.h"
#include "value_binding.h"

// One screen-owned editor. Close outside the keyboard event callback.
// The panel tracks deletion so a screen/theme rebuild cannot retain its widgets.
class UiInputPanel {
 public:
  UiInputPanel() = default;
  UiInputPanel(const UiInputPanel&) = delete;
  UiInputPanel& operator=(const UiInputPanel&) = delete;
  bool active() const { return root_ != nullptr; }
  const char* text() const { return field_ ? lv_textarea_get_text(field_) : ""; }
  void error(const char* message) { errorBinding_.set(message); }
  void open(const char* title, const char* hint, const char* initial,
            const char* accepted, uint32_t maxLength, lv_keyboard_mode_t mode,
            lv_event_cb_t callback) {
    if (active()) return;
    root_ = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_size(root_, SCREEN_W, SCREEN_H);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_shadow_width(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, COL_BG, 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_scrollable(root_, false);
    lv_obj_add_event_cb(root_, deleted, LV_EVENT_DELETE, this);
    auto heading = ui_create_text(root_, 24, 24, 570, title, FONT_XL, COL_TEXT);
    lv_label_set_long_mode(heading, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_max_lines(heading, 1);
    ui_create_btn(root_, 620, 20, 156, 48, "CANCEL", FONT_BTN, UI_BTN_NORMAL,
                  cancel, this);
    auto instruction = ui_create_text(root_, 24, 70, 752, hint, FONT_SUBTITLE, COL_TEXT_DIM);
    lv_label_set_long_mode(instruction, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_max_lines(instruction, 2);
    field_ = lv_textarea_create(root_);
    lv_obj_set_pos(field_, 24, 112);
    lv_obj_set_size(field_, 752, 54);
    lv_textarea_set_one_line(field_, true);
    lv_textarea_set_max_length(field_, maxLength);
    // LVGL copies this string, so callers need not keep a temporary filter alive.
    if (accepted) lv_textarea_set_accepted_chars(field_, accepted);
    lv_obj_set_style_text_font(field_, mode == LV_KEYBOARD_MODE_NUMBER ? FONT_XL : FONT_LARGE, 0);
    lv_textarea_set_text(field_, initial);
    auto errorLabel = ui_create_text(root_, 24, 178, 752, "", FONT_SUBTITLE, COL_RED);
    lv_label_set_long_mode(errorLabel, LV_LABEL_LONG_MODE_WRAP);
    lv_label_set_max_lines(errorLabel, 3);
    errorBinding_.bind(errorLabel);
    // Leave LVGL's built-in textarea/keyboard metrics intact.
    ui_trim_text_leading(root_);
    keyboard_ = lv_keyboard_create(root_);
    lv_obj_set_size(keyboard_, SCREEN_W, 236);
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(keyboard_, mode);
    lv_keyboard_set_textarea(keyboard_, field_);
    lv_obj_add_event_cb(keyboard_, callback, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(keyboard_, callback, LV_EVENT_CANCEL, nullptr);
  }
  void close() {
    if (!root_) return;
    errorBinding_.reset();
    lv_keyboard_set_textarea(keyboard_, nullptr);
    lv_obj_remove_event_cb_with_user_data(root_, deleted, this);
    // A queued deletion must not cover a replacement screen for one more frame.
    lv_obj_set_hidden(root_, true);
    lv_obj_delete_async(root_);
    root_ = field_ = keyboard_ = nullptr;
  }
 private:
  lv_obj_t *root_ = nullptr, *field_ = nullptr, *keyboard_ = nullptr;
  UiTextBinding<192> errorBinding_;
  static void cancel(lv_event_t* event) {
    auto panel = static_cast<UiInputPanel*>(lv_event_get_user_data(event));
    if (panel->keyboard_) lv_obj_send_event(panel->keyboard_, LV_EVENT_CANCEL, nullptr);
  }
  static void deleted(lv_event_t* event) {
    auto panel = static_cast<UiInputPanel*>(lv_event_get_user_data(event));
    panel->errorBinding_.reset();
    panel->root_ = panel->field_ = panel->keyboard_ = nullptr;
  }
};
