#include "../screens.h"
#include "../theme.h"

static void (*onConfirmCallback)() = nullptr;
static void (*onCancelCallback)() = nullptr;

static lv_obj_t* confirmOverlay = nullptr;
static lv_obj_t* dialogTitleLabel = nullptr;
static lv_obj_t* dialogMsgLabel = nullptr;

static void confirm_event_cb(lv_event_t* e) {
    if (onConfirmCallback) {
        onConfirmCallback();
    }
    onConfirmCallback = nullptr;
    onCancelCallback = nullptr;
    if (confirmOverlay) lv_obj_add_flag(confirmOverlay, LV_OBJ_FLAG_HIDDEN);
}

static void cancel_event_cb(lv_event_t* e) {
    if (onCancelCallback) {
        onCancelCallback();
    }
    onConfirmCallback = nullptr;
    onCancelCallback = nullptr;
    if (confirmOverlay) lv_obj_add_flag(confirmOverlay, LV_OBJ_FLAG_HIDDEN);
}

void screen_confirm_create_static() {
    confirmOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(confirmOverlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(confirmOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(confirmOverlay, LV_OPA_50, 0);
    lv_obj_set_style_border_width(confirmOverlay, 0, 0);
    lv_obj_set_style_pad_all(confirmOverlay, 0, 0);
    lv_obj_set_style_radius(confirmOverlay, 0, 0);
    lv_obj_add_flag(confirmOverlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* dialog = lv_obj_create(confirmOverlay);
    lv_obj_set_size(dialog, SW(280), SH(150));
    lv_obj_set_pos(dialog, SX(40), SY(50));
    lv_obj_set_style_bg_color(dialog, COL_HEADER_BG, 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog, COL_BORDER, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_set_style_radius(dialog, RADIUS_MD, 0);
    lv_obj_set_style_pad_all(dialog, 0, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* warnIcon = lv_label_create(dialog);
    lv_label_set_text(warnIcon, "\xE2\x9A\xA0");
    lv_obj_set_style_text_font(warnIcon, FONT_XXL, 0);
    lv_obj_set_style_text_color(warnIcon, COL_ACCENT, 0);
    lv_obj_align(warnIcon, LV_ALIGN_TOP_MID, 0, SY(10));

    dialogTitleLabel = lv_label_create(dialog);
    lv_label_set_text(dialogTitleLabel, "Delete program?");
    lv_obj_set_style_text_font(dialogTitleLabel, FONT_SUB, 0);
    lv_obj_set_style_text_color(dialogTitleLabel, COL_TEXT, 0);
    lv_obj_align(dialogTitleLabel, LV_ALIGN_TOP_MID, 0, SY(40));

    dialogMsgLabel = lv_label_create(dialog);
    lv_label_set_text(dialogMsgLabel, "This action cannot be undone.");
    lv_obj_set_style_text_font(dialogMsgLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(dialogMsgLabel, COL_TEXT_DIM, 0);
    lv_obj_align(dialogMsgLabel, LV_ALIGN_TOP_MID, 0, SY(65));

    lv_obj_t* sep = lv_obj_create(dialog);
    lv_obj_set_size(sep, SW(240), SH(1));
    lv_obj_set_pos(sep, SX(20), SY(90));
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_shadow_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cancelBtn = lv_btn_create(dialog);
    lv_obj_set_size(cancelBtn, SW(120), SH(34));
    lv_obj_set_pos(cancelBtn, SX(16), SY(100));
    lv_obj_set_style_bg_color(cancelBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(cancelBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(cancelBtn, 2, 0);
    lv_obj_set_style_radius(cancelBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(cancelBtn, 0, 0);
    lv_obj_set_style_pad_all(cancelBtn, 0, 0);
    lv_obj_add_event_cb(cancelBtn, cancel_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "CANCEL");
    lv_obj_set_style_text_font(cancelLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(cancelLbl, COL_TEXT, 0);
    lv_obj_center(cancelLbl);

    lv_obj_t* confirmBtn = lv_btn_create(dialog);
    lv_obj_set_size(confirmBtn, SW(120), SH(34));
    lv_obj_set_pos(confirmBtn, SX(144), SY(100));
    lv_obj_set_style_bg_color(confirmBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_border_color(confirmBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(confirmBtn, 2, 0);
    lv_obj_set_style_radius(confirmBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(confirmBtn, 0, 0);
    lv_obj_set_style_pad_all(confirmBtn, 0, 0);
    lv_obj_add_event_cb(confirmBtn, confirm_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* confirmLbl = lv_label_create(confirmBtn);
    lv_label_set_text(confirmLbl, "CONFIRM");
    lv_obj_set_style_text_font(confirmLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(confirmLbl, COL_TEXT, 0);
    lv_obj_center(confirmLbl);
}

void screen_confirm_create(const char* title, const char* message,
                            void (*on_confirm)(), void (*on_cancel)()) {
    onConfirmCallback = on_confirm;
    onCancelCallback = on_cancel;

    if (dialogTitleLabel && title) {
        lv_label_set_text(dialogTitleLabel, title);
    }
    if (dialogMsgLabel && message) {
        lv_label_set_text(dialogMsgLabel, message);
    }

    if (confirmOverlay) {
        lv_obj_clear_flag(confirmOverlay, LV_OBJ_FLAG_HIDDEN);
    }
}
