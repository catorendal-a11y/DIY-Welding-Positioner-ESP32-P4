// TIG Rotator Controller - Timer Mode Screen
// Clean layout: timer ring top-center, controls below, big buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static uint32_t timerSeconds = 30;
static lv_obj_t* timerLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* arcRing = nullptr;
static lv_obj_t* progressBar = nullptr;
static lv_obj_t* presetBtns[4] = {nullptr};
static int activePreset = 1;  // 30s default
static lv_obj_t* customNumpad = nullptr;
static lv_obj_t* customTa = nullptr;
static volatile bool numpadClosePending = false;

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_timer_display() {
  if (!timerLabel) return;
  uint32_t min = timerSeconds / 60;
  uint32_t sec = timerSeconds % 60;
  lv_label_set_text_fmt(timerLabel, "%02d:%02d", (int)min, (int)sec);
}

static void update_preset_styles() {
  for (int i = 0; i < 4; i++) {
    if (!presetBtns[i]) continue;
    bool isActive = (i == activePreset);
    lv_obj_set_style_bg_color(presetBtns[i], isActive ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_width(presetBtns[i], isActive ? 2 : 1, 0);
    lv_obj_set_style_border_color(presetBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_t* lbl = lv_obj_get_child(presetBtns[i], 0);
    if (lbl) lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  if (customNumpad) { lv_obj_delete(customNumpad); customNumpad = nullptr; }
  if (customTa) { lv_obj_delete(customTa); customTa = nullptr; }
  screens_show(SCREEN_MAIN);
}

static void preset_event_cb(lv_event_t* e) {
  int index = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  const uint32_t presets[] = {10, 30, 60};
  if (index < 3) {
    timerSeconds = presets[index];
    activePreset = index;
    update_timer_display();
    update_preset_styles();
  }
}

static void custom_keyboard_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (customTa) {
      const char* txt = lv_textarea_get_text(customTa);
      int val = atoi(txt);
      if (val > 0) {
        if (val > 600) val = 600;
        timerSeconds = (uint32_t)val;
        activePreset = 3;
        update_timer_display();
        update_preset_styles();
      }
    }
  }
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    numpadClosePending = true;
  }
}

