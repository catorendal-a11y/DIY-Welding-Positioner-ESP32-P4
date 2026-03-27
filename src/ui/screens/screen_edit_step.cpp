#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include <cstdio>
#include <cstdint>

static lv_obj_t* angleLabel = nullptr;
static lv_obj_t* presetBtns[5] = {nullptr};
static int activePreset = -1;
static lv_obj_t* dirBtns[2] = {nullptr};

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAM_EDIT); }

static void update_preset_highlight(int newIndex) {
    if (activePreset >= 0 && activePreset < 5 && presetBtns[activePreset]) {
        lv_obj_set_style_bg_color(presetBtns[activePreset], COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(presetBtns[activePreset], COL_BORDER, 0);
        lv_obj_set_style_border_width(presetBtns[activePreset], 1, 0);
        lv_obj_t* lbl = lv_obj_get_child(presetBtns[activePreset], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    }
    activePreset = newIndex;
    if (activePreset >= 0 && activePreset < 5 && presetBtns[activePreset]) {
        lv_obj_set_style_bg_color(presetBtns[activePreset], COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(presetBtns[activePreset], COL_ACCENT, 0);
        lv_obj_set_style_border_width(presetBtns[activePreset], 2, 0);
        lv_obj_t* lbl = lv_obj_get_child(presetBtns[activePreset], 0);
        if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
    }
}

static void angle_adj_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (!p || !angleLabel) return;
    float delta = (float)(intptr_t)lv_event_get_user_data(e);
    p->step_angle += delta;
    if (p->step_angle < 1.0f) p->step_angle = 1.0f;
    if (p->step_angle > 3600.0f) p->step_angle = 3600.0f;
    lv_label_set_text_fmt(angleLabel, "%.0f\xC2\xB0", p->step_angle);

    const float presets[] = {45.0f, 90.0f, 180.0f, 360.0f, 720.0f};
    int match = -1;
    for (int i = 0; i < 5; i++) {
        if (p->step_angle == presets[i]) { match = i; break; }
    }
    if (match != activePreset) {
        update_preset_highlight(match);
    }
}

static void preset_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (!p || !angleLabel) return;
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    const float presets[] = {45.0f, 90.0f, 180.0f, 360.0f, 720.0f};
    p->step_angle = presets[index];
    lv_label_set_text_fmt(angleLabel, "%.0f\xC2\xB0", p->step_angle);
    update_preset_highlight(index);
}

static void dir_cb(lv_event_t* e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    for (int i = 0; i < 2; i++) {
        if (dirBtns[i]) {
            if (i == index) {
                lv_obj_set_style_bg_color(dirBtns[i], COL_BTN_ACTIVE, 0);
                lv_obj_set_style_border_color(dirBtns[i], COL_ACCENT, 0);
                lv_obj_set_style_border_width(dirBtns[i], 2, 0);
                lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
                if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
            } else {
                lv_obj_set_style_bg_color(dirBtns[i], COL_BTN_FILL, 0);
                lv_obj_set_style_border_color(dirBtns[i], COL_BORDER, 0);
                lv_obj_set_style_border_width(dirBtns[i], 1, 0);
                lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
                if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
            }
        }
    }
}

void screen_edit_step_create() {
    lv_obj_t* screen = screenRoots[SCREEN_EDIT_STEP];
    lv_obj_clean(screen);

    Preset* p = screen_program_edit_get_preset();
    float currentAngle = p ? p->step_angle : 180.0f;

    const float presets[] = {45.0f, 90.0f, 180.0f, 360.0f, 720.0f};
    activePreset = -1;
    for (int i = 0; i < 5; i++) {
        if (currentAngle == presets[i]) { activePreset = i; break; }
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
    lv_label_set_text(titleLabel, "STEP SETTINGS");
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

    lv_obj_t* angleTitle = lv_label_create(screen);
    lv_label_set_text(angleTitle, "TARGET ANGLE");
    lv_obj_set_style_text_font(angleTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(angleTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(angleTitle, SX(16), SY(50));

    lv_obj_t* minusBtn = lv_btn_create(screen);
    lv_obj_set_size(minusBtn, SW(76), SH(36));
    lv_obj_set_pos(minusBtn, SX(16), SY(56));
    lv_obj_set_style_bg_color(minusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(minusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(minusBtn, 1, 0);
    lv_obj_set_style_radius(minusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(minusBtn, 0, 0);
    lv_obj_add_event_cb(minusBtn, angle_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-5);

    lv_obj_t* minusLbl = lv_label_create(minusBtn);
    lv_label_set_text(minusLbl, "\xE2\x88\x92");
    lv_obj_set_style_text_font(minusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(minusLbl, COL_TEXT, 0);
    lv_obj_center(minusLbl);

    lv_obj_t* panel = lv_obj_create(screen);
    lv_obj_set_size(panel, SW(160), SH(36));
    lv_obj_set_pos(panel, SX(100), SY(56));
    lv_obj_set_style_bg_color(panel, COL_PANEL_BG, 0);
    lv_obj_set_style_border_color(panel, COL_ACCENT, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, RADIUS_SM, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    angleLabel = lv_label_create(panel);
    lv_label_set_text_fmt(angleLabel, "%.0f\xC2\xB0", currentAngle);
    lv_obj_set_style_text_font(angleLabel, FONT_XXL, 0);
    lv_obj_set_style_text_color(angleLabel, COL_ACCENT, 0);
    lv_obj_center(angleLabel);

    lv_obj_t* plusBtn = lv_btn_create(screen);
    lv_obj_set_size(plusBtn, SW(76), SH(36));
    lv_obj_set_pos(plusBtn, SX(268), SY(56));
    lv_obj_set_style_bg_color(plusBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(plusBtn, COL_BORDER, 0);
    lv_obj_set_style_border_width(plusBtn, 1, 0);
    lv_obj_set_style_radius(plusBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(plusBtn, 0, 0);
    lv_obj_add_event_cb(plusBtn, angle_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)5);

    lv_obj_t* plusLbl = lv_label_create(plusBtn);
    lv_label_set_text(plusLbl, "+");
    lv_obj_set_style_text_font(plusLbl, FONT_LARGE, 0);
    lv_obj_set_style_text_color(plusLbl, COL_TEXT, 0);
    lv_obj_center(plusLbl);

    lv_obj_t* sep2 = lv_obj_create(screen);
    lv_obj_set_size(sep2, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep2, 0, SY(102));
    lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    lv_obj_t* presetsTitle = lv_label_create(screen);
    lv_label_set_text(presetsTitle, "ANGLE PRESETS");
    lv_obj_set_style_text_font(presetsTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(presetsTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(presetsTitle, SX(16), SY(118));

    const float presetValues[] = {45.0f, 90.0f, 180.0f, 360.0f, 720.0f};
    const char* presetTexts[] = {"45\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "360\xC2\xB0", "720\xC2\xB0"};
    const int16_t presetX[] = {SX(16), SX(82), SX(148), SX(214), SX(280)};
    const int16_t presetW[] = {SW(60), SW(60), SW(60), SW(60), SW(64)};

    for (int i = 0; i < 5; i++) {
        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, presetW[i], SH(28));
        lv_obj_set_pos(btn, presetX[i], SY(124));
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

    lv_obj_t* sep3 = lv_obj_create(screen);
    lv_obj_set_size(sep3, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep3, 0, SY(162));
    lv_obj_set_style_bg_color(sep3, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep3, 0, 0);
    lv_obj_set_style_pad_all(sep3, 0, 0);
    lv_obj_set_style_radius(sep3, 0, 0);

    lv_obj_t* dirTitle = lv_label_create(screen);
    lv_label_set_text(dirTitle, "DIRECTION");
    lv_obj_set_style_text_font(dirTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(dirTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(dirTitle, SX(16), SY(178));

    const char* dirTexts[] = {"CW \xE2\x86\xBB", "CCW \xE2\x86\xBA"};
    const int16_t dirX[] = {SX(16), SX(188)};

    for (int i = 0; i < 2; i++) {
        lv_obj_t* btn = lv_btn_create(screen);
        lv_obj_set_size(btn, SW(156), SH(28));
        lv_obj_set_pos(btn, dirX[i], SY(184));
        lv_obj_set_style_radius(btn, RADIUS_SM, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (i == 0) {
            lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
            lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
            lv_obj_set_style_border_color(btn, COL_BORDER, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
        }

        lv_obj_add_event_cb(btn, dir_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, dirTexts[i]);
        lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
        lv_obj_set_style_text_color(lbl, (i == 0) ? COL_ACCENT : COL_TEXT_DIM, 0);
        lv_obj_center(lbl);

        dirBtns[i] = btn;
    }
}
