// TIG Rotator Controller - Step Mode Screen
// Matching ui_all_screens.svg: Large circle angle display + preset buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static float currentAngle = 90.0f;
static lv_obj_t* angleLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void execute_event_cb(lv_event_t* e) {
  control_start_step(currentAngle);
}

static void angle_preset_cb(lv_event_t* e) {
  float angle = (float)(intptr_t)lv_event_get_user_data(e);
  currentAngle = angle;
  lv_label_set_text_fmt(angleLabel, "%.0f°", currentAngle);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — SVG: Large teal circle + preset row + STEP button
// ───────────────────────────────────────────────────────────────────────────────
void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // Title
  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "STEP MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEAL, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // ── Large angle circle (SVG: cx=150, cy=93, r=58, stroke=#1DE9B6) ──
  lv_obj_t* circle = lv_obj_create(screen);
  lv_obj_set_size(circle, 220, 220);
  lv_obj_align(circle, LV_ALIGN_LEFT_MID, 80, -20);
  lv_obj_set_style_bg_color(circle, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(circle, 3, 0);
  lv_obj_set_style_border_color(circle, COL_TEAL, 0);

  angleLabel = lv_label_create(circle);
  lv_label_set_text(angleLabel, "90°");
  lv_obj_set_style_text_font(angleLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(angleLabel, COL_TEAL, 0);
  lv_obj_center(angleLabel);

  // ── STEP execute button (right of circle) ──
  lv_obj_t* execBtn = lv_btn_create(screen);
  lv_obj_set_size(execBtn, 200, 160);
  lv_obj_set_pos(execBtn, 520, 100);
  lv_obj_set_style_bg_color(execBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(execBtn, 8, 0);
  lv_obj_set_style_border_width(execBtn, 2, 0);
  lv_obj_set_style_border_color(execBtn, COL_TEAL, 0);
  lv_obj_add_event_cb(execBtn, execute_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* execLabel = lv_label_create(execBtn);
  lv_label_set_text(execLabel, "STEP");
  lv_obj_set_style_text_font(execLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(execLabel, COL_TEAL, 0);
  lv_obj_center(execLabel);

  // ── Preset angle buttons (bottom row) ──
  const float angles[] = {45.0f, 90.0f, 180.0f, 360.0f};
  const char* labels[] = {"45°", "90°", "180°", "360°"};

  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 130, 60);
    lv_obj_set_pos(btn, 50 + i * 145, 360);
    lv_obj_set_style_radius(btn, 6, 0);

    if (angles[i] == 90.0f) {
      lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, COL_TEAL, 0);
    } else {
      lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
      lv_obj_set_style_border_width(btn, 1, 0);
      lv_obj_set_style_border_color(btn, COL_BORDER_SPD, 0);
    }

    lv_obj_add_event_cb(btn, angle_preset_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(int)angles[i]);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, labels[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, (angles[i] == 90.0f) ? COL_TEAL : COL_TEXT_DIM, 0);
    lv_obj_center(lbl);
  }

  // CUSTOM button
  lv_obj_t* customBtn = lv_btn_create(screen);
  lv_obj_set_size(customBtn, 130, 60);
  lv_obj_set_pos(customBtn, 50 + 4 * 145, 360);
  lv_obj_set_style_bg_color(customBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(customBtn, 6, 0);
  lv_obj_set_style_border_width(customBtn, 1, 0);
  lv_obj_set_style_border_color(customBtn, COL_BORDER_SPD, 0);

  lv_obj_t* customLabel = lv_label_create(customBtn);
  lv_label_set_text(customLabel, "CUSTOM");
  lv_obj_set_style_text_color(customLabel, COL_TEXT_DIM, 0);
  lv_obj_center(customLabel);

  LOG_I("Screen step: SVG-matched layout created");
}

void screen_step_update() {
  // Update position display
}
