#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include <cstdio>
#include <cstring>

static int editSlot = -1;
static Preset editPreset;

Preset* screen_program_edit_get_preset() { return &editPreset; }

static lv_obj_t* nameInput = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* modeButtonsContainer = nullptr;
static lv_obj_t* settingsLink = nullptr;
static lv_obj_t* settingsTitleLabel = nullptr;
static lv_obj_t* settingsDetailLabel = nullptr;

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

static void update_rpm_display() {
    if (!rpmLabel) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f RPM", editPreset.rpm);
    lv_label_set_text(rpmLabel, buf);
}

static void update_settings_link() {
    if (!settingsTitleLabel || !settingsDetailLabel) return;
    char buf[64];
    switch (editPreset.mode) {
        case STATE_RUNNING:
            lv_label_set_text(settingsTitleLabel, "CONTINUOUS");
            lv_label_set_text(settingsDetailLabel, "No additional settings");
            lv_obj_add_state(settingsLink, LV_STATE_DISABLED);
            break;
        case STATE_PULSE:
            lv_label_set_text(settingsTitleLabel, "PULSE SETTINGS");
            snprintf(buf, sizeof(buf), "ON: %ums \xC2\xB7 OFF: %ums \xE2\x86\x92",
                     editPreset.pulse_on_ms, editPreset.pulse_off_ms);
            lv_label_set_text(settingsDetailLabel, buf);
            lv_obj_clear_state(settingsLink, LV_STATE_DISABLED);
            break;
        case STATE_STEP:
            lv_label_set_text(settingsTitleLabel, "STEP SETTINGS");
            snprintf(buf, sizeof(buf), "ANGLE: %.1f\xC2\xB0 \xE2\x86\x92", editPreset.step_angle);
            lv_label_set_text(settingsDetailLabel, buf);
            lv_obj_clear_state(settingsLink, LV_STATE_DISABLED);
            break;
        case STATE_TIMER:
            lv_label_set_text(settingsTitleLabel, "TIMER SETTINGS");
            snprintf(buf, sizeof(buf), "DURATION: %us \xE2\x86\x92", editPreset.timer_ms / 1000);
            lv_label_set_text(settingsDetailLabel, buf);
            lv_obj_clear_state(settingsLink, LV_STATE_DISABLED);
            break;
        default:
            break;
    }
}

static void mode_select_cb(lv_event_t* e) {
    SystemState mode = (SystemState)(intptr_t)lv_event_get_user_data(e);
    editPreset.mode = mode;

    lv_obj_t* btn = lv_event_get_target(e);
    if (modeButtonsContainer) {
        for (int i = 0; i < 4; i++) {
            lv_obj_t* b = lv_obj_get_child(modeButtonsContainer, i);
            if (b) {
                lv_obj_set_style_bg_color(b, COL_BTN_FILL, 0);
                lv_obj_set_style_border_color(b, COL_BORDER, 0);
                lv_obj_t* lbl = lv_obj_get_child(b, 0);
                if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
            }
        }
    }
    lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);

    update_settings_link();
}

static void rpm_minus_cb(lv_event_t* e) {
    float val = editPreset.rpm - 0.1f;
    if (val < MIN_RPM) val = MIN_RPM;
    editPreset.rpm = val;
    update_rpm_display();
}

static void rpm_plus_cb(lv_event_t* e) {
    float val = editPreset.rpm + 0.1f;
    if (val > MAX_RPM) val = MAX_RPM;
    editPreset.rpm = val;
    update_rpm_display();
}

static void settings_link_cb(lv_event_t* e) {
    switch (editPreset.mode) {
        case STATE_PULSE:
            screen_edit_pulse_create();
            screens_show(SCREEN_EDIT_PULSE);
            break;
        case STATE_STEP:
            screen_edit_step_create();
            screens_show(SCREEN_EDIT_STEP);
            break;
        case STATE_TIMER:
            screen_edit_timer_create();
            screens_show(SCREEN_EDIT_TIMER);
            break;
        default:
            break;
    }
}

