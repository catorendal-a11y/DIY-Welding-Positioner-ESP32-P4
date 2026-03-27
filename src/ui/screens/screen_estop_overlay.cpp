#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../safety/safety.h"

static lv_obj_t* estopOverlay = nullptr;
static lv_obj_t* resetBtn = nullptr;
static lv_obj_t* iconLabel = nullptr;
static bool estopVisible = false;
static lv_timer_t* blinkTimer = nullptr;
static bool blinkState = false;
static bool lastEstopPressed = false;

static void triangle_draw_cb(lv_event_t* e) {
    lv_draw_ctx_t* draw_ctx = lv_event_get_draw_ctx(e);
    lv_obj_t* obj = lv_event_get_target(e);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    int16_t cx = area.x1 + lv_area_get_width(&area) / 2;
    int16_t cy = area.y1 + lv_area_get_height(&area) / 2;
    int16_t r = LV_MIN(lv_area_get_width(&area), lv_area_get_height(&area)) / 2 - 6;
    lv_point_t pts[3];
    pts[0].x = cx;
    pts[0].y = cy - r;
    pts[1].x = cx - (int16_t)(r * 0.866f);
    pts[1].y = cy + (int16_t)(r * 0.5f);
    pts[2].x = cx + (int16_t)(r * 0.866f);
    pts[2].y = cy + (int16_t)(r * 0.5f);
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_color = COL_RED;
    rect_dsc.border_width = 3;
    lv_draw_polygon(draw_ctx, &rect_dsc, pts, 3);
}

static void blink_timer_cb(lv_timer_t* timer) {
    if (!estopVisible || !iconLabel) return;
    blinkState = !blinkState;
    lv_obj_set_style_text_opa(iconLabel, blinkState ? LV_OPA_COVER : LV_OPA_20, 0);
}

static void reset_event_cb(lv_event_t* e) {
    if (digitalRead(PIN_ESTOP) == HIGH) {
        safety_reset_estop();
        control_transition_to(STATE_IDLE);
        estop_overlay_hide();
    }
}

