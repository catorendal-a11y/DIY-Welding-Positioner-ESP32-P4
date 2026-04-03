// TIG Rotator Controller - Edit Timer Settings Screen
// Matching new_ui.svg: Duration editing with scale, RPM, direction, auto-stop

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"
#include "../../motor/microstep.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* durationLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* dirBtns[2] = {nullptr};
static lv_obj_t* autoStopBtns[2] = {nullptr};
static lv_obj_t* computedLabel = nullptr;
static lv_obj_t* scaleBar = nullptr;

// Local edit state (cancel-safe — only written to preset on SAVE)
static uint32_t editDurationMs = 30000;
static float editRpm = 2.0f;
static int editDir = 0;           // 0=CW, 1=CCW
static bool editAutoStop = true;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_computed_info() {
    if (!computedLabel) return;

    uint32_t durSec = editDurationMs / 1000;
    float rpm = editRpm;
    // Revolutions = RPM * duration_minutes = rpm * (durSec / 60.0)
    float revolutions = rpm * (durSec / 60.0f);
    float degrees = revolutions * 360.0f;
    long totalSteps = (long)(revolutions * GEAR_RATIO * microstep_get_steps_per_rev());

    lv_label_set_text_fmt(computedLabel,
        "TOTAL %.1f rev (%.0f deg) * STEPS %ld",
        revolutions, degrees, totalSteps);
}

static void update_duration_display() {
    if (!durationLabel) return;
    uint32_t durSec = editDurationMs / 1000;
    uint32_t min = durSec / 60;
    uint32_t sec = durSec % 60;
    lv_label_set_text_fmt(durationLabel, "%02d:%02d", (int)min, (int)sec);
    update_computed_info();
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
    screen_program_edit_update_ui();
    screens_show(SCREEN_PROGRAM_EDIT);
}

static void duration_adj_cb(lv_event_t* e) {
    int delta = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
    int32_t dur = (int32_t)(editDurationMs / 1000) + delta;
    if (dur < 1) dur = 1;
    if (dur > 600) dur = 600;
    editDurationMs = (uint32_t)dur * 1000;
    update_duration_display();
}

static void rpm_adj_cb(lv_event_t* e) {
    if (!rpmLabel) return;
    int delta = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
    editRpm += delta * 0.1f;
    if (editRpm < MIN_RPM) editRpm = MIN_RPM;
    if (editRpm > MAX_RPM) editRpm = MAX_RPM;
    lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
    update_computed_info();
}

