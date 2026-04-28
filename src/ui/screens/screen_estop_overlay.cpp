// TIG Rotator Controller - ESTOP Overlay (POST mockup ui_screens_post_style_proposal.svg #16)
// Wash + disc + EMERGENCY STOP + subtitle + fault bar; RESET below fault bar for recovery

#include "../screens.h"
#include "../theme.h"
#include "../lvgl_hal.h"
#include "../../config.h"
#include "../../control/control.h"
#include "../../safety/safety.h"

static lv_obj_t* estopOverlay = nullptr;
static lv_obj_t* s_wash = nullptr;
static lv_obj_t* s_disc = nullptr;
static lv_obj_t* bangLabel = nullptr;
static lv_obj_t* estopTitleLabel = nullptr;
static lv_obj_t* estopSubtitleLabel = nullptr;
static lv_obj_t* faultBar = nullptr;
static lv_obj_t* faultBarLabel = nullptr;
static lv_obj_t* resetBtn = nullptr;
static bool estopVisible = false;
static uint32_t lastOverlayUpdate = 0;

static bool estop_overlay_is_driver_fault(void) {
  return safety_get_fault_reason() == FAULT_DRIVER_ALARM || safety_is_driver_alarm_latched();
}

static lv_color_t estop_overlay_accent_color(void) {
  return estop_overlay_is_driver_fault() ? COL_WARN : COL_RED;
}

static void estop_overlay_apply_accent(void) {
  if (!estopOverlay) return;
  const lv_color_t accent = estop_overlay_accent_color();

  if (s_disc) {
    lv_obj_set_style_border_color(s_disc, accent, 0);
  }
  if (bangLabel) {
    lv_obj_set_style_text_color(bangLabel, accent, 0);
  }
  if (estopTitleLabel) {
    lv_obj_set_style_text_color(estopTitleLabel, accent, 0);
  }
  if (faultBar) {
    lv_obj_set_style_border_color(faultBar, accent, 0);
  }
  if (faultBarLabel) {
    lv_obj_set_style_text_color(faultBarLabel, accent, 0);
  }
  if (resetBtn) {
    lv_obj_set_style_border_color(resetBtn, accent, 0);
    lv_obj_t* resetLbl = lv_obj_get_child(resetBtn, 0);
    if (resetLbl) lv_obj_set_style_text_color(resetLbl, accent, 0);
  }
}

static void reset_event_cb(lv_event_t* e) {
  if (safety_can_reset_from_overlay()) {
    g_uiResetPending.store(true, std::memory_order_release);
  }
}

