// TIG Rotator Controller - ESTOP Overlay (T-23)
// Full-screen red overlay that appears on STATE_ESTOP

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../safety/safety.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* estopOverlay = nullptr;
static lv_obj_t* resetBtn = nullptr;
static lv_obj_t* iconLabel = nullptr;
static bool estopVisible = false;
static uint32_t blinkTimer = 0;
static bool blinkState = false;

// ───────────────────────────────────────────────────────────────────────────────
// BLINK TIMER CALLBACK (1 Hz)
// ───────────────────────────────────────────────────────────────────────────────
static void blink_timer_cb(lv_timer_t* timer) {
  if (!estopVisible) return;

  blinkState = !blinkState;
  lv_obj_set_style_text_opa(iconLabel, blinkState ? LV_OPA_COVER : LV_OPA_50, 0);
}

// ───────────────────────────────────────────────────────────────────────────────
// RESET BUTTON EVENT
// ───────────────────────────────────────────────────────────────────────────────
static void reset_event_cb(lv_event_t* e) {
  // Only allow reset if physical ESTOP is released
  if (digitalRead(PIN_ESTOP) == HIGH) {
    safety_reset_estop();
    control_transition_to(STATE_IDLE);
    estop_overlay_hide();
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY CREATE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_create() {
  // Create overlay on top layer (above all screens)
  estopOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(estopOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(estopOverlay, COL_RED, 0);
  lv_obj_set_style_bg_opa(estopOverlay, LV_OPA_80, 0);
  lv_obj_set_style_border_width(estopOverlay, 0, 0);
  lv_obj_set_style_pad_all(estopOverlay, 0, 0);

  // Initially hidden
  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);

  // Icon (blinking warning)
  iconLabel = lv_label_create(estopOverlay);
  lv_label_set_text(iconLabel, "⛔");
  lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(iconLabel, COL_TEXT, 0);
  lv_obj_align(iconLabel, LV_ALIGN_CENTER, 0, -70);

  // Title
  lv_obj_t* title = lv_label_create(estopOverlay);
  lv_label_set_text(title, "EMERGENCY STOP");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(title, COL_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

  // Subtitle
  lv_obj_t* sub = lv_label_create(estopOverlay);
  lv_label_set_text(sub, "ACTIVATED — MOTOR DISABLED");
  lv_obj_set_style_text_color(sub, COL_TEXT_DIM, 0);
  lv_obj_align(sub, LV_ALIGN_CENTER, 0, 10);

  // Reset button
  resetBtn = lv_btn_create(estopOverlay);
  lv_obj_set_size(resetBtn, 320, 60);
  lv_obj_align(resetBtn, LV_ALIGN_CENTER, 0, 80);
  lv_obj_set_style_bg_color(resetBtn, COL_BG_CARD, 0);
  lv_obj_set_style_radius(resetBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(resetBtn, reset_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* resetLabel = lv_label_create(resetBtn);
  lv_label_set_text(resetLabel, "RESET — CONFIRM SAFE TO CONTINUE");
  lv_obj_set_style_text_font(resetLabel, &lv_font_montserrat_16, 0);
  lv_obj_center(resetLabel);

  LOG_I("ESTOP overlay: created");
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY SHOW/HIDE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_show() {
  if (estopOverlay == nullptr) return;

  lv_obj_clear_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = true;

  // Start blink timer
  if (blinkTimer == 0) {
    blinkTimer = (uint32_t)lv_timer_create(blink_timer_cb, 500, nullptr);
  }

  LOG_E("ESTOP overlay: shown");
}

void estop_overlay_hide() {
  if (estopOverlay == nullptr) return;

  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = false;
  blinkState = false;

  LOG_I("ESTOP overlay: hidden");
}

bool estop_overlay_visible() {
  return estopVisible;
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_update() {
  if (!estopVisible) return;

  // Enable/disable reset button based on ESTOP state
  bool estopPressed = safety_is_estop_active();
  if (estopPressed) {
    lv_obj_add_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(resetBtn, COL_BG_INPUT, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(resetBtn, 0), COL_TEXT_DIM, 0);
  } else {
    lv_obj_clear_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(resetBtn, COL_BG_CARD, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(resetBtn, 0), COL_TEXT, 0);
  }
}
