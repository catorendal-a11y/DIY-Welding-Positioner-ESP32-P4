// TIG Rotator Controller - Step Mode Screen (T-24)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static float currentAngle = 10.0f;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void angle_event_cb(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_target(e);
  currentAngle = atof((const char*)lv_obj_get_user_data(btn));
  // Update button styles to show selection
}

static void execute_event_cb(lv_event_t* e) {
  control_start_step(currentAngle);
}

static void reset_event_cb(lv_event_t* e) {
  control_reset_step_accumulator();
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_step_create() {
  lv_obj_t* screen = screenRoots[SCREEN_STEP];

  // Header
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, STATUS_BAR_H);
  lv_obj_set_style_bg_color(backBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_set_style_text_color(backLabel, COL_ACCENT, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "◈ STEP MODE");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEAL, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // STEP SIZE label
  lv_obj_t* sizeLabel = lv_label_create(screen);
  lv_label_set_text(sizeLabel, "STEP SIZE");
  lv_obj_set_style_text_color(sizeLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(sizeLabel, 12, 50);

  // Angle buttons (6 buttons: 5°, 10°, 15°, 30°, 45°, 90°)
  const float angles[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f};
  const char* angleLabels[] = {"5°", "10°", "15°", "30°", "45°", "90°"};
  const int btnW = 72;
  const int btnH = 56;
  const int gap = 4;

  for (int i = 0; i < 6; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    lv_obj_set_size(btn, btnW, btnH);
    lv_obj_set_pos(btn, 12 + i * (btnW + gap), 74);

    // Store angle in user_data
    static char angleStr[6][16];
    snprintf(angleStr[i], 16, "%f", angles[i]);
    lv_obj_set_user_data(btn, angleStr[i]);

    lv_obj_add_event_cb(btn, angle_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, angleLabels[i]);
    lv_obj_center(label);

    // Highlight 10° by default
    if (angles[i] == 10.0f) {
      lv_obj_set_style_bg_color(btn, COL_BG_CARD, 0);
      lv_obj_set_style_border_width(btn, 3, 0);
      lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
      lv_obj_set_style_text_color(label, COL_ACCENT, 0);
    }
  }

  // Direction label
  lv_obj_t* dirLabel = lv_label_create(screen);
  lv_label_set_text(dirLabel, "DIRECTION");
  lv_obj_set_style_text_color(dirLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(dirLabel, 12, 140);

  // Direction buttons
  lv_obj_t* ccwBtn = lv_btn_create(screen);
  lv_obj_set_size(ccwBtn, 210, 52);
  lv_obj_set_pos(ccwBtn, 12, 166);
  lv_obj_set_style_bg_color(ccwBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_border_width(ccwBtn, 2, 0);
  lv_obj_set_style_border_color(ccwBtn, COL_BORDER, 0);

  lv_obj_t* ccwLabel = lv_label_create(ccwBtn);
  lv_label_set_text(ccwLabel, "◄ CCW");
  lv_obj_center(ccwLabel);

  lv_obj_t* cwBtn = lv_btn_create(screen);
  lv_obj_set_size(cwBtn, 210, 52);
  lv_obj_set_pos(cwBtn, 258, 166);
  lv_obj_set_style_bg_color(cwBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(cwBtn, 3, 0);
  lv_obj_set_style_border_color(cwBtn, COL_ACCENT, 0);

  lv_obj_t* cwLabel = lv_label_create(cwBtn);
  lv_label_set_text(cwLabel, "CW ►");
  lv_obj_set_style_text_color(cwLabel, COL_ACCENT, 0);
  lv_obj_center(cwLabel);

  // Position info
  lv_obj_t* posPanel = lv_obj_create(screen);
  lv_obj_set_size(posPanel, 456, 48);
  lv_obj_set_pos(posPanel, 12, 228);
  lv_obj_set_style_bg_color(posPanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(posPanel, RADIUS_CARD, 0);

  lv_obj_t* posLabel = lv_label_create(posPanel);
  lv_label_set_text(posLabel, "Position: 0.0°  Steps: 0");
  lv_obj_set_style_text_color(posLabel, COL_TEXT_DIM, 0);
  lv_obj_center(posLabel);

  // Action buttons
  lv_obj_t* execBtn = lv_btn_create(screen);
  lv_obj_set_size(execBtn, 220, 50);
  lv_obj_set_pos(execBtn, 12, 270);
  lv_obj_set_style_bg_color(execBtn, COL_TEAL, 0);
  lv_obj_set_style_radius(execBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(execBtn, execute_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* execLabel = lv_label_create(execBtn);
  lv_label_set_text(execLabel, "◈ EXECUTE STEP");
  lv_obj_center(execLabel);

  lv_obj_t* resetBtn = lv_btn_create(screen);
  lv_obj_set_size(resetBtn, 108, 50);
  lv_obj_set_pos(resetBtn, 244, 270);
  lv_obj_set_style_bg_color(resetBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(resetBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(resetBtn, reset_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* resetLabel = lv_label_create(resetBtn);
  lv_label_set_text(resetLabel, "RESET");
  lv_obj_center(resetLabel);

  lv_obj_t* backBtn2 = lv_btn_create(screen);
  lv_obj_set_size(backBtn2, 112, 50);
  lv_obj_set_pos(backBtn2, 356, 270);
  lv_obj_set_style_bg_color(backBtn2, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn2, RADIUS_BTN, 0);
  lv_obj_add_event_cb(backBtn2, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel2 = lv_label_create(backBtn2);
  lv_label_set_text(backLabel2, "BACK");
  lv_obj_center(backLabel2);

  LOG_I("Screen step: created");
}

void screen_step_update() {
  // Update position display
}
