#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/microstep.h"
#include "../../motor/acceleration.h"
#include "../../motor/calibration.h"

static int selectedRow = 0;
static lv_obj_t* valueLabels[3] = {nullptr};
static lv_obj_t* rowRects[3] = {nullptr};

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static lv_timer_t* saveResetTimer = nullptr;

static void save_reset_timer_cb(lv_timer_t* timer) {
    lv_obj_t* btn = (lv_obj_t*)timer->user_data;
    if (!btn) return;
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) lv_label_set_text(label, "SAVE");
    saveResetTimer = nullptr;
}

static void save_event_cb(lv_event_t* e) {
    acceleration_save();
    microstep_save();
    calibration_save();
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* label = lv_obj_get_child(btn, 0);
    if (label) lv_label_set_text(label, "SAVED!");
    if (saveResetTimer) lv_timer_del(saveResetTimer);
    saveResetTimer = lv_timer_create(save_reset_timer_cb, 800, btn);
    lv_timer_set_repeat_count(saveResetTimer, 1);
}

static void select_row(int index) {
    selectedRow = index;
    for (int i = 0; i < 3; i++) {
        if (!rowRects[i]) continue;
        if (i == index) {
            lv_obj_set_style_bg_color(rowRects[i], COL_BTN_ACTIVE, 0);
            lv_obj_set_style_border_color(rowRects[i], COL_ACCENT, 0);
            lv_obj_set_style_border_width(rowRects[i], 2, 0);
            if (valueLabels[i]) {
                lv_obj_set_style_text_color(valueLabels[i], COL_ACCENT, 0);
            }
        } else {
            lv_obj_set_style_bg_color(rowRects[i], COL_BTN_FILL, 0);
            lv_obj_set_style_border_color(rowRects[i], COL_BORDER, 0);
            lv_obj_set_style_border_width(rowRects[i], 1, 0);
            if (valueLabels[i]) {
                lv_obj_set_style_text_color(valueLabels[i], COL_TEXT_DIM, 0);
            }
        }
    }
}

static void row_click_cb(lv_event_t* e) {
    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(e);
    int index = (int)(intptr_t)lv_obj_get_user_data(row);
    select_row(index);
}

static void update_value_label(int row) {
    char buf[16];
    switch (row) {
        case 0:
            snprintf(buf, sizeof(buf), "%u", acceleration_get());
            lv_label_set_text(valueLabels[0], buf);
            break;
        case 1:
            lv_label_set_text(valueLabels[1], microstep_get_string());
            break;
        case 2:
            snprintf(buf, sizeof(buf), "%.2f", calibration_get_factor());
            lv_label_set_text(valueLabels[2], buf);
            break;
    }
}

static void plus_event_cb(lv_event_t* e) {
    switch (selectedRow) {
        case 0: {
            uint32_t accel = acceleration_get();
            if (accel < 1000) accel = 1000;
            else if (accel < 2000) accel = 2000;
            else if (accel < 5000) accel = 5000;
            else if (accel < 10000) accel = 10000;
            else if (accel < 20000) accel = 20000;
            else accel = 1000;
            acceleration_set(accel);
            break;
        }
        case 1: {
            MicrostepSetting cur = microstep_get();
            MicrostepSetting next;
            switch (cur) {
                case MICROSTEP_8:  next = MICROSTEP_16; break;
                case MICROSTEP_16: next = MICROSTEP_32; break;
                case MICROSTEP_32: next = MICROSTEP_8;  break;
                default: next = MICROSTEP_8;
            }
            microstep_set(next);
            break;
        }
        case 2: {
            float f = calibration_get_factor();
            f *= 1.01f;
            if (f > 1.50f) f = 1.50f;
            calibration_set_factor(f);
            break;
        }
    }
    update_value_label(selectedRow);
}

static void minus_event_cb(lv_event_t* e) {
    switch (selectedRow) {
        case 0: {
            uint32_t accel = acceleration_get();
            if (accel > 20000) accel = 20000;
            else if (accel > 10000) accel = 10000;
            else if (accel > 5000) accel = 5000;
            else if (accel > 2000) accel = 2000;
            else if (accel > 1000) accel = 1000;
            else accel = 20000;
            acceleration_set(accel);
            break;
        }
        case 1: {
            MicrostepSetting cur = microstep_get();
            MicrostepSetting next;
            switch (cur) {
                case MICROSTEP_8:  next = MICROSTEP_32; break;
                case MICROSTEP_16: next = MICROSTEP_8;  break;
                case MICROSTEP_32: next = MICROSTEP_16; break;
                default: next = MICROSTEP_8;
            }
            microstep_set(next);
            break;
        }
        case 2: {
            float f = calibration_get_factor();
            f *= 0.99f;
            if (f < 0.50f) f = 0.50f;
            calibration_set_factor(f);
            break;
        }
    }
    update_value_label(selectedRow);
}