static void custom_btn_cb(lv_event_t* e) {
  if (!customNumpad) {
    customTa = lv_textarea_create(screenRoots[SCREEN_TIMER]);
    lv_obj_set_size(customTa, 200, 50);
    lv_obj_align(customTa, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_one_line(customTa, true);
    lv_textarea_set_accepted_chars(customTa, "0123456789");
    lv_obj_set_style_bg_color(customTa, COL_BG, 0);
    lv_obj_set_style_border_color(customTa, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customTa, 2, 0);
    lv_obj_set_style_text_color(customTa, COL_TEXT, 0);

    customNumpad = lv_keyboard_create(screenRoots[SCREEN_TIMER]);
    lv_keyboard_set_mode(customNumpad, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(customNumpad, customTa);
    lv_obj_set_style_bg_color(customNumpad, COL_BG, 0);
    lv_obj_set_style_border_color(customNumpad, COL_ACCENT, 0);
    lv_obj_set_style_border_width(customNumpad, 2, 0);
    lv_obj_add_event_cb(customNumpad, custom_keyboard_cb, LV_EVENT_ALL, nullptr);
  }
}

static void dur_adj_cb(lv_event_t* e) {
  int delta = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  int32_t dur = (int32_t)timerSeconds + delta;
  if (dur < 1) dur = 1;
  if (dur > 600) dur = 600;
  timerSeconds = (uint32_t)dur;

  const uint32_t presets[] = {10, 30, 60};
  activePreset = 3;
  for (int i = 0; i < 3; i++) {
    if (timerSeconds == presets[i]) { activePreset = i; break; }
  }
  update_timer_display();
  update_preset_styles();
}

static void rpm_adj_cb(lv_event_t* e) {
  int delta = (int)(intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  float rpm = speed_get_target_rpm();
  rpm += delta * 0.1f;
  if (rpm < MIN_RPM) rpm = MIN_RPM;
  if (rpm > MAX_RPM) rpm = MAX_RPM;
  speed_slider_set(rpm);
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", rpm);
}

static void start_event_cb(lv_event_t* e) {
  SystemState state = control_get_state();
  if (state == STATE_IDLE && timerSeconds > 0) {
    speed_slider_set(speed_get_target_rpm());
    control_start_timer(timerSeconds);
  }
}

static void stop_event_cb(lv_event_t* e) {
  if (control_get_state() == STATE_TIMER) {
    control_stop();
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// Helper: create +/- button (56x48, glove-friendly)
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_pm_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                const char* text, lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, 64, 52);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER_SM, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_XL, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// Layout: Header(30) | Timer ring(180, centered) | Duration row(50) |
//         Presets row(44) | RPM row(50) | Bottom bar(48) = 30+180+50+44+50+48=402+gaps
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_create() {
  lv_obj_t* screen = screenRoots[SCREEN_TIMER];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Header bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "TIMER MODE");
  lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 8);

  // ── Timer ring (180x180, centered horizontally, y=36) ──
  // Ring center: (400, 126)
  const int ringSize = 180;
  const int ringX = (SCREEN_W - ringSize) / 2;  // 310
  const int ringY = 36;

  arcRing = lv_arc_create(screen);
  lv_obj_set_size(arcRing, ringSize, ringSize);
  lv_obj_set_pos(arcRing, ringX, ringY);
  lv_arc_set_range(arcRing, 0, 100);
  lv_arc_set_value(arcRing, 100);
  lv_arc_set_bg_angles(arcRing, 0, 360);
  lv_arc_set_angles(arcRing, 0, 360);
  lv_obj_set_style_arc_color(arcRing, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcRing, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arcRing, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arcRing, 6, LV_PART_INDICATOR);
  lv_obj_remove_flag(arcRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(arcRing, 0, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arcRing, 0, LV_PART_KNOB);

  // Timer label centered INSIDE the ring
  timerLabel = lv_label_create(screen);
  update_timer_display();
  lv_obj_set_style_text_font(timerLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(timerLabel, COL_ACCENT, 0);
  lv_obj_set_style_text_align(timerLabel, LV_TEXT_ALIGN_CENTER, 0);
  // Center in ring: ring center = (ringX + ringSize/2, ringY + ringSize/2)
  // Use lv_obj_align to center on ring center
  lv_obj_set_width(timerLabel, ringSize);
  lv_obj_set_pos(timerLabel, ringX, ringY + (ringSize - 44) / 2 - 4);

  // "REMAINING" label below timer value inside ring
  lv_obj_t* remainTag = lv_label_create(screen);
  lv_label_set_text(remainTag, "REMAINING");
  lv_obj_set_style_text_font(remainTag, FONT_SMALL, 0);
  lv_obj_set_style_text_color(remainTag, COL_TEXT_DIM, 0);
  lv_obj_set_pos(remainTag, ringX + (ringSize - 54) / 2, ringY + ringSize / 2 + 16);

  // ── Progress bar (hidden — arc ring shows progress) ──
  progressBar = lv_bar_create(screen);
  lv_obj_set_size(progressBar, 400, 4);
  lv_obj_set_pos(progressBar, 200, 224);
  lv_bar_set_range(progressBar, 0, 100);
  lv_bar_set_value(progressBar, 100, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(progressBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_border_width(progressBar, 0, 0);
  lv_obj_set_style_pad_all(progressBar, 0, 0);
  lv_obj_set_style_radius(progressBar, 2, 0);
  lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_add_flag(progressBar, LV_OBJ_FLAG_HIDDEN);

  // ── Duration row (y=240, big buttons) ──
  const int durY = 270;
  lv_obj_t* durTitle = lv_label_create(screen);
  lv_label_set_text(durTitle, "DURATION");
  lv_obj_set_style_text_font(durTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(durTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(durTitle, 30, durY + 12);

  // Duration value label
  lv_obj_t* durValLabel = lv_label_create(screen);
  uint32_t dMin = timerSeconds / 60;
  uint32_t dSec = timerSeconds % 60;
  lv_label_set_text_fmt(durValLabel, "%02d:%02d", (int)dMin, (int)dSec);
  lv_obj_set_style_text_font(durValLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(durValLabel, COL_TEXT_BRIGHT, 0);
  lv_obj_set_pos(durValLabel, 130, durY + 10);

  // Duration +/- buttons (56x48, glove-friendly)
  create_pm_btn(screen, 220, durY, "-", dur_adj_cb, (void*)(intptr_t)(-5));
  create_pm_btn(screen, 284, durY, "+", dur_adj_cb, (void*)(intptr_t)(5));

  // Range hint
  lv_obj_t* rangeHint = lv_label_create(screen);
  lv_label_set_text(rangeHint, "1-600s");
  lv_obj_set_style_text_font(rangeHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rangeHint, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(rangeHint, 350, durY + 16);

  // ── RPM row (y=240, right side) ──
  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, 460, durY + 12);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", speed_get_target_rpm());
  lv_obj_set_style_text_font(rpmLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT_BRIGHT, 0);
  lv_obj_set_pos(rpmLabel, 510, durY + 10);

  create_pm_btn(screen, 580, durY, "-", rpm_adj_cb, (void*)(intptr_t)(-1));
  create_pm_btn(screen, 644, durY, "+", rpm_adj_cb, (void*)(intptr_t)(1));

  // ── Presets row (y=300) ──
  const int presetY = 340;
  const int presetW = 150;
  const int presetH = 44;
  const int presetGap = 10;
  const int presetStartX = 40;

  const uint32_t presets[] = {10, 30, 60};
  const char* presetLabels[] = {"10s", "30s", "60s"};

  for (int i = 0; i < 3; i++) {
    presetBtns[i] = lv_button_create(screen);
    lv_obj_set_size(presetBtns[i], presetW, presetH);
    lv_obj_set_pos(presetBtns[i], presetStartX + i * (presetW + presetGap), presetY);
    lv_obj_set_style_radius(presetBtns[i], RADIUS_BTN, 0);
    lv_obj_add_event_cb(presetBtns[i], preset_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    bool isActive = (i == activePreset);
    lv_obj_set_style_bg_color(presetBtns[i], isActive ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_width(presetBtns[i], isActive ? 2 : 1, 0);
    lv_obj_set_style_border_color(presetBtns[i], isActive ? COL_ACCENT : COL_BORDER, 0);

    lv_obj_t* lbl = lv_label_create(presetBtns[i]);
    lv_label_set_text(lbl, presetLabels[i]);
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl, isActive ? COL_ACCENT : COL_TEXT, 0);
    lv_obj_center(lbl);
  }

  // CUSTOM button
  presetBtns[3] = lv_button_create(screen);
  lv_obj_set_size(presetBtns[3], presetW, presetH);
  lv_obj_set_pos(presetBtns[3], presetStartX + 3 * (presetW + presetGap), presetY);
  lv_obj_set_style_bg_color(presetBtns[3], COL_BTN_BG, 0);
  lv_obj_set_style_radius(presetBtns[3], RADIUS_BTN, 0);
  lv_obj_set_style_border_width(presetBtns[3], 1, 0);
  lv_obj_set_style_border_color(presetBtns[3], COL_BORDER, 0);
  lv_obj_add_event_cb(presetBtns[3], custom_btn_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* customLbl = lv_label_create(presetBtns[3]);
  lv_label_set_text(customLbl, "CUSTOM");
  lv_obj_set_style_text_font(customLbl, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(customLbl, COL_TEXT, 0);
  lv_obj_center(customLbl);

  // ── Bottom bar: BACK + START + STOP (y=428, standard) ──
  const int barY = 428;
  const int barH = 48;
  const int barBtnW = 256;
  const int barGap = 8;
  const int barX = 8;

  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, barBtnW, barH);
  lv_obj_set_pos(backBtn, barX, barY);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  lv_obj_t* startBtn = lv_button_create(screen);
  lv_obj_set_size(startBtn, barBtnW, barH);
  lv_obj_set_pos(startBtn, barX + barBtnW + barGap, barY);
  lv_obj_set_style_bg_color(startBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(startBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(startBtn, 2, 0);
  lv_obj_set_style_border_color(startBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(startBtn, start_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* startLabel = lv_label_create(startBtn);
  lv_label_set_text(startLabel, "> START");
  lv_obj_set_style_text_font(startLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(startLabel, COL_ACCENT, 0);
  lv_obj_center(startLabel);

  lv_obj_t* stopBtn = lv_button_create(screen);
  lv_obj_set_size(stopBtn, barBtnW, barH);
  lv_obj_set_pos(stopBtn, barX + (barBtnW + barGap) * 2, barY);
  lv_obj_set_style_bg_color(stopBtn, COL_BG_DANGER, 0);
  lv_obj_set_style_radius(stopBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(stopBtn, 2, 0);
  lv_obj_set_style_border_color(stopBtn, COL_RED, 0);
  lv_obj_add_event_cb(stopBtn, stop_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* stopLabel = lv_label_create(stopBtn);
  lv_label_set_text(stopLabel, "[] STOP");
  lv_obj_set_style_text_font(stopLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(stopLabel, COL_RED, 0);
  lv_obj_center(stopLabel);

  LOG_I("Screen timer: clean layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_timer_update() {
  if (numpadClosePending) {
    if (customNumpad) { lv_obj_delete(customNumpad); customNumpad = nullptr; }
    if (customTa) { lv_obj_delete(customTa); customTa = nullptr; }
    numpadClosePending = false;
  }
  if (!screens_is_active(SCREEN_TIMER)) return;

  uint32_t remaining = control_get_timer_remaining();
  SystemState state = control_get_state();

  if (state == STATE_TIMER && remaining > 0) {
    uint32_t min = remaining / 60;
    uint32_t sec = remaining % 60;
    lv_label_set_text_fmt(timerLabel, "%02d:%02d", (int)min, (int)sec);

    // Progress bar
    uint32_t pct = (timerSeconds > 0) ? (remaining * 100 / timerSeconds) : 0;
    lv_bar_set_value(progressBar, (int32_t)pct, LV_ANIM_OFF);

    // Arc ring
    lv_arc_set_value(arcRing, (int32_t)pct);
    lv_arc_set_angles(arcRing, 0, pct * 360 / 100);
  } else {
    update_timer_display();
    lv_bar_set_value(progressBar, 100, LV_ANIM_OFF);
    lv_arc_set_value(arcRing, 100);
    lv_arc_set_angles(arcRing, 0, 360);
  }

  // Update RPM display
  if (rpmLabel) lv_label_set_text_fmt(rpmLabel, "%.1f", speed_get_target_rpm());
}
