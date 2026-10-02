// TIG Rotator Controller - Edit Pulse Settings Screen
// Brutalist v2.0 design — two-column layout: ON TIME / OFF TIME / RPM / CYCLES
// Computed info line, CANCEL / SAVE buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
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

  if (infoDutyLabel) lv_label_set_text_fmt(infoDutyLabel, "DUTY %d%%", (int)(duty + 0.5f));
  if (infoCycleLabel) lv_label_set_text_fmt(infoCycleLabel, "CYCLE %.1fs", cycleSec);
  if (infoFreqLabel) lv_label_set_text_fmt(infoFreqLabel, "FREQ %.1fHz", freq);
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
  const uint32_t pulseSpan = PULSE_MS_MAX - PULSE_MS_MIN;
  if (onBar) {
    int pct = (int)((editOnMs - PULSE_MS_MIN) * 100 / pulseSpan);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(onBar, pct, LV_ANIM_OFF);
  }
  if (offBar) {
    int pct = (int)((editOffMs - PULSE_MS_MIN) * 100 / pulseSpan);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(offBar, pct, LV_ANIM_OFF);
  }
  if (rpmBar) {
    float mx = speed_get_rpm_max();
    float span = mx - MIN_RPM;
    if (span < 1e-6f) span = 1e-6f;
    int pct = (int)((editRpm - MIN_RPM) * 100.0f / span + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(rpmBar, pct, LV_ANIM_OFF);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────


static void on_time_adj_cb(lv_event_t* e) {
  if (!onTimeLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    editOnMs += (uint32_t)delta;
  else if (editOnMs > PULSE_MS_MIN)
    editOnMs -= (uint32_t)(-delta);
  if (editOnMs < PULSE_MS_MIN) editOnMs = PULSE_MS_MIN;
  if (editOnMs > PULSE_MS_MAX) editOnMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  update_computed_info();
}

static void off_time_adj_cb(lv_event_t* e) {
  if (!offTimeLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    editOffMs += (uint32_t)delta;
  else if (editOffMs > PULSE_MS_MIN)
    editOffMs -= (uint32_t)(-delta);
  if (editOffMs < PULSE_MS_MIN) editOffMs = PULSE_MS_MIN;
  if (editOffMs > PULSE_MS_MAX) editOffMs = PULSE_MS_MAX;
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  update_computed_info();
}

static void rpm_adj_cb(lv_event_t* e) {
  if (!rpmLabel) return;
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  if (delta > 0)
    editRpm += ui_rpm_increment(editRpm);
  else if (editRpm > MIN_RPM)
    editRpm -= ui_rpm_increment(editRpm);
  if (editRpm < MIN_RPM) editRpm = MIN_RPM;
  if (editRpm > speed_get_rpm_max()) editRpm = speed_get_rpm_max();
  ui_set_rpm(rpmLabel, editRpm);
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
// Helper: create a separator line
// ───────────────────────────────────────────────────────────────────────────────


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
  ui_create_header(screen, "Pulse settings", "PROGRAM EDIT", nullptr);
  onTimeLabel = ui_create_adjust_card(screen, 24, 86, 368, "ROTATE FOR / s", on_time_adj_cb, 100);
  ui_highlight_value_card(onTimeLabel);
  offTimeLabel = ui_create_adjust_card(screen, 408, 86, 368, "PAUSE FOR / s", off_time_adj_cb, 100);
  rpmLabel = ui_create_adjust_card(screen, 24, 226, 368, "TARGET SPEED / RPM", rpm_adj_cb);
  cyclesLabel = ui_create_adjust_card(screen, 408, 226, 368, "CYCLES / 0 = continuous", cycles_adj_cb);
  lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  ui_set_rpm(rpmLabel, editRpm);
  if (editCycles)
    lv_label_set_text_fmt(cyclesLabel, "%d", editCycles);
  else
    lv_label_set_text(cyclesLabel, "INF");
  onBar = offBar = rpmBar = nullptr;
  infoDutyLabel = ui_create_text(screen, 24, 366, 176, "", FONT_NORMAL, COL_TEXT_DIM);
  infoCycleLabel = ui_create_text(screen, 216, 366, 176, "", FONT_NORMAL, COL_TEXT_DIM);
  infoFreqLabel = ui_create_text(screen, 408, 366, 176, "", FONT_NORMAL, COL_TEXT_DIM);
  infoTotalLabel = ui_create_text(screen, 600, 366, 176, "", FONT_NORMAL, COL_TEXT_DIM);
  ui_create_btn(screen, 24, 408, 152, 56, "CANCEL", FONT_BTN, UI_BTN_NORMAL, cancel_event_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "SAVE", FONT_BTN, UI_BTN_ACCENT, save_event_cb, nullptr);
  update_computed_info();
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

  // Draft is loaded on entry. Preserve edits until SAVE or CANCEL.

  // Update labels
  if (onTimeLabel) lv_label_set_text_fmt(onTimeLabel, "%.1fs", editOnMs / 1000.0f);
  if (offTimeLabel) lv_label_set_text_fmt(offTimeLabel, "%.1fs", editOffMs / 1000.0f);
  if (rpmLabel) ui_set_rpm(rpmLabel, editRpm);
  if (cyclesLabel) {
    if (editCycles == 0)
      lv_label_set_text(cyclesLabel, "INF");
    else
      lv_label_set_text_fmt(cyclesLabel, "%d", editCycles);
  }

  update_computed_info();
}
