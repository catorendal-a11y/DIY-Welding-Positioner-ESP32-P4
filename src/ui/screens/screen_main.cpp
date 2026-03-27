#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../control/control.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static lv_obj_t* mainScreen;

static lv_obj_t* stateLabel;
static lv_obj_t* voltageLabel;

static lv_obj_t* rpmMeter;
static lv_meter_scale_t* gaugeScale;
static lv_meter_indicator_t* bgArc;
static lv_meter_indicator_t* activeArc;
static lv_obj_t* rpmValueLabel;
static lv_obj_t* rpmUnitLabel;

static lv_obj_t* onBtn;
static lv_obj_t* stopBtn;
static lv_obj_t* jogBtn;
static lv_obj_t* cwBtn;
static lv_obj_t* ccwBtn;
static lv_obj_t* menuBtn;
static lv_obj_t* rpmDownBtn;
static lv_obj_t* rpmUpBtn;

static const char* state_strings[] = {
    "\xE2\x97\x8F IDLE", "\xE2\x97\x8F RUNNING", "\xE2\x97\x8F PULSE", "\xE2\x97\x8F STEP",
    "\xE2\x97\x8F JOG", "\xE2\x97\x8F TIMER", "\xE2\x97\x8F STOPPING", "\xE2\x97\x8F ESTOP"
};

static lv_color_t state_colors[] = {
    COL_TEXT_DIM, COL_GREEN, COL_PURPLE, COL_TEAL,
    COL_AMBER, COL_ACCENT, COL_AMBER, COL_RED
};

static void on_event_cb(lv_event_t* e) {
    if (control_get_state() == STATE_IDLE) control_start_continuous();
}

static void stop_event_cb(lv_event_t* e) {
    SystemState s = control_get_state();
    if (s != STATE_IDLE && s != STATE_ESTOP) control_stop();
}

static void jog_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        if (speed_get_direction() == DIR_CW) control_start_jog_cw();
        else control_start_jog_ccw();
    } else if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        control_stop();
    }
}

static void cw_event_cb(lv_event_t* e) {
    speed_set_direction(DIR_CW);
    screen_main_update();
}

static void ccw_event_cb(lv_event_t* e) {
    speed_set_direction(DIR_CCW);
    screen_main_update();
}

static void menu_event_cb(lv_event_t* e) {
    screens_show(SCREEN_MENU);
}

static void rpm_down_cb(lv_event_t* e) {
    float rpm = speed_get_target_rpm();
    if (rpm > MIN_RPM) {
        float newRpm = rpm - 0.1f;
        if (newRpm < MIN_RPM) newRpm = MIN_RPM;
        speed_slider_set(newRpm);
    }
    screen_main_update();
}

static void rpm_up_cb(lv_event_t* e) {
    float rpm = speed_get_target_rpm();
    if (rpm < MAX_RPM) {
        float newRpm = rpm + 0.1f;
        if (newRpm > MAX_RPM) newRpm = MAX_RPM;
        speed_slider_set(newRpm);
    }
    screen_main_update();
}

static lv_obj_t* make_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h,
                           const char* text, bool active) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, RADIUS_SM, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    if (active) {
        lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    } else {
        lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    }
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
    lv_obj_center(lbl);
    return btn;
}

static void set_btn_active(lv_obj_t* btn, bool active) {
    if (active) {
        lv_obj_set_style_bg_color(btn, COL_BTN_ACTIVE, 0);
        lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    } else {
        lv_obj_set_style_bg_color(btn, COL_BTN_FILL, 0);
        lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    }
}

static void make_separator(lv_obj_t* parent, lv_coord_t y) {
    lv_obj_t* sep = lv_obj_create(parent);
    lv_obj_set_size(sep, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep, 0, y);
    lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
}

