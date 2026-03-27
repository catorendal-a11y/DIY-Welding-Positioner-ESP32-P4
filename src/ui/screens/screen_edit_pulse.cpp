#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include <cstdio>

static lv_obj_t* onTimeLabel = nullptr;
static lv_obj_t* offTimeLabel = nullptr;
static lv_obj_t* presetBtns[4] = {nullptr};
static int activePreset = -1;

static void back_event_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (p && onTimeLabel && offTimeLabel) {
        lv_label_set_text(onTimeLabel, "");
        lv_label_set_text(offTimeLabel, "");
    }
    screens_show(SCREEN_PROGRAM_EDIT);
}

static void update_preset_highlight(int newIndex) {
    if (activePreset >= 0 && activePreset < 4 && presetBtns[activePreset]) {
        lv_obj_set_style_bg_color(presetBtns[activePreset], COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(presetBtns[activePreset], COL_BORDER, 0);
        lv_obj_set_style_border_width(presetBtns[activePreset], 1, 0);
        lv_obj_t* lbl = lv_obj_get_child(presetBtns[activePreset], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    }
    activePreset = newIndex;
    if (activePreset >= 0 && activePreset < 4 && presetBtns[activePreset]) {
        lv_obj_set_style_bg_color(presetBtns[activePreset], COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(presetBtns[activePreset], COL_ACCENT, 0);
        lv_obj_set_style_border_width(presetBtns[activePreset], 2, 0);
        lv_obj_t* lbl = lv_obj_get_child(presetBtns[activePreset], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
    }
}

static void on_time_adj_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (!p || !onTimeLabel) return;
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    p->pulse_on_ms += delta;
    if (p->pulse_on_ms < 100) p->pulse_on_ms = 100;
    if (p->pulse_on_ms > 10000) p->pulse_on_ms = 10000;
    lv_label_set_text_fmt(onTimeLabel, "%dms", (int)p->pulse_on_ms);

    const uint32_t presets[] = {100, 250, 500, 1000};
    int match = -1;
    for (int i = 0; i < 4; i++) {
        if (p->pulse_on_ms == presets[i]) { match = i; break; }
    }
    if (match != activePreset) {
        update_preset_highlight(match);
    }
}

static void off_time_adj_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (!p || !offTimeLabel) return;
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    p->pulse_off_ms += delta;
    if (p->pulse_off_ms < 100) p->pulse_off_ms = 100;
    if (p->pulse_off_ms > 10000) p->pulse_off_ms = 10000;
    lv_label_set_text_fmt(offTimeLabel, "%dms", (int)p->pulse_off_ms);
}

static void preset_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (!p || !onTimeLabel) return;
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    const uint32_t presets[] = {100, 250, 500, 1000};
    p->pulse_on_ms = presets[index];
    p->pulse_off_ms = presets[index];
    lv_label_set_text_fmt(onTimeLabel, "%dms", (int)p->pulse_on_ms);
    if (offTimeLabel) lv_label_set_text_fmt(offTimeLabel, "%dms", (int)p->pulse_off_ms);
    update_preset_highlight(index);
}

void screen_edit_pulse_create() {
    lv_obj_t* screen = screenRoots[SCREEN_EDIT_PULSE];
    lv_obj_clean(screen);

    Preset* p = screen_program_edit_get_preset();
    uint32_t onMs = p ? p->pulse_on_ms : 500;
    uint32_t offMs = p ? p->pulse_off_ms : 300;

    const uint32_t presets[] = {100, 250, 500, 1000};
    activePreset = -1;
    for (int i = 0; i < 4; i++) {
        if (onMs == presets[i] && offMs == presets[i]) { activePreset = i; break; }
    }

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
    lv_label_set_text(titleLabel, "PULSE SETTINGS");
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

    lv_obj_t* onTitle = lv_label_create(screen);
    lv_label_set_text(onTitle, "ON TIME");
    lv_obj_set_style_text_font(onTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(onTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(onTitle, SX(16), SY(50));

    lv_obj_t* onMinusBtn = lv_btn_create(screen);
    lv_obj_set_size(onMinusBtn, SW(76), SH(36));
    lv_obj_set_pos(onMinusBtn, SX(16), SY(56));
    lv_obj_set_style_bg_color(onMinusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(onMinusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(onMinusBtn, 1, 0);
    lv_obj_set_style_radius(onMinusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(onMinusBtn, 0, 0);
    lv_obj_add_event_cb(onMinusBtn, on_time_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-50);

    lv_obj_t* onMinusLbl = lv_label_create(onMinusBtn);
    lv_label_set_text(onMinusLbl, "\xE2\x88\x92");
    lv_obj_set_style_text_font(onMinusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(onMinusLbl, COL_TEXT, 0);
    lv_obj_center(onMinusLbl);

    lv_obj_t* onPanel = lv_obj_create(screen);
    lv_obj_set_size(onPanel, SW(160), SH(36));
    lv_obj_set_pos(onPanel, SX(100), SY(56));
    lv_obj_set_style_bg_color(onPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(onPanel, COL_ACCENT, 0);
    lv_obj_set_style_border_width(onPanel, 2, 0);
    lv_obj_set_style_radius(onPanel, RADIUS_SM, 0);
    lv_obj_set_style_pad_all(onPanel, 0, 0);
    lv_obj_clear_flag(onPanel, LV_OBJ_FLAG_SCROLLABLE);

    onTimeLabel = lv_label_create(onPanel);
    lv_label_set_text_fmt(onTimeLabel, "%dms", (int)onMs);
    lv_obj_set_style_text_font(onTimeLabel, FONT_XXL, 0);
    lv_obj_set_style_text_color(onTimeLabel, COL_ACCENT, 0);
    lv_obj_center(onTimeLabel);

    lv_obj_t* onPlusBtn = lv_btn_create(screen);
    lv_obj_set_size(onPlusBtn, SW(76), SH(36));
    lv_obj_set_pos(onPlusBtn, SX(268), SY(56));
    lv_obj_set_style_bg_color(onPlusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(onPlusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(onPlusBtn, 1, 0);
    lv_obj_set_style_radius(onPlusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(onPlusBtn, 0, 0);
    lv_obj_add_event_cb(onPlusBtn, on_time_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)50);

    lv_obj_t* onPlusLbl = lv_label_create(onPlusBtn);
    lv_label_set_text(onPlusLbl, "+");
    lv_obj_set_style_text_font(onPlusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(onPlusLbl, COL_TEXT, 0);
    lv_obj_center(onPlusLbl);

    lv_obj_t* offTitle = lv_label_create(screen);
    lv_label_set_text(offTitle, "OFF TIME");
    lv_obj_set_style_text_font(offTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(offTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(offTitle, SX(16), SY(110));

    lv_obj_t* offMinusBtn = lv_btn_create(screen);
    lv_obj_set_size(offMinusBtn, SW(76), SH(36));
    lv_obj_set_pos(offMinusBtn, SX(16), SY(116));
    lv_obj_set_style_bg_color(offMinusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(offMinusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(offMinusBtn, 1, 0);
    lv_obj_set_style_radius(offMinusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(offMinusBtn, 0, 0);
    lv_obj_add_event_cb(offMinusBtn, off_time_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-50);

    lv_obj_t* offMinusLbl = lv_label_create(offMinusBtn);
    lv_label_set_text(offMinusLbl, "\xE2\x88\x92");
    lv_obj_set_style_text_font(offMinusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(offMinusLbl, COL_TEXT, 0);
    lv_obj_center(offMinusLbl);

    lv_obj_t* offPanel = lv_obj_create(screen);
    lv_obj_set_size(offPanel, SW(160), SH(36));
    lv_obj_set_pos(offPanel, SX(100), SY(116));
    lv_obj_set_style_bg_color(offPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(offPanel, COL_BORDER, 0);
    lv_obj_set_style_border_width(offPanel, 1, 0);
    lv_obj_set_style_radius(offPanel, RADIUS_SM, 0);
    lv_obj_set_style_pad_all(offPanel, 0, 0);
    lv_obj_clear_flag(offPanel, LV_OBJ_FLAG_SCROLLABLE);

    offTimeLabel = lv_label_create(offPanel);
    lv_label_set_text_fmt(offTimeLabel, "%dms", (int)offMs);
    lv_obj_set_style_text_font(offTimeLabel, FONT_XXL, 0);
    lv_obj_set_style_text_color(offTimeLabel, COL_TEXT, 0);
    lv_obj_center(offTimeLabel);

    lv_obj_t* offPlusBtn = lv_btn_create(screen);
    lv_obj_set_size(offPlusBtn, SW(76), SH(36));
    lv_obj_set_pos(offPlusBtn, SX(268), SY(116));
    lv_obj_set_style_bg_color(offPlusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(offPlusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(offPlusBtn, 1, 0);
    lv_obj_set_style_radius(offPlusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(offPlusBtn, 0, 0);
    lv_obj_add_event_cb(offPlusBtn, off_time_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)50);

    lv_obj_t* offPlusLbl = lv_label_create(offPlusBtn);
    lv_label_set_text(offPlusLbl, "+");
    lv_obj_set_style_text_font(offPlusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(offPlusLbl, COL_TEXT, 0);
    lv_obj_center(offPlusLbl);

    lv_obj_t* sep2 = lv_obj_create(screen);
    lv_obj_set_size(sep2, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep2, 0, SY(162));
    lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    lv_obj_t* presetsTitle = lv_label_create(screen);
    lv_label_set_text(presetsTitle, "PRESETS");
    lv_obj_set_style_text_font(presetsTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(presetsTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(presetsTitle, SX(16), SY(178));

    const uint32_t presetValues[] = {100, 250, 500, 1000};
    const char* presetTexts[] = {"100ms", "250ms", "500ms", "1000ms"};
    const int16_t presetX[] = {SX(16), SX(98), SX(180), SX(262)};
    const int16_t presetW[] = {SW(76), SW(76), SW(76), SW(82)};

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, presetW[i], SH(28));
        lv_obj_set_pos(btn, presetX[i], SY(184));
        lv_obj_set_style_radius(btn, RADIUS_SM, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (i == activePreset) {
            lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
            lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
            lv_obj_set_style_border_color(btn, COL_BORDER, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
        }

        lv_obj_add_event_cb(btn, preset_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, presetTexts[i]);
        lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
        lv_obj_set_style_text_color(lbl, (i == activePreset) ? COL_ACCENT : COL_TEXT_DIM, 0);
        lv_obj_center(lbl);

        presetBtns[i] = btn;
    }
}
