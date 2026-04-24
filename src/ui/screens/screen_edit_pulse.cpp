// TIG Rotator Controller - Edit Pulse Settings Screen
// Brutalist v2.0 design — two-column layout: ON TIME / OFF TIME / RPM / CYCLES
// Computed info line, CANCEL / SAVE buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* onTimeLabel = nullptr;
static lv_obj_t* offTimeLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* cyclesLabel = nullptr;
static lv_obj_t* onBar = nullptr;
static lv_obj_t* offBar = nullptr;
static lv_obj_t* rpmBar = nullptr;
static lv_obj_t* infoDutyLabel = nullptr;
static lv_obj_t* infoCycleLabel = nullptr;
static lv_obj_t* infoFreqLabel = nullptr;
static lv_obj_t* infoTotalLabel = nullptr;

// Local edit state
static uint32_t editOnMs = 500;
static uint32_t editOffMs = 500;
static float editRpm = 1.2f;
static int editCycles = 0;  // 0 = infinite

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
static void update_computed_info() {
  float onSec = editOnMs / 1000.0f;
  float offSec = editOffMs / 1000.0f;
  float cycleSec = onSec + offSec;
  float duty = (cycleSec > 0.0f) ? (onSec / cycleSec * 100.0f) : 0.0f;
  float freq = (cycleSec > 0.0f) ? (1.0f / cycleSec) : 0.0f;

  if (infoDutyLabel)
    lv_label_set_text_fmt(infoDutyLabel, "DUTY %d%%", (int)(duty + 0.5f));
  if (infoCycleLabel)
    lv_label_set_text_fmt(infoCycleLabel, "CYCLE %.1fs", cycleSec);
  if (infoFreqLabel)
    lv_label_set_text_fmt(infoFreqLabel, "FREQ %.1fHz", freq);
  if (infoTotalLabel) {
    if (editCycles > 0) {
      float totalSec = cycleSec * editCycles;
      if (totalSec < 60.0f)
        lv_label_set_text_fmt(infoTotalLabel, "TOTAL %.0fs", totalSec);
      else
        lv_label_set_text_fmt(infoTotalLabel, "TOTAL %.1fm", totalSec / 60.0f);
    } else {
      lv_label_set_text(infoTotalLabel, "TOTAL INF");
    }
  }

  // Progress bars
  if (onBar) {
    int pct = (int)((editOnMs - 100) * 100 / 4900);
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    lv_bar_set_value(onBar, pct, LV_ANIM_OFF);
  }
  if (offBar) {
    int pct = (int)((editOffMs - 100) * 100 / 4900);
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    lv_bar_set_value(offBar, pct, LV_ANIM_OFF);
  }
  if (rpmBar) {
    float mx = speed_get_rpm_max();
    float span = mx - MIN_RPM;
    if (span < 1e-6f) span = 1e-6f;
    int pct = (int)((editRpm - MIN_RPM) * 100.0f / span + 0.5f);
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    lv_bar_set_value(rpmBar, pct, LV_ANIM_OFF);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void on_time_adj_cb(lv_event_t* e) {
  if (!onTimeLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  editOnMs += delta;
  if (editOnMs < 100) editOnMs = 100;
  if (editOnMs > 5000) editOnMs = 5000;
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  update_computed_info();
}

static void off_time_adj_cb(lv_event_t* e) {
  if (!offTimeLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  editOffMs += delta;
  if (editOffMs < 100) editOffMs = 100;
  if (editOffMs > 5000) editOffMs = 5000;
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  update_computed_info();
}

static void rpm_adj_cb(lv_event_t* e) {
  if (!rpmLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  if (delta > 0) editRpm += 0.1f;
  else if (editRpm > MIN_RPM) editRpm -= 0.1f;
  if (editRpm < MIN_RPM) editRpm = MIN_RPM;
  if (editRpm > speed_get_rpm_max()) editRpm = speed_get_rpm_max();
  lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
  update_computed_info();
}

static void cycles_adj_cb(lv_event_t* e) {
  if (!cyclesLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  editCycles += delta;
  if (editCycles < 0) editCycles = 0;
  if (editCycles > 999) editCycles = 999;
  if (editCycles == 0)
    lv_label_set_text(cyclesLabel, "INF");
  else
    lv_label_set_text_fmt(cyclesLabel, "%d", editCycles);
  update_computed_info();
}

static void cancel_event_cb(lv_event_t* e) {
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void save_event_cb(lv_event_t* e) {
  Preset* p = screen_program_edit_get_preset();
  if (p) {
    p->pulse_on_ms = editOnMs;
    p->pulse_off_ms = editOffMs;
    p->rpm = editRpm;
    p->pulse_cycles = (uint16_t)editCycles;
  }
  screen_program_edit_update_ui();
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// Helper: create small -/+ button
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_pm_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                int16_t w, int16_t h, const char* text,
                                lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
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
// Helper: create a separator line
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_separator(lv_obj_t* parent, int16_t y) {
  lv_obj_t* sep = lv_obj_create(parent);
  lv_obj_set_size(sep, SCREEN_W, 1);
  lv_obj_set_pos(sep, 0, y);
  lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
  return sep;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — two-column layout per new_ui.svg:
//   Left col (x=20): ON TIME, RPM
//   Right col (x=420): OFF TIME, CYCLES
//   Separator lines, computed info, CANCEL/SAVE
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_pulse_create() {
  lv_obj_t* screen = screenRoots[SCREEN_EDIT_PULSE];
  lv_obj_clean(screen);

  Preset* p = screen_program_edit_get_preset();
  editOnMs = p ? p->pulse_on_ms : 500;
  editOffMs = p ? p->pulse_off_ms : 300;
  editRpm = p ? p->rpm : 1.2f;
  editCycles = p ? p->pulse_cycles : 0;

  // ── Header bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "EDIT PULSE");
  lv_obj_set_style_text_font(title, FONT_MED, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 12, 8);

  // [ESC] button at right of header
  lv_obj_t* escBtn = lv_button_create(header);
  lv_obj_set_size(escBtn, 60, 24);
  lv_obj_set_pos(escBtn, SCREEN_W - 60 - PAD_X, 3);
  lv_obj_set_style_bg_color(escBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(escBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(escBtn, 1, 0);
  lv_obj_set_style_border_color(escBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(escBtn, 0, 0);
  lv_obj_set_style_pad_all(escBtn, 0, 0);
  lv_obj_add_event_cb(escBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* escLbl = lv_label_create(escBtn);
  lv_label_set_text(escLbl, "[ESC]");
  lv_obj_set_style_text_font(escLbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(escLbl, COL_TEXT_DIM, 0);
  lv_obj_center(escLbl);

  // ── Layout constants (must fit 800px wide: old right col + bar + 2×btn ended at x=816) ──
  const int edge = 16;
  const int gapMid = 16;
  const int colW = (SCREEN_W - 2 * edge - gapMid) / 2;  // 376
  const int colLeftX = edge;
  const int colRightX = edge + colW + gapMid;           // 408
  const int btnW = BTN_W_PM;   // 44
  const int btnH = BTN_H_PM;   // 30
  const int btnGap = 8;
  const int btnRowW = btnW + btnGap + btnW;
  const int barW = colW - 20 - btnRowW;               // 260 → right + ends at 784
  const int barH = 3;

  // ════════════════════════════════════════════════════════════════════════════════
  // ON TIME — left column (y=40)
  // ════════════════════════════════════════════════════════════════════════════════
  const int onY = 40;

  lv_obj_t* onTitle = lv_label_create(screen);
  lv_label_set_text(onTitle, "ON TIME");
  lv_obj_set_style_text_font(onTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(onTitle, COL_ACCENT, 0);
  lv_obj_set_pos(onTitle, colLeftX, onY);

  onTimeLabel = lv_label_create(screen);
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  lv_obj_set_style_text_font(onTimeLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(onTimeLabel, COL_ACCENT, 0);
  lv_obj_set_pos(onTimeLabel, colLeftX + 80, onY);

  // Progress bar
  onBar = lv_bar_create(screen);
  lv_obj_set_size(onBar, barW, barH);
  lv_obj_set_pos(onBar, colLeftX, onY + 38);
  lv_obj_set_style_bg_color(onBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(onBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(onBar, 0, 0);
  lv_obj_set_style_radius(onBar, 1, 0);
  lv_bar_set_range(onBar, 0, 100);
  lv_obj_set_style_bg_color(onBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(onBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, colLeftX + barW + 20, onY + 24, btnW, btnH,
                "-", on_time_adj_cb, (void*)(intptr_t)-100);
  create_pm_btn(screen, colLeftX + barW + 20 + btnW + btnGap, onY + 24, btnW, btnH,
                "+", on_time_adj_cb, (void*)(intptr_t)100);

  // ════════════════════════════════════════════════════════════════════════════════
  // OFF TIME — right column (y=40)
  // ════════════════════════════════════════════════════════════════════════════════
  lv_obj_t* offTitle = lv_label_create(screen);
  lv_label_set_text(offTitle, "OFF TIME");
  lv_obj_set_style_text_font(offTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(offTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(offTitle, colRightX, onY);

  offTimeLabel = lv_label_create(screen);
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  lv_obj_set_style_text_font(offTimeLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(offTimeLabel, COL_TEXT, 0);
  lv_obj_set_pos(offTimeLabel, colRightX + 100, onY);

  // Progress bar
  offBar = lv_bar_create(screen);
  lv_obj_set_size(offBar, barW, barH);
  lv_obj_set_pos(offBar, colRightX, onY + 38);
  lv_obj_set_style_bg_color(offBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(offBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(offBar, 0, 0);
  lv_obj_set_style_radius(offBar, 1, 0);
  lv_bar_set_range(offBar, 0, 100);
  lv_obj_set_style_bg_color(offBar, COL_TEXT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(offBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, colRightX + barW + 20, onY + 24, btnW, btnH,
                "-", off_time_adj_cb, (void*)(intptr_t)-100);
  create_pm_btn(screen, colRightX + barW + 20 + btnW + btnGap, onY + 24, btnW, btnH,
                "+", off_time_adj_cb, (void*)(intptr_t)100);

  // ════════════════════════════════════════════════════════════════════════════════
  // Separator at y=166
  // ════════════════════════════════════════════════════════════════════════════════
  create_separator(screen, 166);

  // ════════════════════════════════════════════════════════════════════════════════
  // RPM — left column (y=180)
  // ════════════════════════════════════════════════════════════════════════════════
  const int rpmY = 180;

  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "RPM");
  lv_obj_set_style_text_font(rpmTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmTitle, colLeftX, rpmY);

  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_TEXT, 0);
  lv_obj_set_pos(rpmLabel, colLeftX + 50, rpmY);

  // Progress bar
  rpmBar = lv_bar_create(screen);
  lv_obj_set_size(rpmBar, barW, barH);
  lv_obj_set_pos(rpmBar, colLeftX, rpmY + 38);
  lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_bg_opa(rpmBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(rpmBar, 0, 0);
  lv_obj_set_style_radius(rpmBar, 1, 0);
  lv_bar_set_range(rpmBar, 0, 100);
  lv_obj_set_style_bg_color(rpmBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_radius(rpmBar, 1, LV_PART_INDICATOR);

  // -/+ buttons
  create_pm_btn(screen, colLeftX + barW + 20, rpmY + 24, btnW, btnH,
                "-", rpm_adj_cb, (void*)(intptr_t)-1);
  create_pm_btn(screen, colLeftX + barW + 20 + btnW + btnGap, rpmY + 24, btnW, btnH,
                "+", rpm_adj_cb, (void*)(intptr_t)1);

  // Range hint
  lv_obj_t* rpmHint = lv_label_create(screen);
  char hintBuf[16];
  snprintf(hintBuf, sizeof(hintBuf), "%.3f-%.3f", (double)MIN_RPM, (double)speed_get_rpm_max());
  lv_label_set_text(rpmHint, hintBuf);
  lv_obj_set_style_text_font(rpmHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(rpmHint, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(rpmHint, colLeftX + barW + 20 + btnRowW + 4, rpmY + 28);

  // ════════════════════════════════════════════════════════════════════════════════
  // CYCLES — right column (y=180)
  // ════════════════════════════════════════════════════════════════════════════════
  lv_obj_t* cyclesTitle = lv_label_create(screen);
  lv_label_set_text(cyclesTitle, "CYCLES");
  lv_obj_set_style_text_font(cyclesTitle, FONT_SMALL, 0);
  lv_obj_set_style_text_color(cyclesTitle, COL_TEXT_DIM, 0);
  lv_obj_set_pos(cyclesTitle, colRightX, rpmY);

  cyclesLabel = lv_label_create(screen);
  lv_label_set_text(cyclesLabel, "INF");
  lv_obj_set_style_text_font(cyclesLabel, FONT_XXL, 0);
  lv_obj_set_style_text_color(cyclesLabel, COL_TEXT, 0);
  lv_obj_set_pos(cyclesLabel, colRightX + 70, rpmY);

  // -/+ buttons
  create_pm_btn(screen, colRightX + barW + 20, rpmY + 24, btnW, btnH,
                "-", cycles_adj_cb, (void*)(intptr_t)-1);
  create_pm_btn(screen, colRightX + barW + 20 + btnW + btnGap, rpmY + 24, btnW, btnH,
                "+", cycles_adj_cb, (void*)(intptr_t)1);

  // ════════════════════════════════════════════════════════════════════════════════
  // Separator at y=304
  // ════════════════════════════════════════════════════════════════════════════════
  create_separator(screen, 304);

  // ════════════════════════════════════════════════════════════════════════════════
  // Computed info line (y=320)
  // ════════════════════════════════════════════════════════════════════════════════
  const int infoY = 320;

  infoDutyLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoDutyLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoDutyLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoDutyLabel, 20, infoY);

  infoCycleLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoCycleLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoCycleLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoCycleLabel, 200, infoY);

  infoFreqLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoFreqLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoFreqLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoFreqLabel, 380, infoY);

  infoTotalLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(infoTotalLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(infoTotalLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(infoTotalLabel, 560, infoY);

  update_computed_info();

  ui_create_btn(screen, 120, 400, 260, BTN_H_ACTION, "CANCEL", FONT_SUBTITLE, UI_BTN_NORMAL,
                cancel_event_cb, nullptr);
  ui_create_btn(screen, 420, 400, 260, BTN_H_ACTION, "SAVE", FONT_SUBTITLE, UI_BTN_ACCENT,
                save_event_cb, nullptr);

  LOG_I("Screen edit pulse: v2.0 two-column layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE — refresh values from preset
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_pulse_invalidate_widgets() {
  onTimeLabel = nullptr;
  offTimeLabel = nullptr;
  rpmLabel = nullptr;
  cyclesLabel = nullptr;
  onBar = nullptr;
  offBar = nullptr;
  rpmBar = nullptr;
  infoDutyLabel = nullptr;
  infoCycleLabel = nullptr;
  infoFreqLabel = nullptr;
  infoTotalLabel = nullptr;
}

void screen_edit_pulse_update() {
  if (!screens_is_active(SCREEN_EDIT_PULSE)) return;

  Preset* p = screen_program_edit_get_preset();
  if (!p) return;

  // Sync local state from preset
  editOnMs = p->pulse_on_ms;
  editOffMs = p->pulse_off_ms;
  editRpm = p->rpm;

  // Update labels
  if (onTimeLabel)
    lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  if (offTimeLabel)
    lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  if (rpmLabel)
    lv_label_set_text_fmt(rpmLabel, "%.1f", editRpm);

  update_computed_info();
}
