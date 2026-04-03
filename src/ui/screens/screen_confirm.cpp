// TIG Rotator Controller - Confirmation Dialog Screen
// Fully opaque, no LV_OBJ_FLAG_HIDDEN on screen root (causes crash - screen has no parent)

#include "../screens.h"
#include "../theme.h"

// ───────────────────────────────────────────────────────────────────────────────
// CALLBACKS
// ───────────────────────────────────────────────────────────────────────────────
static void (*onConfirmCallback)() = nullptr;
static void (*onCancelCallback)() = nullptr;
static ScreenId returnScreen = SCREEN_PROGRAMS;

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* titleLabel = nullptr;
static lv_obj_t* warnLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void confirm_event_cb(lv_event_t* e) {
  LOG_I("Confirm dialog: CONFIRM pressed");
  auto cb = onConfirmCallback;
  onConfirmCallback = nullptr;
  onCancelCallback = nullptr;
  if (cb) {
    cb();
  }
  screens_show(returnScreen);
}

static void cancel_event_cb(lv_event_t* e) {
  LOG_I("Confirm dialog: CANCEL pressed");
  if (onCancelCallback) {
    onCancelCallback();
  }
  onConfirmCallback = nullptr;
  onCancelCallback = nullptr;
  screens_show(returnScreen);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE (called once at init)
// NOTE: Do NOT set LV_OBJ_FLAG_HIDDEN on screen root — screen objects have no
// parent, so lv_obj_remove_flag(screen, LV_OBJ_FLAG_HIDDEN) crashes because
// LVGL tries to mark the NULL parent's layout dirty. Use lv_screen_load only.
// ───────────────────────────────────────────────────────────────────────────────
void screen_confirm_create_static() {
  LOG_I("Creating confirm dialog screen...");

  lv_obj_t* screen = screenRoots[SCREEN_CONFIRM];
  if (!screen) {
    LOG_E("screenRoots[SCREEN_CONFIRM] is NULL!");
    return;
  }

  // Solid dark background
  lv_obj_set_style_bg_color(screen, COL_BG, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  // ── Dialog box (centered) ──
  lv_obj_t* dialogBox = lv_obj_create(screen);
  lv_obj_set_size(dialogBox, 700, 340);
  lv_obj_align(dialogBox, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(dialogBox, COL_BG_CARD, 0);
  lv_obj_set_style_bg_opa(dialogBox, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dialogBox, RADIUS_CARD, 0);
  lv_obj_set_style_border_color(dialogBox, COL_BORDER, 0);
  lv_obj_set_style_border_width(dialogBox, 1, 0);
  lv_obj_set_style_pad_all(dialogBox, 0, 0);
  lv_obj_remove_flag(dialogBox, LV_OBJ_FLAG_SCROLLABLE);

  // ── Header bar ──
  lv_obj_t* headerBg = lv_obj_create(dialogBox);
  lv_obj_set_size(headerBg, 700, 32);
  lv_obj_set_pos(headerBg, 0, 0);
  lv_obj_set_style_bg_color(headerBg, COL_BG_HEADER, 0);
  lv_obj_set_style_bg_opa(headerBg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(headerBg, 0, 0);
  lv_obj_set_style_radius(headerBg, 0, 0);
  lv_obj_set_style_pad_all(headerBg, 0, 0);
  lv_obj_remove_flag(headerBg, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* headerLabel = lv_label_create(headerBg);
  lv_label_set_text(headerLabel, "CONFIRM ACTION");
  lv_obj_set_style_text_font(headerLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(headerLabel, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_letter_space(headerLabel, 3, 0);
  lv_obj_set_pos(headerLabel, 16, 9);

  // Red line below header
  lv_obj_t* redLine = lv_obj_create(dialogBox);
  lv_obj_set_size(redLine, 700, 2);
  lv_obj_set_pos(redLine, 0, 32);
  lv_obj_set_style_bg_color(redLine, COL_RED, 0);
  lv_obj_set_style_bg_opa(redLine, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(redLine, 0, 0);
  lv_obj_set_style_radius(redLine, 0, 0);
  lv_obj_set_style_pad_all(redLine, 0, 0);
  lv_obj_remove_flag(redLine, LV_OBJ_FLAG_SCROLLABLE);

  // ── Warning icon ──
  lv_obj_t* warnIcon = lv_label_create(dialogBox);
  lv_label_set_text(warnIcon, "!");
  lv_obj_set_style_text_font(warnIcon, FONT_HUGE, 0);
  lv_obj_set_style_text_color(warnIcon, COL_RED, 0);
  lv_obj_align(warnIcon, LV_ALIGN_TOP_MID, 0, 46);

  // ── Title label (dynamic) ──
  titleLabel = lv_label_create(dialogBox);
  lv_label_set_text(titleLabel, "DELETE?");
  lv_obj_set_style_text_font(titleLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(titleLabel, COL_TEXT_BRIGHT, 0);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 92);

  // ── Warning message (dynamic) ──
  warnLabel = lv_label_create(dialogBox);
  lv_label_set_text(warnLabel, "This action cannot be undone.");
  lv_obj_set_style_text_font(warnLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(warnLabel, COL_RED, 0);
  lv_obj_align(warnLabel, LV_ALIGN_TOP_MID, 0, 118);

  // ── CANCEL button ──
  lv_obj_t* cancelBtn = lv_button_create(dialogBox);
  lv_obj_set_size(cancelBtn, 300, 64);
  lv_obj_set_pos(cancelBtn, 30, 240);
  lv_obj_set_style_bg_color(cancelBtn, COL_BTN_BG, 0);
  lv_obj_set_style_bg_opa(cancelBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(cancelBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_color(cancelBtn, COL_BORDER, 0);
  lv_obj_set_style_border_width(cancelBtn, 1, 0);
  lv_obj_set_style_shadow_width(cancelBtn, 0, 0);
  lv_obj_set_style_pad_all(cancelBtn, 0, 0);
  lv_obj_add_event_cb(cancelBtn, cancel_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLabel, "CANCEL");
  lv_obj_set_style_text_font(cancelLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(cancelLabel, COL_TEXT, 0);
  lv_obj_center(cancelLabel);

  // ── DELETE button ──
  lv_obj_t* deleteBtn = lv_button_create(dialogBox);
  lv_obj_set_size(deleteBtn, 300, 64);
  lv_obj_set_pos(deleteBtn, 370, 240);
  lv_obj_set_style_bg_color(deleteBtn, COL_BG_DANGER, 0);
  lv_obj_set_style_bg_opa(deleteBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(deleteBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_color(deleteBtn, COL_RED, 0);
  lv_obj_set_style_border_width(deleteBtn, 2, 0);
  lv_obj_set_style_shadow_width(deleteBtn, 0, 0);
  lv_obj_set_style_pad_all(deleteBtn, 0, 0);
  lv_obj_add_event_cb(deleteBtn, confirm_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* deleteLabel = lv_label_create(deleteBtn);
  lv_label_set_text(deleteLabel, "CONFIRM");
  lv_obj_set_style_text_font(deleteLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(deleteLabel, COL_RED, 0);
  lv_obj_center(deleteLabel);

  // Do NOT set LV_OBJ_FLAG_HIDDEN on screen root!
  // lv_screen_load() handles screen visibility.
  LOG_I("Confirm dialog screen created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SHOW DIALOG
// ───────────────────────────────────────────────────────────────────────────────
void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)()) {
  LOG_I("Confirm dialog: title='%s'", title ? title : "null");

  if (!title || !message) {
    LOG_E("Confirm dialog: null title or message!");
    return;
  }

  if (!titleLabel || !warnLabel) {
    LOG_E("Confirm dialog: widgets not initialized!");
    return;
  }

  onConfirmCallback = on_confirm;
  onCancelCallback = on_cancel;
  returnScreen = screens_get_current();

  lv_label_set_text(titleLabel, title);
  lv_label_set_text(warnLabel, message);

  // Just switch screen — no LV_OBJ_FLAG_HIDDEN manipulation
  screens_show(SCREEN_CONFIRM);
  LOG_I("Confirm dialog shown");
}