void estop_overlay_create() {
    estopOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(estopOverlay, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(estopOverlay, COL_ESTOP_BG, 0);
    lv_obj_set_style_border_color(estopOverlay, COL_RED, 0);
    lv_obj_set_style_border_width(estopOverlay, 3, 0);
    lv_obj_set_style_pad_all(estopOverlay, 0, 0);
    lv_obj_set_style_radius(estopOverlay, 0, 0);
    lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* header = lv_obj_create(estopOverlay);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_ESTOP_HDR, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* headerLabel = lv_label_create(header);
    lv_label_set_text(headerLabel, LV_SYMBOL_WARNING " E-STOP TRIGGERED");
    lv_obj_set_style_text_font(headerLabel, FONT_BODY, 0);
    lv_obj_set_style_text_color(headerLabel, COL_RED, 0);
    lv_obj_set_style_text_align(headerLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(headerLabel, SCREEN_W);
    lv_obj_set_pos(headerLabel, 0, (HEADER_H - 22) / 2);

    lv_obj_t* sep1 = lv_obj_create(estopOverlay);
    lv_obj_set_size(sep1, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep1, 0, HEADER_H);
    lv_obj_set_style_bg_color(sep1, COL_ESTOP_SEP, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);
    lv_obj_set_style_radius(sep1, 0, 0);

    lv_obj_t* triContainer = lv_obj_create(estopOverlay);
    int16_t triW = SW(60);
    int16_t triH = SH(44);
    lv_obj_set_size(triContainer, triW, triH);
    lv_obj_set_pos(triContainer, SX(180) - triW / 2, SY(88) - triH / 2);
    lv_obj_set_style_bg_opa(triContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(triContainer, 0, 0);
    lv_obj_set_style_shadow_width(triContainer, 0, 0);
    lv_obj_set_style_pad_all(triContainer, 0, 0);
    lv_obj_set_style_radius(triContainer, 0, 0);
    lv_obj_clear_flag(triContainer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(triContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(triContainer, triangle_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);

    iconLabel = lv_label_create(triContainer);
    lv_label_set_text(iconLabel, "!");
    lv_obj_set_style_text_font(iconLabel, FONT_XXL, 0);
    lv_obj_set_style_text_color(iconLabel, COL_RED, 0);
    lv_obj_center(iconLabel);

    lv_obj_t* stoppedLabel = lv_label_create(estopOverlay);
    lv_label_set_text(stoppedLabel, "Motor stopped");
    lv_obj_set_style_text_font(stoppedLabel, FONT_SUB, 0);
    lv_obj_set_style_text_color(stoppedLabel, COL_TEXT, 0);
    lv_obj_set_style_text_align(stoppedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(stoppedLabel, SCREEN_W);
    lv_obj_set_pos(stoppedLabel, 0, SY(118));

    lv_obj_t* checkLabel = lv_label_create(estopOverlay);
    lv_label_set_text(checkLabel, "Check safety conditions before reset.");
    lv_obj_set_style_text_font(checkLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(checkLabel, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(checkLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(checkLabel, SCREEN_W);
    lv_obj_set_pos(checkLabel, 0, SY(142));

    lv_obj_t* sep2 = lv_obj_create(estopOverlay);
    lv_obj_set_size(sep2, SCREEN_W, SEP_H);
    lv_obj_set_pos(sep2, 0, SY(168));
    lv_obj_set_style_bg_color(sep2, COL_ESTOP_SEP, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);
    lv_obj_set_style_radius(sep2, 0, 0);

    resetBtn = lv_btn_create(estopOverlay);
    lv_obj_set_size(resetBtn, SW(240), SH(40));
    lv_obj_set_pos(resetBtn, SX(60), SY(180));
    lv_obj_set_style_bg_color(resetBtn, COL_ESTOP_BG, 0);
    lv_obj_set_style_border_color(resetBtn, COL_RED, 0);
    lv_obj_set_style_border_width(resetBtn, 2, 0);
    lv_obj_set_style_radius(resetBtn, RADIUS_MD, 0);
    lv_obj_set_style_shadow_width(resetBtn, 0, 0);
    lv_obj_add_event_cb(resetBtn, reset_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* resetLabel = lv_label_create(resetBtn);
    lv_label_set_text(resetLabel, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_font(resetLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(resetLabel, COL_TEXT, 0);
    lv_obj_center(resetLabel);

    blinkTimer = lv_timer_create(blink_timer_cb, 500, nullptr);
    lv_timer_pause(blinkTimer);
}

void estop_overlay_show() {
    if (!estopOverlay) return;
    lv_obj_clear_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
    estopVisible = true;
    blinkState = true;
    if (iconLabel) lv_obj_set_style_text_opa(iconLabel, LV_OPA_COVER, 0);
    if (blinkTimer) lv_timer_resume(blinkTimer);
}

void estop_overlay_hide() {
    if (!estopOverlay) return;
    lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
    estopVisible = false;
    blinkState = false;
    if (blinkTimer) lv_timer_pause(blinkTimer);
}

bool estop_overlay_visible() {
    return estopVisible;
}

void estop_overlay_update() {
    if (!estopVisible || !resetBtn) return;
    bool estopPressed = safety_is_estop_active();
    if (estopPressed != lastEstopPressed) {
        lastEstopPressed = estopPressed;
        lv_obj_t* resetLabel = lv_obj_get_child(resetBtn, 0);
        if (estopPressed) {
            lv_obj_add_state(resetBtn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(resetBtn, COL_ESTOP_BG, 0);
            lv_obj_set_style_border_color(resetBtn, COL_RED_DARK, 0);
            if (resetLabel) lv_obj_set_style_text_color(resetLabel, COL_TEXT_DIM, 0);
        } else {
            lv_obj_clear_state(resetBtn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(resetBtn, COL_ESTOP_BG, 0);
            lv_obj_set_style_border_color(resetBtn, COL_RED, 0);
            if (resetLabel) lv_obj_set_style_text_color(resetLabel, COL_TEXT, 0);
        }
    }
}