void screen_main_create() {
    mainScreen = screenRoots[SCREEN_MAIN];
    lv_obj_set_style_bg_color(mainScreen, COL_BG, 0);

    lv_obj_t* header = lv_obj_create(mainScreen);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_HEADER_BG, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(mainScreen);
    lv_label_set_text(title, "TIG ROTATOR");
    lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(title, COL_TEXT_DIM, 0);
    lv_obj_set_pos(title, SX(16), SY(21));

    stateLabel = lv_label_create(mainScreen);
    lv_label_set_text(stateLabel, state_strings[0]);
    lv_obj_set_style_text_font(stateLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(stateLabel, COL_GREEN, 0);
    lv_obj_align(stateLabel, LV_ALIGN_TOP_MID, 0, SY(21));

    voltageLabel = lv_label_create(mainScreen);
    lv_label_set_text(voltageLabel, "24.5V");
    lv_obj_set_style_text_font(voltageLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(voltageLabel, COL_TEXT_DIM, 0);
    lv_obj_align(voltageLabel, LV_ALIGN_TOP_RIGHT, -SX(16), SY(21));

    make_separator(mainScreen, HEADER_H);

    int32_t gauge_pad = SW(10);
    int32_t meter_sz = GAUGE_R * 2 + gauge_pad * 2;
    rpmMeter = lv_meter_create(mainScreen);
    lv_obj_set_size(rpmMeter, meter_sz, meter_sz);
    lv_obj_set_pos(rpmMeter, GAUGE_CX - meter_sz / 2, GAUGE_CY - meter_sz / 2);
    lv_obj_set_style_bg_opa(rpmMeter, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(rpmMeter, 0, 0);
    lv_obj_set_style_pad_all(rpmMeter, gauge_pad, 0);
    lv_obj_remove_style(rpmMeter, nullptr, LV_PART_INDICATOR);

    gaugeScale = lv_meter_add_scale(rpmMeter);
    lv_meter_set_scale_range(rpmMeter, gaugeScale, 0, 60, 270, 135);

    bgArc = lv_meter_add_arc(rpmMeter, gaugeScale, SW(7), COL_GAUGE_BG, 0);
    lv_meter_set_indicator_start_value(rpmMeter, bgArc, 0);
    lv_meter_set_indicator_end_value(rpmMeter, bgArc, 60);

    activeArc = lv_meter_add_arc(rpmMeter, gaugeScale, SW(5), COL_ACCENT, 0);
    lv_meter_set_indicator_start_value(rpmMeter, activeArc, 0);
    lv_meter_set_indicator_end_value(rpmMeter, activeArc, 0);

    int32_t label_r = GAUGE_R + SW(7) / 2 + SY(10);
    struct ScaleLabel { const char* txt; float rpm; };
    ScaleLabel scale_labels[] = {
        {"0", 0.0f}, {"2.0", 2.0f}, {"3.0", 3.0f}, {"4.0", 4.0f}, {"6.0", 6.0f}
    };
    for (int i = 0; i < 5; i++) {
        float angle = 135.0f + (scale_labels[i].rpm / 6.0f) * 270.0f;
        float rad = angle * (float)M_PI / 180.0f;
        lv_coord_t lx = GAUGE_CX + (lv_coord_t)(label_r * sinf(rad));
        lv_coord_t ly = GAUGE_CY - (lv_coord_t)(label_r * cosf(rad));
        lv_obj_t* lbl = lv_label_create(mainScreen);
        lv_label_set_text(lbl, scale_labels[i].txt);
        lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
        lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
        lv_obj_set_pos(lbl, lx - 8, ly - 7);
    }

    rpmValueLabel = lv_label_create(mainScreen);
    lv_label_set_text(rpmValueLabel, "0.0");
    lv_obj_set_style_text_font(rpmValueLabel, FONT_HUGE, 0);
    lv_obj_set_style_text_color(rpmValueLabel, COL_TEXT, 0);
    lv_obj_align(rpmValueLabel, LV_ALIGN_TOP_MID, 0, GAUGE_CY - 35);

    rpmUnitLabel = lv_label_create(mainScreen);
    lv_label_set_text(rpmUnitLabel, "RPM");
    lv_obj_set_style_text_font(rpmUnitLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(rpmUnitLabel, COL_ACCENT, 0);
    lv_obj_set_style_text_letter_space(rpmUnitLabel, SX(2), 0);
    lv_obj_align(rpmUnitLabel, LV_ALIGN_TOP_MID, 0, GAUGE_CY + 18);

    onBtn = make_btn(mainScreen, LEFT_BTN_X, BTN_ROW_1_Y, BTN_W, BTN_H, "ON", true);
    lv_obj_add_event_cb(onBtn, on_event_cb, LV_EVENT_CLICKED, nullptr);

    stopBtn = make_btn(mainScreen, LEFT_BTN_X, BTN_ROW_2_Y, BTN_W, BTN_H, "STOP", false);
    lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);

    jogBtn = make_btn(mainScreen, LEFT_BTN_X, BTN_ROW_3_Y, BTN_W, BTN_H, "JOG", false);
    lv_obj_add_event_cb(jogBtn, jog_event_cb, LV_EVENT_ALL, nullptr);

    cwBtn = make_btn(mainScreen, RIGHT_BTN_X, BTN_ROW_1_Y, BTN_W, BTN_H, "CW", true);
    lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

    ccwBtn = make_btn(mainScreen, RIGHT_BTN_X, BTN_ROW_2_Y, BTN_W, BTN_H, "CCW", false);
    lv_obj_add_event_cb(ccwBtn, ccw_event_cb, LV_EVENT_CLICKED, nullptr);

    menuBtn = make_btn(mainScreen, RIGHT_BTN_X, BTN_ROW_3_Y, BTN_W, BTN_H, "MENU", false);
    lv_obj_add_event_cb(menuBtn, menu_event_cb, LV_EVENT_CLICKED, nullptr);

    make_separator(mainScreen, SY(196));

    rpmDownBtn = make_btn(mainScreen, SPD_BTN_L_X, SPD_BTN_Y, SX(50), SH(30), "- RPM", false);
    lv_obj_add_event_cb(rpmDownBtn, rpm_down_cb, LV_EVENT_CLICKED, nullptr);

    rpmUpBtn = make_btn(mainScreen, SPD_BTN_R_X, SPD_BTN_Y, SX(50), SH(30), "+ RPM", false);
    lv_obj_add_event_cb(rpmUpBtn, rpm_up_cb, LV_EVENT_CLICKED, nullptr);
}

void screen_main_update() {
    if (!screens_is_active(SCREEN_MAIN)) return;

    SystemState state = control_get_state();
    Direction dir = speed_get_direction();

    if (state < 0 || state >= 8) return;

    float rpm;
    if (state == STATE_IDLE || state == STATE_ESTOP) {
        rpm = speed_get_target_rpm();
    } else {
        rpm = speed_get_actual_rpm();
    }
    if (rpm < 0.0f) rpm = 0.0f;

    int32_t meter_val = (int32_t)(rpm * 10.0f);
    if (meter_val > 60) meter_val = 60;

    lv_label_set_text(stateLabel, state_strings[state]);
    lv_obj_set_style_text_color(stateLabel, state_colors[state], 0);

    lv_meter_set_indicator_end_value(rpmMeter, activeArc, meter_val);

    char buf[16];
    int whole = (int)rpm;
    int frac = (int)((rpm - (float)whole) * 10.0f);
    if (frac < 0) frac = 0;
    if (frac > 9) frac = 9;
    snprintf(buf, sizeof(buf), "%d.%d", whole, frac);
    lv_label_set_text(rpmValueLabel, buf);

    bool running = (state == STATE_RUNNING || state == STATE_PULSE ||
                    state == STATE_STEP || state == STATE_TIMER || state == STATE_JOG);

    set_btn_active(onBtn, running);
    set_btn_active(cwBtn, dir == DIR_CW);
    set_btn_active(ccwBtn, dir == DIR_CCW);
}
