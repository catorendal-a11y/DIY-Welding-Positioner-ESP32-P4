// TIG Rotator Controller - ESTOP Overlay (brutalist v2.0)
// Full-screen emergency stop overlay matching new_ui.svg
// Red border frame, blinking warning, info panel, reset button

#include "../screens.h"
#include "../theme.h"
#include "../lvgl_hal.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../safety/safety.h"
#include "../../motor/motor.h"
#include "../../onchip_temp.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* estopOverlay   = nullptr;
static lv_obj_t* resetBtn       = nullptr;
static lv_obj_t* warningLabel   = nullptr;
static lv_obj_t* estopTitleLabel = nullptr;
static lv_obj_t* estopSubtitleLabel = nullptr;
static lv_obj_t* estopBottomHint = nullptr;
static lv_obj_t* infoLabels[6]  = {nullptr};
static lv_obj_t* s_redTint        = nullptr;
static lv_obj_t* s_innerBorder    = nullptr;
static lv_obj_t* s_infoPanel      = nullptr;
static lv_obj_t* s_borderBars[4]  = {nullptr};
static bool estopVisible = false;
static lv_timer_t* blinkTimer = nullptr;
static bool blinkState = false;
static uint32_t lastOverlayUpdate = 0;

static constexpr int ESTOP_FRAME_PX = 10;

// ───────────────────────────────────────────────────────────────────────────────
// Accent: E-STOP red vs driver ALM orange (same overlay, stronger presence)
// ───────────────────────────────────────────────────────────────────────────────
static lv_color_t estop_overlay_accent_color(void) {
  return safety_is_driver_alarm_latched() ? COL_WARN : COL_RED;
}

