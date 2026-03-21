// TIG Rotator Controller - Confirmation Dialog Screen

#include "../screens.h"
#include "../theme.h"

// ───────────────────────────────────────────────────────────────────────────────
// CALLBACKS
// ───────────────────────────────────────────────────────────────────────────────
static void (*onConfirmCallback)() = nullptr;
static void (*onCancelCallback)() = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void confirm_event_cb(lv_event_t* e) {
  if (onConfirmCallback) {
    onConfirmCallback();
  }
  // Return to previous screen
  lv_obj_add_flag(screenRoots[SCREEN_CONFIRM], LV_OBJ_FLAG_HIDDEN);
}

static void cancel_event_cb(lv_event_t* e) {
  if (onCancelCallback) {
    onCancelCallback();
  }
  lv_obj_add_flag(screenRoots[SCREEN_CONFIRM], LV_OBJ_FLAG_HIDDEN);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE (called once at init)
// ───────────────────────────────────────────────────────────────────────────────
void screen_confirm_create_static() {
  lv_obj_t* screen = screenRoots[SCREEN_CONFIRM];
  lv_obj_set_style_bg_opa(screen, LV_OPA_70, 0);  // Semi-transparent

  // Dialog box
  lv_obj_t* dialog = lv_obj_create(screen);
  lv_obj_set_size(dialog, 360, 160);
  lv_obj_center(dialog);
  lv_obj_set_style_bg_color(dialog, COL_BG_CARD, 0);
  lv_obj_set_style_radius(dialog, RADIUS_CARD, 0);

  // Message label
  lv_obj_t* msgLabel = lv_label_create(dialog);
  lv_label_set_text(msgLabel, "Confirm action?");
  lv_label_set_long_mode(msgLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(msgLabel, 320, 80);
  lv_obj_set_style_text_color(msgLabel, COL_TEXT, 0);
  lv_obj_align(msgLabel, LV_ALIGN_TOP_MID, 0, 16);

  // Confirm button
  lv_obj_t* confirmBtn = lv_btn_create(dialog);
  lv_obj_set_size(confirmBtn, 160, 44);
  lv_obj_set_pos(confirmBtn, 16, 104);
  lv_obj_set_style_bg_color(confirmBtn, COL_GREEN_DARK, 0);
  lv_obj_set_style_radius(confirmBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(confirmBtn, confirm_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* confirmLabel = lv_label_create(confirmBtn);
  lv_label_set_text(confirmLabel, "✓ CONFIRM");
  lv_obj_center(confirmLabel);

  // Cancel button
  lv_obj_t* cancelBtn = lv_btn_create(dialog);
  lv_obj_set_size(cancelBtn, 160, 44);
  lv_obj_set_pos(cancelBtn, 188, 104);
  lv_obj_set_style_bg_color(cancelBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(cancelBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(cancelBtn, cancel_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLabel, "✗ CANCEL");
  lv_obj_center(cancelLabel);

  // Initially hidden
  lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
}

// ───────────────────────────────────────────────────────────────────────────────
// SHOW DIALOG
// ───────────────────────────────────────────────────────────────────────────────
void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)()) {
  onConfirmCallback = on_confirm;
  onCancelCallback = on_cancel;

  lv_obj_t* screen = screenRoots[SCREEN_CONFIRM];

  // Update message
  lv_obj_t* dialog = lv_obj_get_child(screen, 0);
  lv_obj_t* msgLabel = lv_obj_get_child(dialog, 0);
  lv_label_set_text(msgLabel, message);

  // Show dialog
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_HIDDEN);
  lv_scr_load(screen);
}
