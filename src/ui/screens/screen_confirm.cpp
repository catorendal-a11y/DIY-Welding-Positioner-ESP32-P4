// TIG Rotator Controller - Confirmation Dialog Screen
// POST mockup: 580x316 dialog, rx=6, CONFIRM ACTION + body, CANCEL / DELETE-style buttons

#include "../screens.h"
#include "../theme.h"
#include <atomic>
#include <cstring>

static void (*onConfirmCallback)() = nullptr;
static void (*onCancelCallback)() = nullptr;
static ScreenId returnScreen = SCREEN_PROGRAMS;
static ScreenId confirmSuccessScreen = SCREEN_NONE;
static std::atomic<bool> confirmPending{false};
static std::atomic<bool> cancelPending{false};

static lv_obj_t* bodyTitleLabel = nullptr;
static lv_obj_t* bodyMsgLabel = nullptr;
static lv_obj_t* dangerBtnLabel = nullptr;

static void confirm_event_cb(lv_event_t* e) {
  confirmPending.store(true, std::memory_order_release);
}

static void cancel_event_cb(lv_event_t* e) {
  cancelPending.store(true, std::memory_order_release);
}

void screen_confirm_update() {
  if (confirmPending.load(std::memory_order_acquire)) {
    confirmPending.store(false, std::memory_order_release);
    auto cb = onConfirmCallback;
    onConfirmCallback = nullptr;
    onCancelCallback = nullptr;
    ScreenId dest = (returnScreen > SCREEN_NONE && returnScreen < SCREEN_COUNT)
                    ? returnScreen : SCREEN_PROGRAMS;
    if (confirmSuccessScreen > SCREEN_NONE && confirmSuccessScreen < SCREEN_COUNT) {
      dest = confirmSuccessScreen;
    }
    confirmSuccessScreen = SCREEN_NONE;
    if (cb) {
      cb();
    }
    screens_request_show(dest);
    return;
  }
  if (cancelPending.load(std::memory_order_acquire)) {
    cancelPending.store(false, std::memory_order_release);
    auto cb = onCancelCallback;
    onConfirmCallback = nullptr;
    onCancelCallback = nullptr;
    confirmSuccessScreen = SCREEN_NONE;
    ScreenId dest = (returnScreen > SCREEN_NONE && returnScreen < SCREEN_COUNT)
                    ? returnScreen : SCREEN_PROGRAMS;
    if (cb) {
      cb();
    }
    screens_request_show(dest);
    return;
  }
}

void screen_confirm_create_static() {
  LOG_I("Creating confirm dialog screen...");

  lv_obj_t* screen = screenRoots[SCREEN_CONFIRM];
  if (!screen) {
    LOG_E("screenRoots[SCREEN_CONFIRM] is NULL!");
    return;
  }

  lv_obj_set_style_bg_color(screen, COL_BG, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* dialogBox = lv_obj_create(screen);
  lv_obj_set_size(dialogBox, 580, 316);
  lv_obj_align(dialogBox, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(dialogBox, COL_BG_CARD, 0);
  lv_obj_set_style_bg_opa(dialogBox, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(dialogBox, 6, 0);
  lv_obj_set_style_border_color(dialogBox, COL_BORDER, 0);
  lv_obj_set_style_border_width(dialogBox, 1, 0);
  lv_obj_set_style_pad_all(dialogBox, 0, 0);
  lv_obj_remove_flag(dialogBox, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* headerLbl = lv_label_create(dialogBox);
  lv_label_set_text(headerLbl, "CONFIRM ACTION");
  lv_obj_set_style_text_font(headerLbl, FONT_XXL, 0);
  lv_obj_set_style_text_color(headerLbl, COL_ACCENT, 0);
  lv_obj_set_style_text_align(headerLbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(headerLbl, 540);
  lv_obj_align(headerLbl, LV_ALIGN_TOP_MID, 0, 20);

  bodyTitleLabel = lv_label_create(dialogBox);
  lv_label_set_text(bodyTitleLabel, "");
  lv_obj_set_style_text_font(bodyTitleLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(bodyTitleLabel, COL_TEXT, 0);
  lv_obj_set_style_text_align(bodyTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bodyTitleLabel, 520);
  lv_obj_align(bodyTitleLabel, LV_ALIGN_TOP_MID, 0, 62);

  bodyMsgLabel = lv_label_create(dialogBox);
  lv_label_set_text(bodyMsgLabel, "");
  lv_obj_set_style_text_font(bodyMsgLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(bodyMsgLabel, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_align(bodyMsgLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bodyMsgLabel, 520);
  lv_label_set_long_mode(bodyMsgLabel, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_align(bodyMsgLabel, LV_ALIGN_TOP_MID, 0, 100);

  lv_obj_t* cancelBtn = lv_button_create(dialogBox);
  lv_obj_set_size(cancelBtn, 200, 74);
  lv_obj_set_pos(cancelBtn, 54, 168);
  ui_btn_style_post(cancelBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(cancelBtn, cancel_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLbl, "CANCEL");
  lv_obj_set_style_text_font(cancelLbl, FONT_LARGE, 0);
  lv_obj_set_style_text_color(cancelLbl, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(cancelLbl);

  lv_obj_t* dangerBtn = lv_button_create(dialogBox);
  lv_obj_set_size(dangerBtn, 200, 74);
  lv_obj_set_pos(dangerBtn, 326, 168);
  ui_btn_style_post(dangerBtn, UI_BTN_DANGER);
  lv_obj_add_event_cb(dangerBtn, confirm_event_cb, LV_EVENT_CLICKED, nullptr);

  dangerBtnLabel = lv_label_create(dangerBtn);
  lv_label_set_text(dangerBtnLabel, "CONFIRM");
  lv_obj_set_style_text_font(dangerBtnLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(dangerBtnLabel, ui_btn_label_color_post(UI_BTN_DANGER), 0);
  lv_obj_center(dangerBtnLabel);

  LOG_I("Confirm dialog screen created");
}

void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)(),
                           ScreenId confirm_success_screen) {
  LOG_I("Confirm dialog: title='%s'", title ? title : "null");

  if (!title || !message) {
    LOG_E("Confirm dialog: null title or message!");
    return;
  }

  if (!bodyTitleLabel || !bodyMsgLabel || !dangerBtnLabel) {
    LOG_E("Confirm dialog: widgets not initialized!");
    return;
  }

  onConfirmCallback = on_confirm;
  onCancelCallback = on_cancel;
  returnScreen = screens_get_current();
  confirmSuccessScreen = confirm_success_screen;

  lv_label_set_text(bodyTitleLabel, title);
  lv_label_set_text(bodyMsgLabel, message);

  const char* dangerTxt = "CONFIRM";
  if (strstr(title, "Delete") != nullptr || strstr(title, "DELETE") != nullptr) {
    dangerTxt = "DELETE";
  } else if (strstr(title, "REBOOT") != nullptr || strstr(title, "Reboot") != nullptr) {
    dangerTxt = "REBOOT";
  }
  lv_label_set_text(dangerBtnLabel, dangerTxt);

  screens_show(SCREEN_CONFIRM);
  LOG_I("Confirm dialog shown");
}
