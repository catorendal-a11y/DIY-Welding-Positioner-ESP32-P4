// TIG Rotator Controller - Program Edit Screen
// Brutalist v2.0 design matching new_ui.svg SCREEN_PROGRAM_EDIT

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include <atomic>
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE - Current preset being edited (-1 = new, 0-15 = existing)
// ───────────────────────────────────────────────────────────────────────────────
static int editSlot = -1;
static Preset editPreset;

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* nameInput = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmBarObj = nullptr;
static lv_obj_t* modeSettingsBtn = nullptr;
static lv_obj_t* keyboard = nullptr;

// Track mode toggle button objects for restyling
static lv_obj_t* modeBtns[4] = { nullptr };

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void keyboard_event_cb(lv_event_t* e);

// Keyboard close is deferred: event cb sets the flag, screen_program_edit_poll_keyboard()
// performs the actual async delete outside the event callback (safe pattern).
static std::atomic<bool> kbClosePending{false};

static void do_cleanup_kb() {
  if (keyboard) {
    lv_obj_remove_event_cb(keyboard, keyboard_event_cb);
    lv_keyboard_set_textarea(keyboard, nullptr);
    lv_obj_t* kb = keyboard;
    keyboard = nullptr;
    lv_obj_delete_async(kb);
  }
  kbClosePending.store(false, std::memory_order_release);
}

static void back_event_cb(lv_event_t* e) {
  do_cleanup_kb();
  screens_show(SCREEN_PROGRAMS);
}

static void keyboard_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
    kbClosePending.store(true, std::memory_order_release);
  }
}

static void textarea_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  if (code == LV_EVENT_FOCUSED) {
    if (!keyboard) {
      // Top layer: above program edit content so keys are visible and tappable.
      keyboard = lv_keyboard_create(lv_layer_top());
      lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_keyboard_set_textarea(keyboard, ta);
      lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_READY, nullptr);
      lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_CANCEL, nullptr);

      // Border only on MAIN; key tiles use theme LV_PART_ITEMS (needs theme_init display theme).
      lv_obj_set_style_border_width(keyboard, 2, 0);
      lv_obj_set_style_border_color(keyboard, COL_ACCENT, 0);
      lv_obj_set_style_pad_all(keyboard, 4, 0);
      lv_obj_set_size(keyboard, SCREEN_W, 220);
      lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_obj_move_foreground(keyboard);
    }
  }
}

// Mode row: each mode can be in the program (mask). Exactly one is RUN (motor uses one at a time).
static void restyle_mode_buttons() {
  const struct {
    const char* label;
    SystemState mode;
  } modes[] = {
    { "CONT",  STATE_RUNNING },
    { "PULSE", STATE_PULSE   },
    { "STEP",  STATE_STEP    }
  };

  for (int i = 0; i < 3; i++) {
    if (!modeBtns[i]) continue;
    uint8_t bit = preset_mode_to_mask(modes[i].mode);
    bool inProg = (editPreset.mode_mask & bit) != 0;
    bool isRun = (editPreset.mode == modes[i].mode);

    if (isRun && inProg) {
      ui_btn_style_post(modeBtns[i], UI_BTN_ACCENT);
    } else if (inProg) {
      lv_obj_set_style_bg_color(modeBtns[i], COL_BTN_BG, 0);
      lv_obj_set_style_border_width(modeBtns[i], 2, 0);
      lv_obj_set_style_border_color(modeBtns[i], COL_ACCENT, 0);
      lv_obj_set_style_radius(modeBtns[i], RADIUS_BTN, 0);
      lv_obj_set_style_shadow_width(modeBtns[i], 0, 0);
      lv_obj_set_style_pad_all(modeBtns[i], 0, 0);
      lv_obj_set_style_bg_opa(modeBtns[i], LV_OPA_COVER, 0);
    } else {
      ui_btn_style_post(modeBtns[i], UI_BTN_NORMAL);
    }

    lv_obj_t* lbl = lv_obj_get_child(modeBtns[i], 0);
    if (lbl) {
      if (!inProg) {
        lv_obj_set_style_text_color(lbl, COL_TEXT_VDIM, 0);
      } else if (isRun) {
        lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
      } else {
        lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
      }
    }
  }
}