static lv_obj_t* create_row(lv_obj_t* parent, int index, const char* title,
                            const char* value, bool active) {
    int16_t y = SY(42) + index * (SH(28) + 12);
    lv_obj_t* row = lv_btn_create(parent);
    lv_obj_set_size(row, SW(328), SH(28));
    lv_obj_set_pos(row, SX(16), y);
    lv_obj_set_style_radius(row, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_style_pad_hor(row, SX(8), 0);

    if (active) {
        lv_obj_set_style_bg_color(row, COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(row, COL_ACCENT, 0);
        lv_obj_set_style_border_width(row, 2, 0);
    } else {
        lv_obj_set_style_bg_color(row, COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(row, COL_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
    }

    lv_obj_set_user_data(row, (void*)(intptr_t)index);
    lv_obj_add_event_cb(row, row_click_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* titleLabel = lv_label_create(row);
    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_font(titleLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(titleLabel, COL_TEXT, 0);
    lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* valLabel = lv_label_create(row);
    lv_label_set_text(valLabel, value);
    lv_obj_set_style_text_font(valLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(valLabel, COL_ACCENT, 0);
    lv_obj_align(valLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    return row;
}

static lv_obj_t* create_small_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                  int16_t w, int16_t h, const char* text) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
    lv_obj_center(label);

    return btn;
}

void screen_settings_create() {
    lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];
    lv_obj_set_style_bg_color(screen, COL_BG, 0);

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* backLabel = lv_label_create(header);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(backLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(backLabel, SX(16), SY(21));

    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, SW(50), SH(14));
    lv_obj_set_pos(backBtn, SX(8), SY(12));
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(backBtn, 0, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_set_style_radius(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* titleLabel = lv_label_create(header);
    lv_label_set_text(titleLabel, "SETTINGS");
    lv_obj_set_style_text_font(titleLabel, FONT_BODY, 0);
    lv_obj_set_style_text_color(titleLabel, COL_TEXT, 0);
    lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(titleLabel, SCREEN_W);
    lv_obj_set_pos(titleLabel, 0, SY(21));

    lv_obj_t* sep = lv_obj_create(screen);
    lv_obj_set_size(sep, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep, 0, HEADER_H);
    lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    char accelBuf[16], calBuf[16];
    snprintf(accelBuf, sizeof(accelBuf), "%u", acceleration_get());
    snprintf(calBuf, sizeof(calBuf), "%.2f", calibration_get_factor());

    const char* titles[] = {"Acceleration", "Microstep", "Calibration Factor"};
    const char* values[] = {accelBuf, microstep_get_string(), calBuf};

    for (int i = 0; i < 3; i++) {
        lv_obj_t* row = create_row(screen, i, titles[i], values[i], i == selectedRow);
        rowRects[i] = row;
        valueLabels[i] = lv_obj_get_child(row, 1);
    }

    lv_obj_t* minusBtn = create_small_btn(screen, SX(16), SY(146), SW(70), SH(28), "-1%");
    lv_obj_add_event_cb(minusBtn, minus_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* plusBtn = create_small_btn(screen, SX(92), SY(146), SW(70), SH(28), "+1%");
    lv_obj_add_event_cb(plusBtn, plus_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* saveBtn = create_small_btn(screen, SX(168), SY(146), SW(70), SH(28), "SAVE");
    lv_obj_add_event_cb(saveBtn, save_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* sep2 = lv_obj_create(screen);
    lv_obj_set_size(sep2, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep2, 0, SY(182));
    lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    lv_obj_t* info1 = lv_label_create(screen);
    lv_label_set_text(info1, "Max RPM: 5.0");
    lv_obj_set_style_text_font(info1, FONT_SMALL, 0);
    lv_obj_set_style_text_color(info1, COL_TEXT_DIM, 0);
    lv_obj_set_pos(info1, SX(16), SY(202));

    lv_obj_t* info2 = lv_label_create(screen);
    lv_label_set_text(info2, "Gear Ratio: 108:1");
    lv_obj_set_style_text_font(info2, FONT_SMALL, 0);
    lv_obj_set_style_text_color(info2, COL_TEXT_DIM, 0);
    lv_obj_set_pos(info2, SX(120), SY(202));

    lv_obj_t* info3 = lv_label_create(screen);
    lv_label_set_text(info3, "FW: v1.2.0");
    lv_obj_set_style_text_font(info3, FONT_SMALL, 0);
    lv_obj_set_style_text_color(info3, COL_TEXT_DIM, 0);
    lv_obj_set_pos(info3, SX(260), SY(202));
}