static void save_preset_cb(lv_event_t* e) {
    const char* name = lv_textarea_get_text(nameInput);
    if (!name || strlen(name) == 0) return;

    strncpy(editPreset.name, name, 31);
    editPreset.name[31] = '\0';

    if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
        g_presets[editSlot] = editPreset;
        g_presets[editSlot].id = editSlot + 1;
    } else {
        if (g_presets.size() >= MAX_PRESETS) return;
        editPreset.id = g_presets.size() + 1;
        g_presets.push_back(editPreset);
    }

    storage_save_presets();
    screens_show(SCREEN_PROGRAMS);
}

void screen_program_edit_create(int slot) {
    lv_obj_t* screen = screenRoots[SCREEN_PROGRAM_EDIT];
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, COL_BG, 0);

    editSlot = slot;

    if (slot >= 0 && slot < (int)g_presets.size()) {
        editPreset = g_presets[slot];
    } else {
        editPreset.id = 0;
        snprintf(editPreset.name, sizeof(editPreset.name), "New Program");
        editPreset.mode = STATE_RUNNING;
        editPreset.rpm = speed_get_target_rpm();
        editPreset.pulse_on_ms = 500;
        editPreset.pulse_off_ms = 300;
        editPreset.step_angle = 90.0f;
        editPreset.timer_ms = 30000;
    }

    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* backBtn = lv_btn_create(header);
    lv_obj_set_size(backBtn, SW(50), SH(22));
    lv_obj_set_pos(backBtn, SX(16), SY(21));
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(backBtn, 0, 0);
    lv_obj_set_style_radius(backBtn, 0, 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT " BACK");
    lv_obj_set_style_text_font(backLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(backLbl, COL_TEXT_DIM, 0);
    lv_obj_center(backLbl);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "EDIT PROGRAM");
    lv_obj_set_style_text_font(title, FONT_BODY, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title, SCREEN_W);
    lv_obj_set_pos(title, 0, SY(21));

    lv_obj_t* slotLabel = lv_label_create(header);
    int slotNum = slot >= 0 ? slot + 1 : (int)g_presets.size() + 1;
    lv_label_set_text_fmt(slotLabel, "%d / %d", slotNum, MAX_PRESETS);
    lv_obj_set_style_text_font(slotLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(slotLabel, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(slotLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(slotLabel, SCREEN_W - SX(16));
    lv_obj_set_pos(slotLabel, SX(16), SY(21));

    lv_obj_t* sep1 = lv_obj_create(screen);
    lv_obj_set_size(sep1, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep1, 0, HEADER_H);
    lv_obj_set_style_bg_color(sep1, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);
    lv_obj_set_style_radius(sep1, 0, 0);

    nameInput = lv_textarea_create(screen);
    lv_obj_set_size(nameInput, SW(328), SH(28));
    lv_obj_set_pos(nameInput, SX(16), SY(40));
    lv_textarea_set_one_line(nameInput, true);
    lv_textarea_set_max_length(nameInput, 31);
    lv_textarea_set_text(nameInput, editPreset.name);
    lv_textarea_set_cursor_pos(nameInput, strlen(editPreset.name));
    lv_obj_set_style_bg_color(nameInput, COL_BTN_FILL, 0);
    lv_obj_set_style_border_color(nameInput, COL_ACCENT, 0);
    lv_obj_set_style_border_width(nameInput, 2, 0);
    lv_obj_set_style_radius(nameInput, RADIUS_SM, 0);
    lv_obj_set_style_text_color(nameInput, COL_TEXT, 0);
    lv_obj_set_style_text_font(nameInput, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(nameInput, COL_ACCENT, LV_PART_CURSOR);

    modeButtonsContainer = lv_obj_create(screen);
    lv_obj_set_size(modeButtonsContainer, SCREEN_W, SH(28));
    lv_obj_set_pos(modeButtonsContainer, 0, SY(76));
    lv_obj_set_style_bg_opa(modeButtonsContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(modeButtonsContainer, 0, 0);
    lv_obj_set_style_pad_all(modeButtonsContainer, 0, 0);
    lv_obj_set_style_radius(modeButtonsContainer, 0, 0);
    lv_obj_clear_flag(modeButtonsContainer, LV_OBJ_FLAG_SCROLLABLE);

    static const struct {
        const char* label;
        SystemState mode;
        int16_t x;
        int16_t w;
    } modes[] = {
        { "CONT",  STATE_RUNNING, SX(16),  SW(76) },
        { "PULSE", STATE_PULSE,   SX(98),  SW(76) },
        { "STEP",  STATE_STEP,    SX(180), SW(76) },
        { "TIMER", STATE_TIMER,   SX(262), SW(82) }
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(modeButtonsContainer);
        lv_obj_set_size(btn, modes[i].w, SH(28));
        lv_obj_set_pos(btn, modes[i].x, 0);
        lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
        lv_obj_set_style_radius(btn, RADIUS_SM, 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, COL_BORDER, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, mode_select_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)modes[i].mode);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, modes[i].label);
        lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
        lv_obj_center(lbl);

        if (editPreset.mode == modes[i].mode) {
            lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
            lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
            lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
        }
    }

    lv_obj_t* rpmMinus = lv_btn_create(screen);
    lv_obj_set_size(rpmMinus, SW(76), SH(28));
    lv_obj_set_pos(rpmMinus, SX(16), SY(112));
    lv_obj_set_style_bg_color(rpmMinus, COL_BTN_FILL, 0);
    lv_obj_set_style_radius(rpmMinus, RADIUS_SM, 0);
    lv_obj_set_style_border_width(rpmMinus, 2, 0);
    lv_obj_set_style_border_color(rpmMinus, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(rpmMinus, 0, 0);
    lv_obj_set_style_pad_all(rpmMinus, 0, 0);
    lv_obj_add_event_cb(rpmMinus, rpm_minus_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* minusLbl = lv_label_create(rpmMinus);
    lv_label_set_text(minusLbl, "RPM \xE2\x88\x92");
    lv_obj_set_style_text_font(minusLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(minusLbl, COL_TEXT_DIM, 0);
    lv_obj_center(minusLbl);

    lv_obj_t* rpmPanel = lv_obj_create(screen);
    lv_obj_set_size(rpmPanel, SW(160), SH(28));
    lv_obj_set_pos(rpmPanel, SX(100), SY(112));
    lv_obj_set_style_bg_color(rpmPanel, COL_PANEL_BG, 0);
    lv_obj_set_style_radius(rpmPanel, RADIUS_SM, 0);
    lv_obj_set_style_border_width(rpmPanel, 2, 0);
    lv_obj_set_style_border_color(rpmPanel, COL_ACCENT, 0);
    lv_obj_set_style_pad_all(rpmPanel, 0, 0);

    rpmLabel = lv_label_create(rpmPanel);
    update_rpm_display();
    lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
    lv_obj_center(rpmLabel);

    lv_obj_t* rpmPlus = lv_btn_create(screen);
    lv_obj_set_size(rpmPlus, SW(76), SH(28));
    lv_obj_set_pos(rpmPlus, SX(268), SY(112));
    lv_obj_set_style_bg_color(rpmPlus, COL_BTN_FILL, 0);
    lv_obj_set_style_radius(rpmPlus, RADIUS_SM, 0);
    lv_obj_set_style_border_width(rpmPlus, 2, 0);
    lv_obj_set_style_border_color(rpmPlus, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(rpmPlus, 0, 0);
    lv_obj_set_style_pad_all(rpmPlus, 0, 0);
    lv_obj_add_event_cb(rpmPlus, rpm_plus_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* plusLbl = lv_label_create(rpmPlus);
    lv_label_set_text(plusLbl, "RPM +");
    lv_obj_set_style_text_font(plusLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(plusLbl, COL_TEXT_DIM, 0);
    lv_obj_center(plusLbl);

    lv_obj_t* sep2 = lv_obj_create(screen);
    lv_obj_set_size(sep2, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep2, 0, SY(148));
    lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    settingsLink = lv_btn_create(screen);
    lv_obj_set_size(settingsLink, SW(328), SH(32));
    lv_obj_set_pos(settingsLink, SX(16), SY(156));
    lv_obj_set_style_bg_color(settingsLink, COL_BTN_FILL, 0);
    lv_obj_set_style_radius(settingsLink, RADIUS_SM, 0);
    lv_obj_set_style_border_width(settingsLink, 2, 0);
    lv_obj_set_style_border_color(settingsLink, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(settingsLink, 0, 0);
    lv_obj_set_style_pad_left(settingsLink, SX(8), 0);
    lv_obj_set_style_pad_right(settingsLink, SX(8), 0);
    lv_obj_add_event_cb(settingsLink, settings_link_cb, LV_EVENT_CLICKED, nullptr);

    settingsTitleLabel = lv_label_create(settingsLink);
    lv_obj_set_style_text_font(settingsTitleLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(settingsTitleLabel, COL_TEXT, 0);
    lv_obj_align(settingsTitleLabel, LV_ALIGN_LEFT_MID, 0, 0);

    settingsDetailLabel = lv_label_create(settingsLink);
    lv_obj_set_style_text_font(settingsDetailLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(settingsDetailLabel, COL_TEXT_DIM, 0);
    lv_obj_align(settingsDetailLabel, LV_ALIGN_RIGHT_MID, 0, 0);

    update_settings_link();

    lv_obj_t* sep3 = lv_obj_create(screen);
    lv_obj_set_size(sep3, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep3, 0, SY(196));
    lv_obj_set_style_bg_color(sep3, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep3, 0, 0);
    lv_obj_set_style_pad_all(sep3, 0, 0);
    lv_obj_set_style_radius(sep3, 0, 0);

    lv_obj_t* cancelBtn = lv_btn_create(screen);
    lv_obj_set_size(cancelBtn, SW(156), SH(32));
    lv_obj_set_pos(cancelBtn, SX(16), SY(204));
    lv_obj_set_style_bg_color(cancelBtn, COL_BTN_FILL, 0);
    lv_obj_set_style_radius(cancelBtn, RADIUS_SM, 0);
    lv_obj_set_style_border_width(cancelBtn, 2, 0);
    lv_obj_set_style_border_color(cancelBtn, COL_BORDER, 0);
    lv_obj_set_style_shadow_width(cancelBtn, 0, 0);
    lv_obj_add_event_cb(cancelBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "CANCEL");
    lv_obj_set_style_text_font(cancelLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(cancelLbl, COL_TEXT, 0);
    lv_obj_center(cancelLbl);

    lv_obj_t* saveBtn = lv_btn_create(screen);
    lv_obj_set_size(saveBtn, SW(156), SH(32));
    lv_obj_set_pos(saveBtn, SX(188), SY(204));
    lv_obj_set_style_bg_color(saveBtn, COL_BTN_ACTIVE, 0);
    lv_obj_set_style_radius(saveBtn, RADIUS_SM, 0);
    lv_obj_set_style_border_width(saveBtn, 2, 0);
    lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
    lv_obj_set_style_shadow_width(saveBtn, 0, 0);
    lv_obj_add_event_cb(saveBtn, save_preset_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "SAVE");
    lv_obj_set_style_text_font(saveLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(saveLbl, COL_ACCENT, 0);
    lv_obj_center(saveLbl);

    LOG_I("Screen program edit: created slot %d", slot);
}