static void dir_cb(lv_event_t* e) {
    int index = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
    editDir = index;

    for (int i = 0; i < 2; i++) {
        if (!dirBtns[i]) continue;
        if (i == index) {
            lv_obj_set_style_bg_color(dirBtns[i], COL_BG_ACTIVE, 0);
            lv_obj_set_style_border_color(dirBtns[i], COL_ACCENT, 0);
            lv_obj_set_style_border_width(dirBtns[i], 2, 0);
            lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
        } else {
            lv_obj_set_style_bg_color(dirBtns[i], COL_BTN_BG, 0);
            lv_obj_set_style_border_color(dirBtns[i], COL_BORDER, 0);
            lv_obj_set_style_border_width(dirBtns[i], 1, 0);
            lv_obj_t* lbl = lv_obj_get_child(dirBtns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
        }
    }
}

static void auto_stop_cb(lv_event_t* e) {
    int index = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
    editAutoStop = (index == 0);  // 0=ON, 1=OFF

    for (int i = 0; i < 2; i++) {
        if (!autoStopBtns[i]) continue;
        bool isActive = ((i == 0 && editAutoStop) || (i == 1 && !editAutoStop));
        if (isActive) {
            lv_obj_set_style_bg_color(autoStopBtns[i], COL_BG_ACTIVE, 0);
            lv_obj_set_style_border_color(autoStopBtns[i], COL_ACCENT, 0);
            lv_obj_set_style_border_width(autoStopBtns[i], 2, 0);
            lv_obj_t* lbl = lv_obj_get_child(autoStopBtns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
        } else {
            lv_obj_set_style_bg_color(autoStopBtns[i], COL_BTN_BG, 0);
            lv_obj_set_style_border_color(autoStopBtns[i], COL_BORDER, 0);
            lv_obj_set_style_border_width(autoStopBtns[i], 1, 0);
            lv_obj_t* lbl = lv_obj_get_child(autoStopBtns[i], 0);
            if (lbl) lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
        }
    }
}

static void cancel_cb(lv_event_t* e) {
    screen_program_edit_update_ui();
    screens_show(SCREEN_PROGRAM_EDIT);
}

static void save_cb(lv_event_t* e) {
    Preset* p = screen_program_edit_get_preset();
    if (p) {
        p->timer_ms = editDurationMs;
        p->rpm = editRpm;
        p->direction = (uint8_t)editDir;
        p->timer_auto_stop = editAutoStop ? 1 : 0;
    }
    screen_program_edit_update_ui();
    screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE - new_ui.svg: Edit Timer screen
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_timer_create() {
    lv_obj_t* screen = screenRoots[SCREEN_EDIT_TIMER];
    lv_obj_clean(screen);

    Preset* p = screen_program_edit_get_preset();
    editDurationMs = p ? p->timer_ms : 30000;
    editRpm = p ? p->rpm : 2.0f;
    editDir = p ? (int)p->direction : 0;
    editAutoStop = p ? (p->timer_auto_stop != 0) : true;
    uint32_t durSec = editDurationMs / 1000;

    // ── Header bar ──
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "EDIT TIMER");
    lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(title, COL_ACCENT, 0);
    lv_obj_set_pos(title, 16, 8);

    // [ESC] button at right of header
    lv_obj_t* escBtn = lv_button_create(header);
    lv_obj_set_size(escBtn, 60, 24);
    lv_obj_set_pos(escBtn, SCREEN_W - 60 - PAD_X, 3);
    lv_obj_set_style_bg_color(escBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(escBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(escBtn, 1, 0);
    lv_obj_set_style_border_color(escBtn, COL_BORDER, 0);
    lv_obj_add_event_cb(escBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* escLbl = lv_label_create(escBtn);
    lv_label_set_text(escLbl, "[ESC]");
    lv_obj_set_style_text_font(escLbl, FONT_SMALL, 0);
    lv_obj_set_style_text_color(escLbl, COL_TEXT_DIM, 0);
    lv_obj_center(escLbl);

    // ── DURATION section (y=58) ──
    // "DURATION" label
    lv_obj_t* durTitle = lv_label_create(screen);
    lv_label_set_text(durTitle, "DURATION");
    lv_obj_set_style_text_font(durTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(durTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(durTitle, 20, 38);

    // Large duration value "00:30" in COL_ACCENT
    durationLabel = lv_label_create(screen);
    uint32_t dMin = durSec / 60;
    uint32_t dSec = durSec % 60;
    lv_label_set_text_fmt(durationLabel, "%02d:%02d", (int)dMin, (int)dSec);
    lv_obj_set_style_text_font(durationLabel, FONT_HUGE, 0);
    lv_obj_set_style_text_color(durationLabel, COL_ACCENT, 0);
    lv_obj_set_pos(durationLabel, 20, 52);

    // "sec" unit label
    lv_obj_t* secUnit = lv_label_create(screen);
    lv_label_set_text(secUnit, "sec");
    lv_obj_set_style_text_font(secUnit, FONT_SMALL, 0);
    lv_obj_set_style_text_color(secUnit, COL_TEXT_DIM, 0);
    // Position to right of FONT_HUGE text (approx width ~160 for "00:30")
    lv_obj_set_pos(secUnit, 190, 80);

    // ── Progress bar (760x3) with scale labels ──
    // Bar at y=104
    scaleBar = lv_bar_create(screen);
    lv_obj_set_size(scaleBar, 760, 3);
    lv_obj_set_pos(scaleBar, 20, 104);
    lv_bar_set_range(scaleBar, 0, 600);
    lv_bar_set_value(scaleBar, (int32_t)durSec, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(scaleBar, COL_GAUGE_BG, 0);
    lv_obj_set_style_border_width(scaleBar, 0, 0);
    lv_obj_set_style_pad_all(scaleBar, 0, 0);
    lv_obj_set_style_radius(scaleBar, 1, 0);
    lv_obj_set_style_bg_color(scaleBar, COL_ACCENT, LV_PART_INDICATOR);

    // Scale labels below the bar
    const uint32_t scaleMarks[] = {0, 60, 120, 300, 480, 600};
    const char* scaleTexts[] = {"0", "60s", "2min", "5min", "8min", "10min"};
    // Distribute 6 labels across 760px starting at x=20
    for (int i = 0; i < 6; i++) {
        lv_obj_t* scl = lv_label_create(screen);
        lv_label_set_text(scl, scaleTexts[i]);
        lv_obj_set_style_text_font(scl, FONT_SMALL, 0);
        lv_obj_set_style_text_color(scl, COL_TEXT_VDIM, 0);
        int xPos = 20 + (int)(scaleMarks[i] * 760 / 600);
        lv_obj_set_pos(scl, xPos, 109);
    }

    // ── 5 Adjustment buttons (y=128) ──
    // -1s, +1s, -10s, +10s, -/+1min
    // Layout: row of 5 buttons
    const int adjY = 128;
    const int adjW = 120;
    const int adjW_wide = 120;
    const int adjH = 40;
    const int adjGap = 8;
    // Total: 6*120 + 5*8 = 720 + 40 = 760, start = (800-760)/2 = 20
    const int adjStartX = 20;

    const int deltas[] = {-1, 1, -10, 10, -60, 60};
    const char* adjLabels[] = {"-1s", "+1s", "-10s", "+10s", "-1min", "+1min"};
    const int adjWidths[] = {adjW, adjW, adjW, adjW, adjW_wide, adjW_wide};

    int xPos = adjStartX;
    for (int i = 0; i < 6; i++) {
        lv_obj_t* btn = lv_button_create(screen);
        lv_obj_set_size(btn, adjWidths[i], adjH);
        lv_obj_set_pos(btn, xPos, adjY);
        lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
        lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, COL_BORDER, 0);
        lv_obj_add_event_cb(btn, duration_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)deltas[i]);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, adjLabels[i]);
        lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
        lv_obj_center(lbl);

        xPos += adjWidths[i] + adjGap;
    }

    // ── Separator (y=178) ──
    lv_obj_t* sep1 = lv_obj_create(screen);
    lv_obj_set_size(sep1, SCREEN_W, 2);
    lv_obj_set_pos(sep1, 0, 178);
    lv_obj_set_style_bg_color(sep1, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);
    lv_obj_set_style_radius(sep1, 0, 0);
    lv_obj_remove_flag(sep1, LV_OBJ_FLAG_SCROLLABLE);

    // ── RPM section (left, y=192) ──
    lv_obj_t* rpmTitle = lv_label_create(screen);
    lv_label_set_text(rpmTitle, "RPM");
    lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(rpmTitle, 20, 190);

    rpmLabel = lv_label_create(screen);
    lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
    lv_obj_set_style_text_font(rpmLabel, FONT_XL, 0);
    lv_obj_set_style_text_color(rpmLabel, COL_TEXT_BRIGHT, 0);
    lv_obj_set_pos(rpmLabel, 20, 204);

    // RPM - button (48x30)
    lv_obj_t* rpmMinusBtn = lv_button_create(screen);
    lv_obj_set_size(rpmMinusBtn, 48, 30);
    lv_obj_set_pos(rpmMinusBtn, 100, 206);
    lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(rpmMinusBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(rpmMinusBtn, 1, 0);
    lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER_SM, 0);
    lv_obj_add_event_cb(rpmMinusBtn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(-1));

    lv_obj_t* rpmMinusLbl = lv_label_create(rpmMinusBtn);
    lv_label_set_text(rpmMinusLbl, "-");
    lv_obj_set_style_text_font(rpmMinusLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(rpmMinusLbl, COL_TEXT, 0);
    lv_obj_center(rpmMinusLbl);

    // RPM + button (48x30)
    lv_obj_t* rpmPlusBtn = lv_button_create(screen);
    lv_obj_set_size(rpmPlusBtn, 48, 30);
    lv_obj_set_pos(rpmPlusBtn, 152, 206);
    lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(rpmPlusBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(rpmPlusBtn, 1, 0);
    lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER_SM, 0);
    lv_obj_add_event_cb(rpmPlusBtn, rpm_adj_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(1));

    lv_obj_t* rpmPlusLbl = lv_label_create(rpmPlusBtn);
    lv_label_set_text(rpmPlusLbl, "+");
    lv_obj_set_style_text_font(rpmPlusLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(rpmPlusLbl, COL_TEXT, 0);
    lv_obj_center(rpmPlusLbl);

    // ── DIRECTION section (right, y=192) ──
    lv_obj_t* dirTitle = lv_label_create(screen);
    lv_label_set_text(dirTitle, "DIRECTION");
    lv_obj_set_style_text_font(dirTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(dirTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(dirTitle, 420, 190);

    int dirIdx = editDir;

    // CW button (168x38)
    dirBtns[0] = lv_button_create(screen);
    lv_obj_set_size(dirBtns[0], 168, 38);
    lv_obj_set_pos(dirBtns[0], 420, 204);
    lv_obj_set_style_radius(dirBtns[0], RADIUS_BTN, 0);
    lv_obj_add_event_cb(dirBtns[0], dir_cb, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    if (dirIdx == 0) {
        lv_obj_set_style_bg_color(dirBtns[0], COL_BG_ACTIVE, 0);
        lv_obj_set_style_border_color(dirBtns[0], COL_ACCENT, 0);
        lv_obj_set_style_border_width(dirBtns[0], 2, 0);
    } else {
        lv_obj_set_style_bg_color(dirBtns[0], COL_BTN_BG, 0);
        lv_obj_set_style_border_color(dirBtns[0], COL_BORDER, 0);
        lv_obj_set_style_border_width(dirBtns[0], 1, 0);
    }

    lv_obj_t* cwLbl = lv_label_create(dirBtns[0]);
    lv_label_set_text(cwLbl, "CW");
    lv_obj_set_style_text_font(cwLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(cwLbl, (dirIdx == 0) ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(cwLbl);

    // CCW button (168x38)
    dirBtns[1] = lv_button_create(screen);
    lv_obj_set_size(dirBtns[1], 168, 38);
    lv_obj_set_pos(dirBtns[1], 612, 204);
    lv_obj_set_style_radius(dirBtns[1], RADIUS_BTN, 0);
    lv_obj_add_event_cb(dirBtns[1], dir_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    if (dirIdx == 1) {
        lv_obj_set_style_bg_color(dirBtns[1], COL_BG_ACTIVE, 0);
        lv_obj_set_style_border_color(dirBtns[1], COL_ACCENT, 0);
        lv_obj_set_style_border_width(dirBtns[1], 2, 0);
    } else {
        lv_obj_set_style_bg_color(dirBtns[1], COL_BTN_BG, 0);
        lv_obj_set_style_border_color(dirBtns[1], COL_BORDER, 0);
        lv_obj_set_style_border_width(dirBtns[1], 1, 0);
    }

    lv_obj_t* ccwLbl = lv_label_create(dirBtns[1]);
    lv_label_set_text(ccwLbl, "CCW");
    lv_obj_set_style_text_font(ccwLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(ccwLbl, (dirIdx == 1) ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(ccwLbl);

    // ── AUTO-STOP AT END toggle (y=252) ──
    lv_obj_t* astTitle = lv_label_create(screen);
    lv_label_set_text(astTitle, "AUTO-STOP AT END");
    lv_obj_set_style_text_font(astTitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(astTitle, COL_TEXT_DIM, 0);
    lv_obj_set_pos(astTitle, 20, 254);

    // ON button (130x32)
    autoStopBtns[0] = lv_button_create(screen);
    lv_obj_set_size(autoStopBtns[0], 130, 32);
    lv_obj_set_pos(autoStopBtns[0], 240, 252);
    lv_obj_set_style_radius(autoStopBtns[0], RADIUS_BTN, 0);
    lv_obj_add_event_cb(autoStopBtns[0], auto_stop_cb, LV_EVENT_CLICKED, (void*)(intptr_t)0);

    lv_obj_set_style_bg_color(autoStopBtns[0], editAutoStop ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_color(autoStopBtns[0], editAutoStop ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_border_width(autoStopBtns[0], editAutoStop ? 2 : 1, 0);

    lv_obj_t* onLbl = lv_label_create(autoStopBtns[0]);
    lv_label_set_text(onLbl, "ON");
    lv_obj_set_style_text_font(onLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(onLbl, editAutoStop ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(onLbl);

    // OFF button (130x32)
    autoStopBtns[1] = lv_button_create(screen);
    lv_obj_set_size(autoStopBtns[1], 130, 32);
    lv_obj_set_pos(autoStopBtns[1], 378, 252);
    lv_obj_set_style_radius(autoStopBtns[1], RADIUS_BTN, 0);
    lv_obj_add_event_cb(autoStopBtns[1], auto_stop_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    lv_obj_set_style_bg_color(autoStopBtns[1], !editAutoStop ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_color(autoStopBtns[1], !editAutoStop ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_border_width(autoStopBtns[1], !editAutoStop ? 2 : 1, 0);

    lv_obj_t* offLbl = lv_label_create(autoStopBtns[1]);
    lv_label_set_text(offLbl, "OFF");
    lv_obj_set_style_text_font(offLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(offLbl, !editAutoStop ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(offLbl);

    // ── Separator (y=294) ──
    lv_obj_t* sep2 = lv_obj_create(screen);
    lv_obj_set_size(sep2, SCREEN_W, 2);
    lv_obj_set_pos(sep2, 0, 294);
    lv_obj_set_style_bg_color(sep2, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);
    lv_obj_remove_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);

    // ── Computed info line ──
    computedLabel = lv_label_create(screen);
    lv_obj_set_style_text_font(computedLabel, FONT_SMALL, 0);
    lv_obj_set_style_text_color(computedLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(computedLabel, 20, 304);
    update_computed_info();

    // ── CANCEL button (560,434,100x32) ──
    lv_obj_t* cancelBtn = lv_button_create(screen);
    lv_obj_set_size(cancelBtn, 100, 32);
    lv_obj_set_pos(cancelBtn, 560, 434);
    lv_obj_set_style_bg_color(cancelBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(cancelBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(cancelBtn, 1, 0);
    lv_obj_set_style_border_color(cancelBtn, COL_BORDER, 0);
    lv_obj_add_event_cb(cancelBtn, cancel_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancelLbl = lv_label_create(cancelBtn);
    lv_label_set_text(cancelLbl, "CANCEL");
    lv_obj_set_style_text_font(cancelLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(cancelLbl, COL_TEXT, 0);
    lv_obj_center(cancelLbl);

    // ── SAVE button (668,434,100x32) ──
    lv_obj_t* saveBtn = lv_button_create(screen);
    lv_obj_set_size(saveBtn, 100, 32);
    lv_obj_set_pos(saveBtn, 668, 434);
    lv_obj_set_style_bg_color(saveBtn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_radius(saveBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(saveBtn, 2, 0);
    lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
    lv_obj_add_event_cb(saveBtn, save_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* saveLbl = lv_label_create(saveBtn);
    lv_label_set_text(saveLbl, "SAVE");
    lv_obj_set_style_text_font(saveLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(saveLbl, COL_ACCENT, 0);
    lv_obj_center(saveLbl);

    LOG_I("Screen edit timer: layout created");
}

void screen_edit_timer_update() {
    if (!screens_is_active(SCREEN_EDIT_TIMER)) return;

    Preset* p = screen_program_edit_get_preset();
    if (!p || !durationLabel) return;

    // Update duration display
    update_duration_display();

    // Update scale bar position
    uint32_t durSec = p->timer_ms / 1000;
    if (scaleBar) {
        lv_bar_set_value(scaleBar, (int32_t)durSec, LV_ANIM_OFF);
    }
}