void estop_overlay_create() {
  estopOverlay = lv_obj_create(lv_layer_top());
  lv_obj_remove_style_all(estopOverlay);
  lv_obj_set_size(estopOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(estopOverlay, 0, 0);
  lv_obj_set_style_bg_color(estopOverlay, COL_BG, 0);
  lv_obj_set_style_bg_opa(estopOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(estopOverlay, 0, 0);
  lv_obj_remove_flag(estopOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);

  // Mockup: #1A0A0A @ 72% over #050505 base
  s_wash = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(s_wash);
  lv_obj_set_size(s_wash, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(s_wash, 0, 0);
  lv_obj_set_style_bg_color(s_wash, lv_color_hex(0x1A0A0A), 0);
  lv_obj_set_style_bg_opa(s_wash, 184, 0);
  lv_obj_remove_flag(s_wash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_wash, LV_OBJ_FLAG_CLICKABLE);

  // Larger warning disc, shifted up (stay within 800x480)
  s_disc = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(s_disc);
  lv_obj_set_size(s_disc, 200, 200);
  lv_obj_align(s_disc, LV_ALIGN_TOP_MID, 0, 34);
  lv_obj_set_style_bg_color(s_disc, lv_color_hex(0x210000), 0);
  lv_obj_set_style_bg_opa(s_disc, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(s_disc, COL_RED, 0);
  lv_obj_set_style_border_width(s_disc, 10, 0);
  lv_obj_set_style_radius(s_disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_remove_flag(s_disc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(s_disc, LV_OBJ_FLAG_CLICKABLE);

  bangLabel = lv_label_create(s_disc);
  lv_label_set_text(bangLabel, "!");
  lv_obj_set_style_text_font(bangLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(bangLabel, COL_RED, 0);
  lv_obj_set_style_text_align(bangLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(bangLabel, 200);
  lv_obj_align(bangLabel, LV_ALIGN_CENTER, 0, 2);
  lv_obj_set_style_transform_zoom(bangLabel, 384, 0);
  lv_obj_set_style_transform_pivot_x(bangLabel, 100, 0);
  lv_obj_set_style_transform_pivot_y(bangLabel, 22, 0);

  estopTitleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(estopTitleLabel, "EMERGENCY STOP");
  lv_obj_set_style_text_font(estopTitleLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(estopTitleLabel, COL_RED, 0);
  lv_obj_set_style_text_align(estopTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(estopTitleLabel, SCREEN_W);
  lv_label_set_long_mode(estopTitleLabel, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_align(estopTitleLabel, LV_ALIGN_TOP_MID, 0, 236);

  estopSubtitleLabel = lv_label_create(estopOverlay);
  lv_label_set_text(estopSubtitleLabel, "Motor disabled. Clear fault before restart.");
  lv_obj_set_style_text_font(estopSubtitleLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(estopSubtitleLabel, COL_TEXT, 0);
  lv_obj_set_style_text_align(estopSubtitleLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(estopSubtitleLabel, 748);
  lv_label_set_long_mode(estopSubtitleLabel, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_align(estopSubtitleLabel, LV_ALIGN_TOP_MID, 0, 282);

  // Wider/taller fault strip; ~36px gap before RESET (fault ends 396, reset 432, h=44 -> 476)
  faultBar = lv_obj_create(estopOverlay);
  lv_obj_remove_style_all(faultBar);
  lv_obj_set_size(faultBar, 520, 56);
  lv_obj_set_pos(faultBar, 140, 340);
  lv_obj_set_style_bg_color(faultBar, COL_BG_DANGER, 0);
  lv_obj_set_style_bg_opa(faultBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(faultBar, COL_RED, 0);
  lv_obj_set_style_border_width(faultBar, 2, 0);
  lv_obj_set_style_radius(faultBar, RADIUS_BTN, 0);
  lv_obj_remove_flag(faultBar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(faultBar, LV_OBJ_FLAG_CLICKABLE);

  faultBarLabel = lv_label_create(faultBar);
  lv_label_set_text(faultBarLabel, "FAULT REASON: E-STOP / DRIVER ALM");
  lv_obj_set_style_text_font(faultBarLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(faultBarLabel, COL_RED, 0);
  lv_obj_set_style_text_align(faultBarLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(faultBarLabel, 480);
  lv_label_set_long_mode(faultBarLabel, LV_LABEL_LONG_MODE_CLIP);
  lv_obj_center(faultBarLabel);

  resetBtn = lv_button_create(estopOverlay);
  lv_obj_remove_style_all(resetBtn);
  lv_obj_set_size(resetBtn, 320, 44);
  lv_obj_align(resetBtn, LV_ALIGN_TOP_MID, 0, 432);
  ui_btn_style_post(resetBtn, UI_BTN_DANGER);
  lv_obj_add_event_cb(resetBtn, reset_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_remove_flag(resetBtn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* resetLabel = lv_label_create(resetBtn);
  lv_label_set_text(resetLabel, "RESET");
  lv_obj_set_style_text_font(resetLabel, FONT_BTN, 0);
  lv_obj_set_style_text_color(resetLabel, ui_btn_label_color_post(UI_BTN_DANGER), 0);
  lv_obj_center(resetLabel);

  estop_overlay_apply_accent();

  LOG_I("ESTOP overlay: POST mockup layout");
}

void estop_overlay_show() {
  dim_reset_activity();
  if (estopOverlay == nullptr) return;

  lv_obj_remove_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = true;
  estop_overlay_apply_accent();
  lastOverlayUpdate = 0;

  LOG_E("ESTOP overlay: shown");
}

void estop_overlay_hide() {
  if (estopOverlay == nullptr) return;

  lv_obj_add_flag(estopOverlay, LV_OBJ_FLAG_HIDDEN);
  estopVisible = false;

  LOG_I("ESTOP overlay: hidden");
}

bool estop_overlay_visible() {
  return estopVisible;
}

void estop_overlay_destroy() {
  if (estopOverlay != nullptr) {
    lv_obj_delete(estopOverlay);
    estopOverlay = nullptr;
  }
  s_wash = nullptr;
  s_disc = nullptr;
  bangLabel = nullptr;
  estopTitleLabel = nullptr;
  estopSubtitleLabel = nullptr;
  faultBar = nullptr;
  faultBarLabel = nullptr;
  resetBtn = nullptr;
  estopVisible = false;
}

void estop_overlay_update() {
  if (!estopVisible) return;

  uint32_t now = millis();
  if (now - lastOverlayUpdate < 500) return;
  lastOverlayUpdate = now;

  FaultReason reason = safety_get_fault_reason();
  if (faultBarLabel) {
    char buf[72];
    snprintf(buf, sizeof(buf), "FAULT REASON: %s", safety_fault_reason_name(reason));
    lv_label_set_text(faultBarLabel, buf);
  }

  if (estopTitleLabel && estopSubtitleLabel) {
    if (reason == FAULT_DRIVER_ALARM || safety_is_driver_alarm_latched()) {
      lv_label_set_text(estopTitleLabel, "EMERGENCY STOP");
      lv_label_set_text(estopSubtitleLabel,
                        "Stepper driver alarm. Motor disabled. Clear fault before restart.");
    } else if (reason == FAULT_ESTOP_GLITCH) {
      lv_label_set_text(estopTitleLabel, "E-STOP FAULT");
      lv_label_set_text(estopSubtitleLabel,
                        "Unstable E-STOP signal. Check wiring before reset.");
    } else {
      lv_label_set_text(estopTitleLabel, "EMERGENCY STOP");
      lv_label_set_text(estopSubtitleLabel, "Motor disabled. Clear fault before restart.");
    }
    lv_obj_set_style_text_color(estopSubtitleLabel, COL_TEXT, 0);
    estop_overlay_apply_accent();
  }

  bool blockReset = !safety_can_reset_from_overlay();
  lv_obj_t* resetLabel = resetBtn ? lv_obj_get_child(resetBtn, 0) : nullptr;

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
