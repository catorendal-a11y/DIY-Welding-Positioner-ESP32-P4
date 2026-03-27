#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../motor/speed.h"
#include "../../control/control.h"
#include <cstdio>

static lv_obj_t* programList = nullptr;
static lv_obj_t* newBtn = nullptr;
static lv_obj_t* countLabel = nullptr;
static lv_obj_t* scrollHint = nullptr;

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void load_preset_cb(lv_event_t* e) {
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    Preset* p = storage_get_preset(id);
    if (p) {
        LOG_I("Loading Preset %d: %s", id, p->name);
        speed_slider_set(p->rpm);
        if (p->mode == STATE_RUNNING) {
            control_start_continuous();
        } else if (p->mode == STATE_PULSE) {
            control_start_pulse(p->pulse_on_ms, p->pulse_off_ms);
        } else if (p->mode == STATE_STEP) {
            control_start_step(p->step_angle);
        } else if (p->mode == STATE_TIMER) {
            control_start_timer(p->timer_ms / 1000);
        }
        screens_show(SCREEN_MAIN);
    }
}

static void edit_preset_cb(lv_event_t* e) {
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    screen_program_edit_create(slot);
    screens_show(SCREEN_PROGRAM_EDIT);
}

static void delete_preset_cb(lv_event_t* e) {
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot >= 0 && slot < (int)g_presets.size()) {
        g_presets.erase(g_presets.begin() + slot);
        for (size_t i = 0; i < g_presets.size(); i++) {
            g_presets[i].id = i + 1;
        }
        storage_save_presets();
        screen_programs_update();
    }
}

static void new_program_cb(lv_event_t* e) {
    screen_program_edit_create(-1);
    screens_show(SCREEN_PROGRAM_EDIT);
}

static lv_obj_t* create_action_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                   int16_t w, int16_t h, const char* symbol,
                                   lv_color_t borderColor, lv_color_t bgColor) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bgColor, 0);
    lv_obj_set_style_border_color(btn, borderColor, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, borderColor, 0);
    lv_obj_center(label);

    return btn;
}