// Update the mode settings info button text
static void update_mode_settings_text() {
  if (!modeSettingsBtn) return;

  const char* runName = "CONT";
  switch (editPreset.mode) {
    case STATE_RUNNING: runName = "CONT"; break;
    case STATE_PULSE:   runName = "PULSE"; break;
    case STATE_STEP:    runName = "STEP"; break;
    default:            runName = "CONT"; break;
  }

  char detail[80] = "";
  switch (editPreset.mode) {
    case STATE_RUNNING:
      snprintf(detail, sizeof(detail), "direction & soft start");
      break;
    case STATE_PULSE:
      snprintf(detail, sizeof(detail), "ON %.1fs / OFF %.1fs",
               editPreset.pulse_on_ms / 1000.0f, editPreset.pulse_off_ms / 1000.0f);
      break;
    case STATE_STEP:
      if (editPreset.workpiece_diameter_mm >= 1.0f) {
        snprintf(detail, sizeof(detail), "%.0f deg / step | OD %.0f mm",
                 editPreset.step_angle, editPreset.workpiece_diameter_mm);
      } else {
        snprintf(detail, sizeof(detail), "%.0f deg / step | default OD", editPreset.step_angle);
      }
      break;
    default:
      snprintf(detail, sizeof(detail), "CONT - direction & soft start");
      break;
  }

  char tags[8];
  int tp = 0;
  if (editPreset.mode_mask & PRESET_MASK_CONT) tags[tp++] = 'C';
  if (editPreset.mode_mask & PRESET_MASK_PULSE) tags[tp++] = 'P';
  if (editPreset.mode_mask & PRESET_MASK_STEP) tags[tp++] = 'S';
  tags[tp] = '\0';

  char buf[128];
  if (preset_mask_popcount(editPreset.mode_mask) > 1) {
    snprintf(buf, sizeof(buf), "[%s] RUN:%s - %s", tags, runName, detail);
  } else {
    switch (editPreset.mode) {
      case STATE_RUNNING:
        snprintf(buf, sizeof(buf), "CONT - %s", detail);
        break;
      case STATE_PULSE:
        snprintf(buf, sizeof(buf), "PULSE - %s", detail);
        break;
      case STATE_STEP:
        snprintf(buf, sizeof(buf), "STEP - %s", detail);
        break;
      default:
        snprintf(buf, sizeof(buf), "%s", detail);
        break;
    }
  }

  // Main text label is child 0
  lv_obj_t* btnLabel = lv_obj_get_child(modeSettingsBtn, 0);
  if (btnLabel) {
    lv_label_set_long_mode(btnLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(btnLabel, 736);
    lv_label_set_text(btnLabel, buf);
  }
}

static void mode_toggle_cb(lv_event_t* e) {
  SystemState tapped = (SystemState)(size_t)lv_event_get_user_data(e);
  uint8_t bit = preset_mode_to_mask(tapped);

  if (editPreset.mode_mask & bit) {
    if (editPreset.mode == tapped) {
      if (preset_mask_popcount(editPreset.mode_mask) > 1) {
        editPreset.mode_mask = (uint8_t)(editPreset.mode_mask & ~bit);
        editPreset.mode = preset_first_in_mask(editPreset.mode_mask);
      }
    } else {
      editPreset.mode = tapped;
    }
  } else {
    editPreset.mode_mask = (uint8_t)(editPreset.mode_mask | bit);
    editPreset.mode = tapped;
  }

  preset_clamp_mode_to_mask(&editPreset);
  restyle_mode_buttons();
  update_mode_settings_text();
}

static void rpm_minus_cb(lv_event_t* e) {
  intptr_t delta = (intptr_t)lv_event_get_user_data(e);
  float adj = (delta < 0) ? -0.1f : 0.1f;
  editPreset.rpm += adj;

  float mx = speed_get_rpm_max();
  if (editPreset.rpm < MIN_RPM) editPreset.rpm = MIN_RPM;
  if (editPreset.rpm > mx) editPreset.rpm = mx;

  if (rpmLabel) {
    lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
  }
  if (rpmBarObj) {
    lv_bar_set_value(rpmBarObj, (int32_t)(editPreset.rpm * 1000.0f + 0.5f), LV_ANIM_OFF);
  }
}

static void mode_settings_cb(lv_event_t* e) {
  const char* nameText = nameInput ? lv_textarea_get_text(nameInput) : nullptr;
  if (nameText && nameText[0]) {
    strlcpy(editPreset.name, nameText, sizeof(editPreset.name));
  }
  // Opening a sub-editor implies this mode block is part of the program.
  editPreset.mode_mask = (uint8_t)(editPreset.mode_mask | preset_mode_to_mask(editPreset.mode));
  switch (editPreset.mode) {
    case STATE_RUNNING:
      screens_show(SCREEN_EDIT_CONT);
      break;
    case STATE_PULSE:
      screens_show(SCREEN_EDIT_PULSE);
      break;
    case STATE_STEP:
      screens_show(SCREEN_EDIT_STEP);
      break;
    default:
      break;
  }
}

static void cancel_cb(lv_event_t* e) {
  (void)e;
  do_cleanup_kb();
  screens_show(SCREEN_PROGRAMS);
}

static void delete_preset_from_edit_do() {
  int slot = editSlot;
  if (slot < 0) return;
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (slot >= 0 && slot < (int)g_presets.size()) {
    g_presets.erase(g_presets.begin() + slot);
    for (size_t i = 0; i < g_presets.size(); i++) {
      g_presets[i].id = i + 1;
    }
    xSemaphoreGive(g_presets_mutex);
    storage_save_presets();
  } else {
    xSemaphoreGive(g_presets_mutex);
  }
  screen_programs_mark_dirty();
}

static void delete_preset_prompt_cb(lv_event_t* e) {
  (void)e;
  char buf[64];
  snprintf(buf, sizeof(buf), "Delete \"%s\"?", editPreset.name);
  screen_confirm_create(buf, "This action cannot be undone.", delete_preset_from_edit_do, nullptr,
                        SCREEN_PROGRAMS);
}

static void save_preset_cb(lv_event_t* e) {
  const char* rawName = nameInput ? lv_textarea_get_text(nameInput) : "";
  char nameBuf[32];
  if (rawName && rawName[0]) {
    strlcpy(nameBuf, rawName, sizeof(nameBuf));
  } else {
    strlcpy(nameBuf, "Untitled", sizeof(nameBuf));
  }

  strlcpy(editPreset.name, nameBuf, sizeof(editPreset.name));
  preset_clamp_mode_to_mask(&editPreset);

  do_cleanup_kb();

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
    editPreset.id = editSlot + 1;
    g_presets[editSlot] = editPreset;
  } else {
    if (g_presets.size() >= MAX_PRESETS) {
      xSemaphoreGive(g_presets_mutex);
      LOG_W("Preset list full (%d/%d)", (int)g_presets.size(), MAX_PRESETS);
      screen_confirm_create("FULL", "Maximum 16 programs reached.", nullptr, nullptr, SCREEN_NONE);
      return;
    }
    editPreset.id = g_presets.size() + 1;
    g_presets.push_back(editPreset);
  }
  xSemaphoreGive(g_presets_mutex);

  storage_save_presets();
  LOG_I("SAVE: switching to programs screen");
  screens_show(SCREEN_PROGRAMS);
  LOG_I("SAVE: screen switch done");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE -- new_ui.svg SCREEN_PROGRAM_EDIT
// Header: "NEW PROGRAM" or "EDIT Pxx" + BACK button at (718,6,68x18)
// NAME input (y=64): rect(20,64,760,36), fill=#0A0A0A, stroke=#333
// MODE selector (y=136): 4 toggles CONT|PULSE|STEP|TIMER, 170x38
// RPM (y=204): large value, progress bar, -/+ buttons
// Mode settings button (y=268)
// CANCEL (120,400,260x52) / SAVE (420,400,260x52)
// ───────────────────────────────────────────────────────────────────────────────
void screen_program_edit_create(int slot) {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAM_EDIT];
  rpmBarObj = nullptr;
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  for (int j = 0; j < 4; j++) {
    modeBtns[j] = nullptr;
  }
  nameInput = nullptr;
  rpmLabel = nullptr;
  modeSettingsBtn = nullptr;

  editSlot = slot;
  keyboard = nullptr;

  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (slot >= 0 && slot < (int)g_presets.size()) {
    editPreset = g_presets[slot];
    preset_clamp_mode_to_mask(&editPreset);
  } else {
    // New preset — start from current machine state (direction, mode, RPM) so it feels consistent.
    editPreset.id = 0;
    snprintf(editPreset.name, sizeof(editPreset.name), "New Program");
    {
      SystemState cs = control_get_state();
      if (cs == STATE_PULSE) {
        editPreset.mode = STATE_PULSE;
      } else if (cs == STATE_STEP) {
        editPreset.mode = STATE_STEP;
      } else {
        editPreset.mode = STATE_RUNNING;
      }
    }
    editPreset.rpm = speed_get_target_rpm();
    editPreset.pulse_on_ms = 500;
    editPreset.pulse_off_ms = 500;
    editPreset.step_angle = 90.0f;
    editPreset.workpiece_diameter_mm = speed_get_workpiece_diameter_mm();
    editPreset.timer_ms = 30000;
    editPreset.direction = (uint8_t)speed_get_direction();
    editPreset.pulse_cycles = 0;     // infinite
    editPreset.step_repeats = 1;
    editPreset.step_dwell_sec = 0.0f;
    editPreset.timer_auto_stop = 1;  // auto stop
    editPreset.cont_soft_start = 0;
    editPreset.mode_mask = PRESET_MASK_ALL;
    preset_clamp_mode_to_mask(&editPreset);
  }
  xSemaphoreGive(g_presets_mutex);

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title (SVG: x=12, "NEW PROGRAM" or "EDIT Pxx")
  lv_obj_t* title = lv_label_create(header);
  if (slot >= 0) {
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "EDIT P%02d", slot + 1);
    lv_label_set_text(title, tbuf);
  } else {
    lv_label_set_text(title, "NEW PROGRAM");
  }
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 12, 11);

  lv_obj_t* subHdr = lv_label_create(header);
  lv_label_set_text(subHdr, "PRESET EDIT");
  lv_obj_set_style_text_font(subHdr, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(subHdr, COL_TEXT_DIM, 0);
  lv_obj_set_width(subHdr, 200);
  lv_obj_set_style_text_align(subHdr, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(subHdr, SCREEN_W - 12 - 200, 12);

  // ── Separator line under header ──
  lv_obj_t* line1 = lv_obj_create(screen);
  lv_obj_set_size(line1, SCREEN_W, 1);
  lv_obj_set_pos(line1, 0, HEADER_H);
  lv_obj_set_style_bg_color(line1, COL_BORDER, 0);
  lv_obj_set_style_pad_all(line1, 0, 0);
  lv_obj_set_style_border_width(line1, 0, 0);
  lv_obj_set_style_radius(line1, 0, 0);
  lv_obj_remove_flag(line1, LV_OBJ_FLAG_SCROLLABLE);

  ui_add_post_header_accent(screen);

  // ── NAME label (SVG: y=48, "NAME" in #4A4A4A) ──
  ui_create_post_card(screen, 16, 48, 768, 66);
  ui_create_post_card(screen, 16, 118, 768, 74);
  ui_create_post_card(screen, 16, 198, 768, 82);

  lv_obj_t* nameLabel = lv_label_create(screen);
  lv_label_set_text(nameLabel, "NAME");
  lv_obj_set_style_text_font(nameLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(nameLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(nameLabel, 20, 50);

  // ── NAME input (SVG: 20,64,760,36, fill=#0A0A0A, stroke=#333) ──
  nameInput = lv_textarea_create(screen);
  lv_obj_set_size(nameInput, 760, 36);
  lv_obj_set_pos(nameInput, 20, 70);
  lv_textarea_set_placeholder_text(nameInput, "Enter name...");
  lv_textarea_set_max_length(nameInput, 31);
  lv_textarea_set_one_line(nameInput, true);
  lv_textarea_set_text(nameInput, editPreset.name);
  lv_obj_set_style_bg_color(nameInput, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(nameInput, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(nameInput, 1, 0);
  lv_obj_set_style_border_color(nameInput, COL_BORDER_SM, 0);
  lv_obj_set_style_text_color(nameInput, COL_TEXT, 0);
  lv_obj_set_style_text_font(nameInput, FONT_NORMAL, 0);
  lv_obj_set_style_pad_all(nameInput, 4, 0);
  lv_obj_add_event_cb(nameInput, textarea_event_cb, LV_EVENT_ALL, nullptr);

  // ── MODE label (SVG: y=118, "MODE" in #4A4A4A) ──
  lv_obj_t* modeLabel = lv_label_create(screen);
  lv_label_set_text(modeLabel, "MODE - tap to add/remove | thick border = RUN");
  lv_obj_set_style_text_font(modeLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(modeLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(modeLabel, 20, 118);
  lv_obj_set_width(modeLabel, 760);
  lv_label_set_long_mode(modeLabel, LV_LABEL_LONG_MODE_WRAP);

  // ── MODE selector (y=136): CONT | PULSE | STEP (multi-mask + single RUN)
  const struct ModeBtn {
    const char* label;
    SystemState mode;
  } modes[] = {
    { "CONT",  STATE_RUNNING },
    { "PULSE", STATE_PULSE   },
    { "STEP",  STATE_STEP    }
  };
  const int modeCount = (int)(sizeof(modes) / sizeof(modes[0]));

  const int modeY = 148;
  const int modeBtnH = 38;
  const int modeGap = 6;
  const int modeStartX = 20;
  const int modeRowW = SCREEN_W - 2 * modeStartX;
  const int modeBtnW = (modeRowW - (modeCount - 1) * modeGap) / modeCount;

  for (int i = 0; i < modeCount; i++) {
    lv_obj_t* btn = lv_button_create(screen);
    lv_obj_set_size(btn, modeBtnW, modeBtnH);
    lv_obj_set_pos(btn, modeStartX + i * (modeBtnW + modeGap), modeY);
    lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, mode_toggle_cb, LV_EVENT_CLICKED, (void*)(size_t)modes[i].mode);

    modeBtns[i] = btn;

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, modes[i].label);
    lv_obj_set_style_text_font(lbl, FONT_NORMAL, 0);
    lv_obj_center(lbl);
  }

  // Apply initial style to mode buttons
  restyle_mode_buttons();

  // ── RPM section label (SVG: y=186, "RPM" in #4A4A4A) ──
  lv_obj_t* rpmSectionLabel = lv_label_create(screen);
  lv_label_set_text(rpmSectionLabel, "RPM");
  lv_obj_set_style_text_font(rpmSectionLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(rpmSectionLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(rpmSectionLabel, 20, 198);

  // ── RPM row (SVG: y=204, large value + progress bar + -/+ buttons) ──
  // RPM value display (SVG: large "2.0" in #FF9500 bold)
  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_set_pos(rpmLabel, 20, 216);

  // RPM progress bar (SVG: 20,252,760,3, track #111, fill #FF9500)
  rpmBarObj = lv_bar_create(screen);
  lv_obj_set_size(rpmBarObj, 560, 3);
  lv_obj_set_pos(rpmBarObj, 20, 264);
  lv_obj_set_style_bg_color(rpmBarObj, COL_GAUGE_BG, 0);
  lv_obj_set_style_border_width(rpmBarObj, 0, 0);
  lv_obj_set_style_radius(rpmBarObj, 1, 0);
  {
    float mx = speed_get_rpm_max();
    lv_bar_set_range(rpmBarObj, (int32_t)(MIN_RPM * 1000.0f + 0.5f), (int32_t)(mx * 1000.0f + 0.5f));
  }
  lv_bar_set_value(rpmBarObj, (int32_t)(editPreset.rpm * 1000.0f + 0.5f), LV_ANIM_OFF);

  // Scale marks on progress bar (derived from MIN_RPM / speed_get_rpm_max())
  char t0[12], t1[12], t2[12], t3[12];
  snprintf(t0, sizeof(t0), "%.2f", (double)MIN_RPM);
  {
    float mx = speed_get_rpm_max();
    snprintf(t1, sizeof(t1), "%.2f", (double)(MIN_RPM + (mx - MIN_RPM) * 0.33f));
    snprintf(t2, sizeof(t2), "%.2f", (double)(MIN_RPM + (mx - MIN_RPM) * 0.66f));
    snprintf(t3, sizeof(t3), "%.2f", (double)mx);
  }
  const char* rpmTicks[] = {t0, t1, t2, t3};
  // Narrow columns so tick text does not run under the +/- buttons (x >= 620).
  const int tickW = 84;
  const int tickX[] = {20, 188, 356, 524};
  for (int i = 0; i < 4; i++) {
    lv_obj_t* tick = lv_label_create(screen);
    lv_label_set_text(tick, rpmTicks[i]);
    lv_obj_set_style_text_font(tick, FONT_TINY, 0);
    lv_obj_set_style_text_color(tick, COL_TEXT_VDIM, 0);
    lv_obj_set_pos(tick, tickX[i], 268);
    lv_label_set_long_mode(tick, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_size(tick, tickW, 16);
    lv_obj_set_style_text_align(tick, (i == 3) ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
  }

  // RPM -/+ buttons (SVG: right side of RPM section)
  lv_obj_t* rpmMinusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmMinusBtn, 80, 42);
  lv_obj_set_pos(rpmMinusBtn, 620, 216);
  ui_btn_style_post(rpmMinusBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_minus_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

  lv_obj_t* minusLabel = lv_label_create(rpmMinusBtn);
  lv_label_set_text(minusLabel, "- 0.1");
  lv_obj_set_style_text_font(minusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(minusLabel, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_center(minusLabel);

  lv_obj_t* rpmPlusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmPlusBtn, 80, 42);
  lv_obj_set_pos(rpmPlusBtn, 704, 216);
  ui_btn_style_post(rpmPlusBtn, UI_BTN_ACCENT);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_minus_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

  lv_obj_t* plusLabel = lv_label_create(rpmPlusBtn);
  lv_label_set_text(plusLabel, "+ 0.1");
  lv_obj_set_style_text_font(plusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(plusLabel, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  lv_obj_center(plusLabel);

  // ── Separator at y=270 ──
  lv_obj_t* line2 = lv_obj_create(screen);
  lv_obj_set_size(line2, SCREEN_W, 1);
  lv_obj_set_pos(line2, 0, 282);
  lv_obj_set_style_bg_color(line2, COL_SEPARATOR, 0);
  lv_obj_set_style_pad_all(line2, 0, 0);
  lv_obj_set_style_border_width(line2, 0, 0);
  lv_obj_set_style_radius(line2, 0, 0);
  lv_obj_remove_flag(line2, LV_OBJ_FLAG_SCROLLABLE);

  // ── Mode-specific settings link button (SVG: y=280, 760x52) ──
  modeSettingsBtn = lv_button_create(screen);
  lv_obj_set_size(modeSettingsBtn, 760, 52);
  lv_obj_set_pos(modeSettingsBtn, 20, 292);
  ui_btn_style_post(modeSettingsBtn, UI_BTN_NORMAL);
  lv_obj_add_event_cb(modeSettingsBtn, mode_settings_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsLabel = lv_label_create(modeSettingsBtn);
  lv_obj_set_style_text_font(settingsLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(settingsLabel, ui_btn_label_color_post(UI_BTN_NORMAL), 0);
  lv_obj_set_pos(settingsLabel, 12, 8);
  lv_label_set_long_mode(settingsLabel, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_size(settingsLabel, 736, 36);

  // Set initial text
  update_mode_settings_text();

  ui_create_separator_line(screen, 0, 354, SCREEN_W, COL_SEPARATOR);

  if (slot >= 0) {
    ui_create_btn(screen, 20, 406, 168, 52, "DELETE", FONT_NORMAL, UI_BTN_DANGER, delete_preset_prompt_cb,
                  nullptr);
    ui_create_btn(screen, 204, 406, 186, 52, "CANCEL", FONT_NORMAL, UI_BTN_NORMAL, cancel_cb, nullptr);
    ui_create_btn(screen, 406, 406, 374, 52, "SAVE", FONT_NORMAL, UI_BTN_ACCENT, save_preset_cb, nullptr);
  } else {
    ui_create_btn(screen, 120, 406, 260, 52, "CANCEL", FONT_NORMAL, UI_BTN_NORMAL, cancel_cb, nullptr);
    ui_create_btn(screen, 420, 406, 260, 52, "SAVE", FONT_NORMAL, UI_BTN_ACCENT, save_preset_cb, nullptr);
  }

  LOG_I("Screen program edit: v2.0 layout created slot %d", slot);
}

// ───────────────────────────────────────────────────────────────────────────────
// UPDATE UI with current preset values
// ───────────────────────────────────────────────────────────────────────────────
void screen_program_edit_poll_keyboard() {
  if (kbClosePending.load(std::memory_order_acquire)) {
    do_cleanup_kb();
  }
}

void screen_program_edit_update_ui() {
    // Update RPM label
    if (rpmLabel) {
        lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
    }
    if (rpmBarObj) {
        lv_bar_set_value(rpmBarObj, (int32_t)(editPreset.rpm * 1000.0f + 0.5f), LV_ANIM_OFF);
    }

    // Update mode settings text
    update_mode_settings_text();

    // Restyle mode buttons
    restyle_mode_buttons();
}

// ───────────────────────────────────────────────────────────────────────────────
// PUBLIC API
// ───────────────────────────────────────────────────────────────────────────────
Preset* screen_program_edit_get_preset() {
  return &editPreset;
}

void screen_program_edit_invalidate_widgets() {
  nameInput = nullptr;
  rpmLabel = nullptr;
  rpmBarObj = nullptr;
  modeSettingsBtn = nullptr;
  keyboard = nullptr;
  kbClosePending.store(false, std::memory_order_release);
  for (int i = 0; i < 4; i++) modeBtns[i] = nullptr;
}