static void estop_overlay_apply_accent(void) {
  if (!estopOverlay) return;
  const lv_color_t accent = estop_overlay_accent_color();
  const lv_opa_t washOpa = safety_is_driver_alarm_latched() ? 52 : 62;

  if (s_redTint) {
    lv_obj_set_style_bg_color(s_redTint, accent, 0);
    lv_obj_set_style_bg_opa(s_redTint, washOpa, 0);
  }
  if (s_innerBorder) {
    lv_obj_set_style_border_color(s_innerBorder, accent, 0);
    lv_obj_set_style_border_opa(s_innerBorder, 200, 0);
    lv_obj_set_style_border_width(s_innerBorder, 4, 0);
  }
  for (int i = 0; i < 4; i++) {
    if (s_borderBars[i]) {
      lv_obj_set_style_bg_color(s_borderBars[i], accent, 0);
      lv_obj_set_style_bg_opa(s_borderBars[i], LV_OPA_COVER, 0);
    }
  }
  if (s_infoPanel) {
    lv_obj_set_style_border_color(s_infoPanel, accent, 0);
    lv_obj_set_style_border_opa(s_infoPanel, 200, 0);
    lv_obj_set_style_border_width(s_infoPanel, 2, 0);
  }
  if (warningLabel) {
    lv_obj_set_style_text_color(warningLabel, accent, 0);
  }
  if (estopTitleLabel) {
    lv_obj_set_style_text_color(estopTitleLabel, accent, 0);
  }
  if (estopSubtitleLabel) {
    lv_obj_set_style_text_color(estopSubtitleLabel, accent, 0);
  }
  if (estopBottomHint) {
    lv_obj_set_style_text_color(estopBottomHint, accent, 0);
  }
  if (resetBtn) {
    lv_obj_set_style_border_color(resetBtn, accent, 0);
    lv_obj_t* resetLabel = lv_obj_get_child(resetBtn, 0);
    if (resetLabel) {
      lv_obj_set_style_text_color(resetLabel, accent, 0);
    }
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// BLINK TIMER CALLBACK (~3 Hz on warning strip)
// ───────────────────────────────────────────────────────────────────────────────
static void blink_timer_cb(lv_timer_t* timer) {
  if (!estopVisible) return;

  blinkState = !blinkState;
  if (warningLabel) {
    lv_obj_set_style_text_opa(warningLabel, blinkState ? LV_OPA_COVER : LV_OPA_40, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// RESET BUTTON EVENT
// ───────────────────────────────────────────────────────────────────────────────
static void reset_event_cb(lv_event_t* e) {
  if (safety_can_reset_from_overlay()) {
    g_uiResetPending.store(true, std::memory_order_release);
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
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
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

  // ── Full-screen color wash (stronger than v2.0; hue follows ALM vs E-STOP) ──
  s_redTint = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(s_redTint);
  lv_obj_set_size(s_redTint, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(s_redTint, 0, 0);
  lv_obj_set_style_bg_color(s_redTint, COL_RED, 0);
  lv_obj_set_style_bg_opa(s_redTint, 62, 0);
  lv_obj_set_style_radius(s_redTint, 0, 0);
  lv_obj_remove_flag(s_redTint, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_redTint, LV_OBJ_FLAG_CLICKABLE);

  // ── Border frame (thick bars, full opacity) ──
  s_borderBars[0] = create_border_bar(estopOverlay, 0, 0, SCREEN_W, ESTOP_FRAME_PX);
  s_borderBars[1] = create_border_bar(estopOverlay, 0, SCREEN_H - ESTOP_FRAME_PX, SCREEN_W, ESTOP_FRAME_PX);
  s_borderBars[2] = create_border_bar(estopOverlay, 0, 0, ESTOP_FRAME_PX, SCREEN_H);
  s_borderBars[3] = create_border_bar(estopOverlay, SCREEN_W - ESTOP_FRAME_PX, 0, ESTOP_FRAME_PX, SCREEN_H);

  // ── Inner border (heavier stroke) ──
  s_innerBorder = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(s_innerBorder);
  lv_obj_set_size(s_innerBorder, 784, 464);
  lv_obj_set_pos(s_innerBorder, 8, 8);
  lv_obj_set_style_border_color(s_innerBorder, COL_RED, 0);
  lv_obj_set_style_border_opa(s_innerBorder, 200, 0);
  lv_obj_set_style_border_width(s_innerBorder, 4, 0);
  lv_obj_set_style_radius(s_innerBorder, RADIUS_CARD, 0);
  lv_obj_set_style_bg_opa(s_innerBorder, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(s_innerBorder, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_innerBorder, LV_OBJ_FLAG_CLICKABLE);

  // ── Warning strip (large type, fast blink) ──
  warningLabel = lv_label_create(estopOverlay);
  lv_label_set_text(warningLabel, "!!!   !!!   !!!   !!!   !!!   !!!   !!!   !!!");
  lv_obj_set_style_text_font(warningLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(warningLabel, COL_RED, 0);
  lv_obj_set_style_text_opa(warningLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_text_letter_space(warningLabel, 10, 0);
  lv_obj_set_pos(warningLabel, 0, 118);
  lv_obj_set_width(warningLabel, SCREEN_W);
  lv_obj_set_style_text_align(warningLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_remove_flag(warningLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── "E-STOP" title at (400,190), font-size=36 (use FONT_HUGE=40 closest) ──
  estopTitleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(estopTitleLabel, "E-STOP");
  lv_obj_set_style_text_font(estopTitleLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(estopTitleLabel, COL_RED, 0);
  lv_obj_set_style_text_align(estopTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(estopTitleLabel, SCREEN_W);
  lv_obj_set_style_text_opa(estopTitleLabel, LV_OPA_COVER, 0);
  lv_obj_set_pos(estopTitleLabel, 0, 168);
  lv_obj_remove_flag(estopTitleLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── Subtitle (larger, full strength) ──
  estopSubtitleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(estopSubtitleLabel, "EMERGENCY STOP ACTIVATED");
  lv_obj_set_style_text_font(estopSubtitleLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(estopSubtitleLabel, COL_RED, 0);
  lv_obj_set_style_text_opa(estopSubtitleLabel, LV_OPA_COVER, 0);
  lv_obj_set_style_text_align(estopSubtitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(estopSubtitleLabel, SCREEN_W);
  lv_obj_set_pos(estopSubtitleLabel, 0, 218);
  lv_obj_remove_flag(estopSubtitleLabel, LV_OBJ_FLAG_SCROLLABLE);

  // ── Info panel (stronger frame) ──
  s_infoPanel = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(s_infoPanel);
  lv_obj_set_size(s_infoPanel, 600, 100);
  lv_obj_set_pos(s_infoPanel, 100, 252);
  lv_obj_set_style_bg_color(s_infoPanel, lv_color_hex(0x0A0505), 0);
  lv_obj_set_style_bg_opa(s_infoPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(s_infoPanel, COL_RED, 0);
  lv_obj_set_style_border_opa(s_infoPanel, 200, 0);
  lv_obj_set_style_border_width(s_infoPanel, 2, 0);
  lv_obj_set_style_radius(s_infoPanel, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(s_infoPanel, 8, 0);
  lv_obj_remove_flag(s_infoPanel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_infoPanel, LV_OBJ_FLAG_CLICKABLE);

  // Info grid: 2 rows x 3 cols — labels must match estop_overlay_update() values
  // Row 1: MODE (state), FREQ (step Hz), CURRENT (unused)
  // Row 2: PROGRAM (unused), UPTIME (since boot), TEMP (placeholder)
  static const char* const info_names[6] = {
    "MODE", "FREQ", "CURRENT", "PROGRAM", "UPTIME", "TEMP"
  };
  static const lv_coord_t col_x[3] = { 0, 194, 388 };

  for (int i = 0; i < 6; i++) {
    int row = i / 3;
    int col = i % 3;

    // Label name (dim)
    lv_obj_t* nameLbl = lv_label_create(s_infoPanel);
    lv_label_set_text(nameLbl, info_names[i]);
    lv_obj_set_style_text_font(nameLbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(nameLbl, COL_RED, 0);
    lv_obj_set_style_text_opa(nameLbl, LV_OPA_60, 0);
    lv_obj_set_pos(nameLbl, col_x[col] + 4, row * 44 + 2);
    lv_obj_remove_flag(nameLbl, LV_OBJ_FLAG_SCROLLABLE);

    // Value placeholder (dimmer)
    lv_obj_t* valLbl = lv_label_create(s_infoPanel);
    lv_label_set_text(valLbl, "---");
    lv_obj_set_style_text_font(valLbl, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(valLbl, COL_RED, 0);
    lv_obj_set_style_text_opa(valLbl, LV_OPA_50, 0);
    lv_obj_set_pos(valLbl, col_x[col] + 4, row * 44 + 16);
    lv_obj_remove_flag(valLbl, LV_OBJ_FLAG_SCROLLABLE);

    infoLabels[i] = valLbl;
  }

  // ── Reset button: rect(250,360,300,60), fill=#1A0A0A, stroke=#FF1744, 2px, rx=4 ──
  resetBtn = lv_button_create(estopOverlay);
  lv_obj_remove_style_all(resetBtn);
  lv_obj_set_size(resetBtn, 300, 60);
  lv_obj_set_pos(resetBtn, 250, 372);
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

  // ── Bottom hint (ESTOP vs driver ALM) at (400,448), opacity=40% ──
  estopBottomHint = lv_label_create(estopOverlay);
  lv_label_set_text(estopBottomHint, "Release physical E-STOP button first");
  lv_obj_set_style_text_font(estopBottomHint, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(estopBottomHint, COL_RED, 0);
  lv_obj_set_style_text_opa(estopBottomHint, LV_OPA_80, 0);
  lv_obj_set_style_text_align(estopBottomHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(estopBottomHint, SCREEN_W);
  lv_obj_set_pos(estopBottomHint, 0, 440);
  lv_obj_remove_flag(estopBottomHint, LV_OBJ_FLAG_SCROLLABLE);

  estop_overlay_apply_accent();

  LOG_I("ESTOP overlay: created (brutalist v2.0)");
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY SHOW/HIDE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_show() {
  dim_reset_activity();  // Full brightness if panel was dimmed
  if (estopOverlay == nullptr) return;

  lv_obj_remove_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = true;

  estop_overlay_apply_accent();

  // Start blink timer (~3.3 Hz on warning strip)
  if (blinkTimer == nullptr) {
    blinkTimer = lv_timer_create(blink_timer_cb, 300, nullptr);
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

void estop_overlay_destroy() {
  if (blinkTimer != nullptr) {
    lv_timer_delete(blinkTimer);
    blinkTimer = nullptr;
  }
  if (estopOverlay != nullptr) {
    lv_obj_delete(estopOverlay);
    estopOverlay = nullptr;
  }
  resetBtn = nullptr;
  warningLabel = nullptr;
  estopTitleLabel = nullptr;
  estopSubtitleLabel = nullptr;
  estopBottomHint = nullptr;
  s_redTint = nullptr;
  s_innerBorder = nullptr;
  s_infoPanel = nullptr;
  for (int i = 0; i < 4; i++) s_borderBars[i] = nullptr;
  estopVisible = false;
  blinkState = false;
  for (int i = 0; i < 6; i++) infoLabels[i] = nullptr;
}

// ───────────────────────────────────────────────────────────────────────────────
// OVERLAY UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_update() {
  if (!estopVisible) return;

  uint32_t now = millis();
  if (now - lastOverlayUpdate < 500) return;
  lastOverlayUpdate = now;

  // Update info labels
  if (infoLabels[0]) {
    lv_label_set_text(infoLabels[0], control_get_state_string());
  }
  if (infoLabels[1]) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%u Hz", motor_get_current_hz());
    lv_label_set_text(infoLabels[1], buf);
  }
  if (infoLabels[2]) {
    lv_label_set_text(infoLabels[2], "--");
  }
  if (infoLabels[3]) {
    lv_label_set_text(infoLabels[3], "--");
  }
  if (infoLabels[4]) {
    uint32_t elapsed = millis() / 1000;
    uint32_t m = elapsed / 60;
    uint32_t s = elapsed % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u:%02u", (unsigned)m, (unsigned)s);
    lv_label_set_text(infoLabels[4], buf);
  }
  if (infoLabels[5]) {
    float t = 0.0f;
    char tbuf[20];
    if (onchip_temp_get_celsius(&t)) {
      snprintf(tbuf, sizeof(tbuf), "%.1f C", t);
    } else {
      snprintf(tbuf, sizeof(tbuf), "-- C");
    }
    lv_label_set_text(infoLabels[5], tbuf);
  }

  if (estopTitleLabel && estopSubtitleLabel && estopBottomHint) {
    if (safety_is_driver_alarm_latched()) {
      lv_label_set_text(estopTitleLabel, "DRIVER FAULT");
      lv_label_set_text(estopSubtitleLabel, "STEPPER DRIVER ALARM (ALM)");
      lv_label_set_text(estopBottomHint, "Clear fault on driver; ALM high before RESET");
    } else {
      lv_label_set_text(estopTitleLabel, "E-STOP");
      lv_label_set_text(estopSubtitleLabel, "EMERGENCY STOP ACTIVATED");
      lv_label_set_text(estopBottomHint, "Release physical E-STOP button first");
    }
    estop_overlay_apply_accent();
  }

  // Enable/disable reset when ESTOP released and driver ALM clear
  bool blockReset = !safety_can_reset_from_overlay();
  lv_obj_t* resetLabel = lv_obj_get_child(resetBtn, 0);

  if (blockReset) {
    lv_obj_add_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(resetBtn, LV_OPA_40, 0);
    lv_obj_set_style_border_opa(resetBtn, LV_OPA_40, 0);
    if (resetLabel) {
      lv_obj_set_style_text_opa(resetLabel, LV_OPA_40, 0);
    }
  } else {
    lv_obj_remove_state(resetBtn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(resetBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(resetBtn, LV_OPA_COVER, 0);
    if (resetLabel) {
      lv_obj_set_style_text_opa(resetLabel, LV_OPA_COVER, 0);
    }
  }
}
