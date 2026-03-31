// TIG Rotator Controller - ESTOP Overlay (brutalist v2.0)
// Full-screen emergency stop overlay matching new_ui.svg
// Red border frame, blinking warning, info panel, reset button

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../safety/safety.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* estopOverlay   = nullptr;
static lv_obj_t* resetBtn       = nullptr;
static lv_obj_t* warningLabel   = nullptr;
static lv_obj_t* infoLabels[6]  = {nullptr};
static bool estopVisible = false;
static lv_timer_t* blinkTimer = nullptr;
static bool blinkState = false;

// ───────────────────────────────────────────────────────────────────────────────
// BLINK TIMER CALLBACK (1 Hz)
// ───────────────────────────────────────────────────────────────────────────────
static void blink_timer_cb(lv_timer_t* timer) {
  if (!estopVisible) return;

  blinkState = !blinkState;
  lv_obj_set_style_text_opa(warningLabel, blinkState ? LV_OPA_50 : LV_OPA_20, 0);
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
// HELPER: create a border bar (one side of the red frame)
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_border_bar(lv_obj_t* parent, lv_coord_t x, lv_coord_t y,
                                    lv_coord_t w, lv_coord_t h) {
  lv_obj_t* bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, w, h);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_style_bg_color(bar, COL_RED, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  return bar;
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY CREATE — brutalist v2.0 matching new_ui.svg
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_create() {
  // Create overlay on top layer (above all screens)
  estopOverlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(estopOverlay);
  lv_obj_set_size(estopOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(estopOverlay, 0, 0);
  lv_obj_set_style_bg_color(estopOverlay, lv_color_hex(0x060606), 0);
  lv_obj_set_style_bg_opa(estopOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(estopOverlay, 0, 0);
  lv_obj_remove_flag(estopOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_CLICKABLE);  // Block clicks to screens below

  // Initially hidden
  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);

  // ── Red tint overlay (full screen, 8% opacity) ──
  lv_obj_t* redTint = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(redTint);
  lv_obj_set_size(redTint, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(redTint, 0, 0);
  lv_obj_set_style_bg_color(redTint, COL_RED, 0);
  lv_obj_set_style_bg_opa(redTint, 20, 0);   // ~8% of 255
  lv_obj_set_style_radius(redTint, 0, 0);
  lv_obj_remove_flag(redTint, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(redTint, LV_OBJ_FLAG_CLICKABLE);

  // ── Red border frame: 4 bars (top/bottom/left/right, 5px, 85% opacity) ──
  create_border_bar(estopOverlay, 0, 0, SCREEN_W, 5);         // Top
  create_border_bar(estopOverlay, 0, SCREEN_H - 5, SCREEN_W, 5); // Bottom
  create_border_bar(estopOverlay, 0, 0, 5, SCREEN_H);         // Left
  create_border_bar(estopOverlay, SCREEN_W - 5, 0, 5, SCREEN_H); // Right

  // ── Inner border: rect(8,8,784,464), stroke=#FF1744, opacity=20%, rx=4 ──
  lv_obj_t* innerBorder = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(innerBorder);
  lv_obj_set_size(innerBorder, 784, 464);
  lv_obj_set_pos(innerBorder, 8, 8);
  lv_obj_set_style_border_color(innerBorder, COL_RED, 0);
  lv_obj_set_style_border_opa(innerBorder, LV_OPA_20, 0);
  lv_obj_set_style_border_width(innerBorder, 1, 0);
  lv_obj_set_style_radius(innerBorder, RADIUS_CARD, 0);
  lv_obj_set_style_bg_opa(innerBorder, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(innerBorder, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(innerBorder, LV_OBJ_FLAG_CLICKABLE);

  // ── Warning bars text: "xxx  xxx  xxx" at y=140 ──
  warningLabel = lv_label_create(estopOverlay);
  lv_label_set_text(warningLabel, "!!!   !!!   !!!   !!!   !!!   !!!   !!!   !!!");
  lv_obj_set_style_text_font(warningLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(warningLabel, COL_RED, 0);
  lv_obj_set_style_text_opa(warningLabel, LV_OPA_50, 0);
  lv_obj_set_style_text_letter_space(warningLabel, 6, 0);
  lv_obj_set_pos(warningLabel, 0, 140);
  lv_obj_set_width(warningLabel, SCREEN_W);
  lv_obj_set_style_text_align(warningLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_remove_flag(warningLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── "E-STOP" title at (400,190), font-size=36 (use FONT_HUGE=40 closest) ──
  lv_obj_t* titleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(titleLabel, "E-STOP");
  lv_obj_set_style_text_font(titleLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(titleLabel, COL_RED, 0);
  lv_obj_set_style_text_align(titleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(titleLabel, SCREEN_W);
  lv_obj_set_pos(titleLabel, 0, 175);
  lv_obj_remove_flag(titleLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── "EMERGENCY STOP ACTIVATED" at (400,218), opacity=60% ──
  lv_obj_t* subtitleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(subtitleLabel, "EMERGENCY STOP ACTIVATED");
  lv_obj_set_style_text_font(subtitleLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(subtitleLabel, COL_RED, 0);
  lv_obj_set_style_text_opa(subtitleLabel, LV_OPA_60, 0);
  lv_obj_set_style_text_align(subtitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(subtitleLabel, SCREEN_W);
  lv_obj_set_pos(subtitleLabel, 0, 222);
  lv_obj_remove_flag(subtitleLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── Info panel: rect(100,240,600,100), fill=#0A0505, stroke=#FF1744, opa=30%, rx=4 ──
  lv_obj_t* infoPanel = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(infoPanel);
  lv_obj_set_size(infoPanel, 600, 100);
  lv_obj_set_pos(infoPanel, 100, 240);
  lv_obj_set_style_bg_color(infoPanel, lv_color_hex(0x0A0505), 0);
  lv_obj_set_style_bg_opa(infoPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(infoPanel, COL_RED, 0);
  lv_obj_set_style_border_opa(infoPanel, LV_OPA_30, 0);
  lv_obj_set_style_border_width(infoPanel, 1, 0);
  lv_obj_set_style_radius(infoPanel, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(infoPanel, 8, 0);
  lv_obj_remove_flag(infoPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(infoPanel, LV_OBJ_FLAG_CLICKABLE);

  // Info grid: 2 rows x 3 cols — MODE, POSITION, CURRENT, PROGRAM, TEMP, ELAPSED
  // Row 1 (top half): MODE, POSITION, CURRENT
  // Row 2 (bottom half): PROGRAM, TEMP, ELAPSED
  // Grid: 3 columns x 2 rows, each col ~194px wide (600 - 16 padding) / 3
  static const char* const info_names[6] = {
    "MODE", "POSITION", "CURRENT", "PROGRAM", "TEMP", "ELAPSED"
  };
  static const lv_coord_t col_x[3] = { 0, 194, 388 };

  for (int i = 0; i < 6; i++) {
    int row = i / 3;
    int col = i % 3;

    // Label name (dim)
    lv_obj_t* nameLbl = lv_label_create(infoPanel);
    lv_label_set_text(nameLbl, info_names[i]);
    lv_obj_set_style_text_font(nameLbl, FONT_SMALL, 0);
    lv_obj_set_style_text_color(nameLbl, COL_RED, 0);
    lv_obj_set_style_text_opa(nameLbl, LV_OPA_40, 0);
    lv_obj_set_pos(nameLbl, col_x[col] + 4, row * 44 + 2);
    lv_obj_remove_flag(nameLbl, LV_OBJ_FLAG_SCROLLABLE);

    // Value placeholder (dimmer)
    lv_obj_t* valLbl = lv_label_create(infoPanel);
    lv_label_set_text(valLbl, "---");
    lv_obj_set_style_text_font(valLbl, FONT_MED, 0);
    lv_obj_set_style_text_color(valLbl, COL_RED, 0);
    lv_obj_set_style_text_opa(valLbl, LV_OPA_30, 0);
    lv_obj_set_pos(valLbl, col_x[col] + 4, row * 44 + 16);
    lv_obj_remove_flag(valLbl, LV_OBJ_FLAG_SCROLLABLE);

    infoLabels[i] = valLbl;
  }

  // ── Reset button: rect(250,360,300,60), fill=#1A0A0A, stroke=#FF1744, 2px, rx=4 ──
  resetBtn = lv_button_create(estopOverlay);
  lv_obj_remove_style_all(resetBtn);
  lv_obj_set_size(resetBtn, 300, 60);
  lv_obj_set_pos(resetBtn, 250, 360);
  lv_obj_set_style_bg_color(resetBtn, COL_BG_DANGER, 0);
  lv_obj_set_style_bg_opa(resetBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(resetBtn, COL_RED, 0);
  lv_obj_set_style_border_opa(resetBtn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(resetBtn, 2, 0);
  lv_obj_set_style_radius(resetBtn, RADIUS_CARD, 0);
  lv_obj_add_event_cb(resetBtn, reset_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_remove_flag(resetBtn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* resetLabel = lv_label_create(resetBtn);
  lv_label_set_text(resetLabel, "RESET");
  lv_obj_set_style_text_font(resetLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(resetLabel, COL_RED, 0);
  lv_obj_center(resetLabel);

  // ── Bottom text: "Release physical E-STOP button first" at (400,448), opacity=40% ──
  lv_obj_t* bottomLabel = lv_label_create(estopOverlay);
  lv_label_set_text(bottomLabel, "Release physical E-STOP button first");
  lv_obj_set_style_text_font(bottomLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(bottomLabel, COL_RED, 0);
  lv_obj_set_style_text_opa(bottomLabel, LV_OPA_40, 0);
  lv_obj_set_style_text_align(bottomLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bottomLabel, SCREEN_W);
  lv_obj_set_pos(bottomLabel, 0, 448);
  lv_obj_remove_flag(bottomLabel, LV_OBJ_FLAG_SCROLLABLE);

  LOG_I("ESTOP overlay: created (brutalist v2.0)");
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY SHOW/HIDE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_show() {
  if (estopOverlay == nullptr) return;

  lv_obj_remove_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = true;

  // Start blink timer
  if (blinkTimer == 0) {
    blinkTimer = lv_timer_create(blink_timer_cb, 500, nullptr);
  }

  LOG_E("ESTOP overlay: shown");
}

void estop_overlay_hide() {
  if (estopOverlay == nullptr) return;

  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = false;
  blinkState = false;

  // Clean up blink timer to prevent resource leak
  if (blinkTimer != nullptr) {
    lv_timer_delete(blinkTimer);
    blinkTimer = nullptr;
  }

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
  lv_obj_t* resetLabel = lv_obj_get_child(resetBtn, 0);

  if (estopPressed) {
    lv_obj_add_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(resetBtn, LV_OPA_40, 0);
    lv_obj_set_style_border_opa(resetBtn, LV_OPA_30, 0);
    if (resetLabel) {
      lv_obj_set_style_text_opa(resetLabel, LV_OPA_30, 0);
    }
  } else {
    lv_obj_clear_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(resetBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(resetBtn, LV_OPA_COVER, 0);
    if (resetLabel) {
      lv_obj_set_style_text_opa(resetLabel, LV_OPA_COVER, 0);
    }
  }
}