void screen_programs_create() {
    lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];
    lv_obj_set_style_bg_color(screen, COL_BG, 0);

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, SW(50), SH(14));
    lv_obj_set_pos(backBtn, SX(8), SY(12));
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(backBtn, 0, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_set_style_radius(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* backLabel = lv_label_create(header);
    lv_label_set_text(backLabel, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(backLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(backLabel, SX(16), SY(21));

    lv_obj_t* titleLabel = lv_label_create(header);
    lv_label_set_text(titleLabel, "PROGRAMS");
    lv_obj_set_style_text_font(titleLabel, FONT_BODY, 0);
    lv_obj_set_style_text_color(titleLabel, COL_TEXT, 0);
    lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(titleLabel, SCREEN_W);
    lv_obj_set_pos(titleLabel, 0, SY(21));

    countLabel = lv_label_create(header);
    lv_label_set_text(countLabel, "0 / 10");
    lv_obj_set_style_text_font(countLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(countLabel, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(countLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(countLabel, SCREEN_W - SX(16));
    lv_obj_set_pos(countLabel, SX(16), SY(21));

    lv_obj_t* sep = lv_obj_create(screen);
    lv_obj_set_size(sep, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep, 0, HEADER_H);
    lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);

    newBtn = lv_btn_create(screen);
    lv_obj_set_size(newBtn, SW(328), SH(28));
    lv_obj_set_pos(newBtn, SX(16), SY(40));
    lv_obj_set_style_bg_color(newBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(newBtn, 2, 0);
    lv_obj_set_style_radius(newBtn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(newBtn, 0, 0);
    lv_obj_add_event_cb(newBtn, new_program_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* newLabel = lv_label_create(newBtn);
    lv_label_set_text(newLabel, LV_SYMBOL_PLUS " NEW PROGRAM");
    lv_obj_set_style_text_font(newLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(newLabel, COL_ACCENT, 0);
    lv_obj_center(newLabel);

    int16_t listY = SY(74);
    int16_t listH = SCREEN_H - listY - SH(16);
    programList = lv_obj_create(screen);
    lv_obj_set_size(programList, SW(328), listH);
    lv_obj_set_pos(programList, SX(16), listY);
    lv_obj_set_style_bg_color(programList, COL_BG, 0);
    lv_obj_set_style_border_width(programList, 0, 0);
    lv_obj_set_style_pad_all(programList, 0, 0);
    lv_obj_set_style_radius(programList, 0, 0);
    lv_obj_set_scrollbar_mode(programList, LV_SCROLLBAR_MODE_ACTIVE);

    scrollHint = lv_label_create(screen);
    lv_label_set_text(scrollHint, LV_SYMBOL_DOWN " Scroll for more " LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(scrollHint, FONT_SMALL, 0);
    lv_obj_set_style_text_color(scrollHint, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(scrollHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(scrollHint, SCREEN_W);
    lv_obj_set_pos(scrollHint, 0, SCREEN_H - SH(12));
}

void screen_programs_update() {
    if (!programList) return;
    lv_obj_clean(programList);

    if (countLabel) {
        lv_label_set_text_fmt(countLabel, "%d / %d", (int)g_presets.size(), MAX_PRESETS);
    }

    if (g_presets.size() >= MAX_PRESETS) {
        lv_obj_set_style_bg_color(newBtn, COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(newBtn, COL_BORDER, 0);
        lv_obj_set_style_opa(newBtn, LV_OPA_50, 0);
        lv_obj_add_state(newBtn, LV_STATE_DISABLED);
    } else {
        lv_obj_set_style_bg_color(newBtn, COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
        lv_obj_set_style_opa(newBtn, LV_OPA_COVER, 0);
        lv_obj_clear_state(newBtn, LV_STATE_DISABLED);
    }

    if (scrollHint) {
        if (g_presets.empty()) {
            lv_obj_add_flag(scrollHint, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(scrollHint, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (g_presets.empty()) {
        lv_obj_t* emptyLabel = lv_label_create(programList);
        lv_label_set_text(emptyLabel, "No programs yet.\nTap NEW PROGRAM to create one.");
        lv_obj_set_style_text_font(emptyLabel, FONT_NORMAL, 0);
        lv_obj_set_style_text_color(emptyLabel, COL_TEXT_DIM, 0);
        lv_obj_set_style_text_align(emptyLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(emptyLabel, SW(328), SH(60));
        lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    int16_t cardW = SW(328);
    int16_t cardH = SH(48);
    int16_t gap = 8;
    int16_t btnH = SH(34);
    int16_t btnY = (cardH - btnH) / 2;

    for (size_t i = 0; i < g_presets.size(); i++) {
        const auto& p = g_presets[i];
        int16_t yPos = (int16_t)(i * (cardH + gap));

        lv_obj_t* card = lv_obj_create(programList);
        lv_obj_set_size(card, cardW, cardH);
        lv_obj_set_pos(card, 0, yPos);
        lv_obj_set_style_bg_color(card, COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(card, COL_BORDER, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_radius(card, RADIUS_SM, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        char nameBuf[48];
        snprintf(nameBuf, sizeof(nameBuf), "%d: %s", p.id, p.name);
        lv_obj_t* nameLabel = lv_label_create(card);
        lv_label_set_text(nameLabel, nameBuf);
        lv_obj_set_style_text_font(nameLabel, FONT_NORMAL, 0);
        lv_obj_set_style_text_color(nameLabel, COL_TEXT, 0);
        lv_obj_set_width(nameLabel, SX(220));
        lv_obj_set_pos(nameLabel, SX(4), 10);

        char detailBuf[96];
        const char* modeName = control_state_name(p.mode);
        switch (p.mode) {
            case STATE_PULSE:
                snprintf(detailBuf, sizeof(detailBuf), "%s · %.1f RPM · ON: %ums · OFF: %ums",
                         modeName, p.rpm, p.pulse_on_ms, p.pulse_off_ms);
                break;
            case STATE_STEP:
                snprintf(detailBuf, sizeof(detailBuf), "%s · %.1f RPM · %.1f°",
                         modeName, p.rpm, p.step_angle);
                break;
            case STATE_TIMER:
                snprintf(detailBuf, sizeof(detailBuf), "%s · %.1f RPM · %us",
                         modeName, p.rpm, p.timer_ms / 1000);
                break;
            default:
                snprintf(detailBuf, sizeof(detailBuf), "%s · %.1f RPM",
                         modeName, p.rpm);
                break;
        }

        lv_obj_t* detailLabel = lv_label_create(card);
        lv_label_set_text(detailLabel, detailBuf);
        lv_obj_set_style_text_font(detailLabel, FONT_SMALL, 0);
        lv_obj_set_style_text_color(detailLabel, COL_ACCENT, 0);
        lv_obj_set_width(detailLabel, SX(220));
        lv_obj_set_pos(detailLabel, SX(4), 36);

        int16_t playX = SX(228);
        int16_t editX = SX(272);
        int16_t delX = SX(300);
        int16_t btnW1 = SW(40);
        int16_t btnW2 = SW(24);

        lv_obj_t* playBtn = create_action_btn(card, playX, btnY, btnW1, btnH,
                                              LV_SYMBOL_PLAY, COL_GREEN, COL_BTN_ACTIVE);
        lv_obj_add_event_cb(playBtn, load_preset_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)p.id);

        lv_obj_t* editBtn = create_action_btn(card, editX, btnY, btnW2, btnH,
                                              LV_SYMBOL_EDIT, COL_ACCENT, COL_BTN_ACTIVE);
        lv_obj_add_event_cb(editBtn, edit_preset_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);

        lv_obj_t* delBtn = create_action_btn(card, delX, btnY, btnW2, btnH,
                                             LV_SYMBOL_CLOSE, COL_RED, COL_BTN_FILL);
        lv_obj_add_event_cb(delBtn, delete_preset_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);
    }
}
