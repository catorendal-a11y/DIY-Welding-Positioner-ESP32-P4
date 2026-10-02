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

static void confirm_event_cb(lv_event_t* e) { confirmPending.store(true, std::memory_order_release); }

static void cancel_event_cb(lv_event_t* e) { cancelPending.store(true, std::memory_order_release); }

void screen_confirm_update() {
  if (confirmPending.load(std::memory_order_acquire)) {
    confirmPending.store(false, std::memory_order_release);
    auto cb = onConfirmCallback;
    onConfirmCallback = nullptr;
    onCancelCallback = nullptr;
    ScreenId dest =
        (returnScreen > SCREEN_NONE && returnScreen < SCREEN_COUNT) ? returnScreen : SCREEN_PROGRAMS;
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
    ScreenId dest =
        (returnScreen > SCREEN_NONE && returnScreen < SCREEN_COUNT) ? returnScreen : SCREEN_PROGRAMS;
    if (cb) {
      cb();
    }
    screens_request_show(dest);
    return;
  }
}

void screen_confirm_create_static() {
  lv_obj_t* screen = screenRoots[SCREEN_CONFIRM];
  if (!screen) return;
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);
  ui_create_header(screen, "Confirm action", "REVIEW", nullptr);
  lv_obj_t* dialog = ui_create_post_card(screen, 56, 94, 688, 280);
  bodyTitleLabel = ui_create_text(dialog, 24, 26, 640, "", FONT_XL, COL_TEXT);
  lv_label_set_long_mode(bodyTitleLabel, LV_LABEL_LONG_MODE_WRAP);
  bodyMsgLabel = ui_create_text(dialog, 24, 94, 640, "", FONT_SUBTITLE, COL_TEXT_DIM);
  lv_label_set_long_mode(bodyMsgLabel, LV_LABEL_LONG_MODE_WRAP);
  ui_create_btn(screen, 24, 408, 368, 56, "CANCEL", FONT_BTN, UI_BTN_NORMAL, cancel_event_cb, nullptr);
  lv_obj_t* confirm =
      ui_create_btn(screen, 408, 408, 368, 56, "CONFIRM", FONT_BTN, UI_BTN_DANGER, confirm_event_cb, nullptr);
  dangerBtnLabel = lv_obj_get_child(confirm, 0);
}

void screen_confirm_create(const char* title, const char* message, void (*on_confirm)(), void (*on_cancel)(),
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
