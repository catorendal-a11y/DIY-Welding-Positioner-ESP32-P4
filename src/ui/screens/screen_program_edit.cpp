// TIG Rotator Controller - Program Edit Screen
// Brutalist v2.0 design matching new_ui.svg SCREEN_PROGRAM_EDIT

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
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
static lv_obj_t* modeSettingsBtn = nullptr;
static lv_obj_t* keyboard = nullptr;

// Track mode toggle button objects for restyling
static lv_obj_t* modeBtns[4] = { nullptr };

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) {
  if (keyboard) { lv_obj_delete(keyboard); keyboard = nullptr; }
  screens_show(SCREEN_PROGRAMS);
}

static void keyboard_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
    if (keyboard) {
      lv_obj_delete(keyboard);
      keyboard = nullptr;
    }
    if (nameInput) {
      lv_obj_clear_state(nameInput, LV_STATE_FOCUSED);
    }
  }
}

static void textarea_event_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  if (code == LV_EVENT_FOCUSED) {
    if (!keyboard) {
      keyboard = lv_keyboard_create(lv_screen_active());
      lv_keyboard_set_textarea(keyboard, ta);
      lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_ALL, nullptr);

      // Style keyboard to match brutalist theme
      lv_obj_set_style_bg_color(keyboard, COL_BG, 0);
      lv_obj_set_style_border_width(keyboard, 2, 0);
      lv_obj_set_style_border_color(keyboard, COL_ACCENT, 0);
      lv_obj_set_style_pad_all(keyboard, 4, 0);
    }
  }
}

// Restyle all mode buttons to reflect current selection
static void restyle_mode_buttons() {
  const struct {
    const char* label;
    SystemState mode;
  } modes[] = {
    { "CONT",  STATE_RUNNING },
    { "PULSE", STATE_PULSE   },
    { "STEP",  STATE_STEP    },
    { "TIMER", STATE_TIMER   }
  };

  for (int i = 0; i < 4; i++) {
    if (!modeBtns[i]) continue;
    bool active = (editPreset.mode == modes[i].mode);

    lv_obj_set_style_bg_color(modeBtns[i], active ? COL_BG_ACTIVE : COL_BTN_BG, 0);
    lv_obj_set_style_border_width(modeBtns[i], active ? 2 : 1, 0);
    lv_obj_set_style_border_color(modeBtns[i], active ? COL_ACCENT : COL_BORDER, 0);

    lv_obj_t* lbl = lv_obj_get_child(modeBtns[i], 0);
    if (lbl) {
      lv_obj_set_style_text_color(lbl, active ? COL_ACCENT : COL_TEXT, 0);
    }
  }
}

// Update the mode settings info button text
static void update_mode_settings_text() {
  if (!modeSettingsBtn) return;

  const char* modeName = "";
  char details[64] = "";

  switch (editPreset.mode) {
    case STATE_RUNNING:
      modeName = "CONTINUOUS";
      snprintf(details, sizeof(details), "No additional settings");
      break;
    case STATE_PULSE:
      modeName = "PULSE";
      snprintf(details, sizeof(details), "ON: %.1fs . OFF: %.1fs",
               editPreset.pulse_on_ms / 1000.0f, editPreset.pulse_off_ms / 1000.0f);
      break;
    case STATE_STEP:
      modeName = "STEP";
      snprintf(details, sizeof(details), "ANGLE: %.0fdeg", editPreset.step_angle);
      break;
    case STATE_TIMER:
      modeName = "TIMER";
      snprintf(details, sizeof(details), "TIME: %us", (unsigned)(editPreset.timer_ms / 1000));
      break;
    default:
      modeName = "CONTINUOUS";
      snprintf(details, sizeof(details), "No additional settings");
      break;
  }

  // Main text label is child 0
  lv_obj_t* btnLabel = lv_obj_get_child(modeSettingsBtn, 0);
  if (btnLabel) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s SETTINGS  >  %s", modeName, details);
    lv_label_set_text(btnLabel, buf);
  }
}

static void mode_select_cb(lv_event_t* e) {
  SystemState mode = (SystemState)(size_t)lv_event_get_user_data(e);
  editPreset.mode = mode;

  // Restyle all mode toggle buttons
  restyle_mode_buttons();

  // Update mode settings info
  update_mode_settings_text();
}

static void rpm_minus_cb(lv_event_t* e) {
  intptr_t delta = (intptr_t)lv_event_get_user_data(e);
  float adj = (delta < 0) ? -0.1f : 0.1f;
  editPreset.rpm += adj;

  if (editPreset.rpm < MIN_RPM) editPreset.rpm = MIN_RPM;
  if (editPreset.rpm > MAX_RPM) editPreset.rpm = MAX_RPM;

  if (rpmLabel) {
    lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
  }
}

static void mode_settings_cb(lv_event_t* e) {
  // Navigate to mode-specific edit screen
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
    case STATE_TIMER:
      screens_show(SCREEN_EDIT_TIMER);
      break;
    default:
      break;
  }
}

static void cancel_cb(lv_event_t* e) {
  if (keyboard) { lv_obj_delete(keyboard); keyboard = nullptr; }
  screens_show(SCREEN_PROGRAMS);
}

static void save_preset_cb(lv_event_t* e) {
  // Get name from input
  const char* name = nameInput ? lv_textarea_get_text(nameInput) : "";
  if (strlen(name) == 0) {
    // Show error - need a name
    return;
  }

  // If editing existing, find and update. If new, add to vector
  if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
    // Update existing
    g_presets[editSlot] = editPreset;
    g_presets[editSlot].id = editSlot + 1;
  } else {
    // Create new
    if (g_presets.size() >= MAX_PRESETS) {
      // Show error - full
      return;
    }
    editPreset.id = g_presets.size() + 1;
    g_presets.push_back(editPreset);
  }

  // Copy name to correct location
  if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
    // Editing existing - use editSlot as index
    strncpy(g_presets[editSlot].name, name, 31);
    g_presets[editSlot].name[31] = '\0';
  } else {
    // New program - use the newly added index
    if (!g_presets.empty()) {
      strncpy(g_presets[g_presets.size() - 1].name, name, 31);
      g_presets[g_presets.size() - 1].name[31] = '\0';
    }
  }

  // Save to LittleFS
  storage_save_presets();

  // Go back to programs list
  screens_show(SCREEN_PROGRAMS);
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
  lv_obj_clean(screen);

  editSlot = slot;
  keyboard = nullptr;

  // Load preset data if editing existing
  if (slot >= 0 && slot < (int)g_presets.size()) {
    editPreset = g_presets[slot];
  } else {
    // New preset - initialize with current values
    editPreset.id = 0;
    snprintf(editPreset.name, sizeof(editPreset.name), "New Program");
    editPreset.mode = STATE_RUNNING;
    editPreset.rpm = speed_get_target_rpm();
    editPreset.pulse_on_ms = 500;
    editPreset.pulse_off_ms = 500;
    editPreset.step_angle = 90.0f;
    editPreset.timer_ms = 30000;
    editPreset.direction = 0;        // CW
    editPreset.pulse_cycles = 0;     // infinite
    editPreset.step_repeats = 1;
    editPreset.step_dwell_sec = 0.0f;
    editPreset.timer_auto_stop = 1;  // auto stop
    editPreset.cont_soft_start = 0;  // disabled
  }

  // ── Header (SVG: 0,0,800,32, fill=#090909) ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 32);
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
  lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 12, 7);

  // [ESC] button at right of header
  lv_obj_t* escBtn = lv_button_create(header);
  lv_obj_set_size(escBtn, 60, 24);
  lv_obj_set_pos(escBtn, SCREEN_W - 60 - PAD_X, 4);
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

  // ── Separator line at y=32 ──
  lv_obj_t* line1 = lv_obj_create(screen);
  lv_obj_set_size(line1, SCREEN_W, 1);
  lv_obj_set_pos(line1, 0, 32);
  lv_obj_set_style_bg_color(line1, COL_BORDER, 0);
  lv_obj_set_style_pad_all(line1, 0, 0);
  lv_obj_set_style_border_width(line1, 0, 0);
  lv_obj_set_style_radius(line1, 0, 0);
  lv_obj_remove_flag(line1, LV_OBJ_FLAG_SCROLLABLE);

  // ── NAME label (SVG: y=48, "NAME" in #4A4A4A) ──
  lv_obj_t* nameLabel = lv_label_create(screen);
  lv_label_set_text(nameLabel, "NAME");
  lv_obj_set_style_text_font(nameLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(nameLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(nameLabel, 20, 44);

  // ── NAME input (SVG: 20,64,760,36, fill=#0A0A0A, stroke=#333) ──
  nameInput = lv_textarea_create(screen);
  lv_obj_set_size(nameInput, 760, 36);
  lv_obj_set_pos(nameInput, 20, 64);
  lv_textarea_set_placeholder_text(nameInput, "Enter name...");
  lv_textarea_set_max_length(nameInput, 31);
  lv_textarea_set_one_line(nameInput, true);
  lv_textarea_set_text(nameInput, editPreset.name);
  lv_obj_set_style_bg_color(nameInput, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(nameInput, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(nameInput, 1, 0);
  lv_obj_set_style_border_color(nameInput, lv_color_hex(0x333333), 0);
  lv_obj_set_style_text_color(nameInput, COL_TEXT, 0);
  lv_obj_set_style_text_font(nameInput, FONT_NORMAL, 0);
  lv_obj_set_style_pad_all(nameInput, 4, 0);
  lv_obj_add_event_cb(nameInput, textarea_event_cb, LV_EVENT_ALL, nullptr);

  // ── MODE label (SVG: y=118, "MODE" in #4A4A4A) ──
  lv_obj_t* modeLabel = lv_label_create(screen);
  lv_label_set_text(modeLabel, "MODE");
  lv_obj_set_style_text_font(modeLabel, FONT_TINY, 0);
  lv_obj_set_style_text_color(modeLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(modeLabel, 20, 118);

  // ── MODE selector (SVG: y=136, 4 toggle buttons, each 170x38, gap=6.67) ──
  const struct ModeBtn {
    const char* label;
    SystemState mode;
  } modes[] = {
    { "CONT",  STATE_RUNNING },
    { "PULSE", STATE_PULSE   },
    { "STEP",  STATE_STEP    },
    { "TIMER", STATE_TIMER   }
  };

  const int modeY = 136;
  const int modeBtnW = 185;  // (760 - 3*6.67) / 4 ~= 185
  const int modeBtnH = 38;
  const int modeGap = 6;
  const int modeStartX = 20;

  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_button_create(screen);
    lv_obj_set_size(btn, modeBtnW, modeBtnH);
    lv_obj_set_pos(btn, modeStartX + i * (modeBtnW + modeGap), modeY);
    lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_add_event_cb(btn, mode_select_cb, LV_EVENT_CLICKED, (void*)(size_t)modes[i].mode);

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
  lv_obj_set_pos(rpmSectionLabel, 20, 186);

  // ── RPM row (SVG: y=204, large value + progress bar + -/+ buttons) ──
  // RPM value display (SVG: large "2.0" in #FF9500 bold)
  rpmLabel = lv_label_create(screen);
  lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
  lv_obj_set_style_text_font(rpmLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_set_pos(rpmLabel, 20, 204);

  // RPM progress bar (SVG: 20,252,760,3, track #111, fill #FF9500)
  lv_obj_t* rpmBar = lv_bar_create(screen);
  lv_obj_set_size(rpmBar, 560, 3);
  lv_obj_set_pos(rpmBar, 20, 252);
  lv_obj_set_style_bg_color(rpmBar, COL_GAUGE_BG, 0);
  lv_obj_set_style_border_width(rpmBar, 0, 0);
  lv_obj_set_style_radius(rpmBar, 1, 0);
  lv_bar_set_range(rpmBar, (int32_t)(MIN_RPM * 10), (int32_t)(MAX_RPM * 10));
  lv_bar_set_value(rpmBar, (int32_t)(editPreset.rpm * 10), LV_ANIM_OFF);

  // Scale marks on progress bar (SVG: tick marks)
  // Using small labels as tick marks
  const char* rpmTicks[] = {"0.1", "1.0", "2.0", "3.0"};
  const int tickX[] = {20, 200, 400, 560};
  for (int i = 0; i < 4; i++) {
    lv_obj_t* tick = lv_label_create(screen);
    lv_label_set_text(tick, rpmTicks[i]);
    lv_obj_set_style_text_font(tick, FONT_TINY, 0);
    lv_obj_set_style_text_color(tick, COL_TEXT_VDIM, 0);
    lv_obj_set_pos(tick, tickX[i], 256);
  }

  // RPM -/+ buttons (SVG: right side of RPM section)
  lv_obj_t* rpmMinusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmMinusBtn, 80, 42);
  lv_obj_set_pos(rpmMinusBtn, 620, 204);
  lv_obj_set_style_bg_color(rpmMinusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmMinusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmMinusBtn, 1, 0);
  lv_obj_set_style_border_color(rpmMinusBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(rpmMinusBtn, 0, 0);
  lv_obj_set_style_pad_all(rpmMinusBtn, 0, 0);
  lv_obj_add_event_cb(rpmMinusBtn, rpm_minus_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

  lv_obj_t* minusLabel = lv_label_create(rpmMinusBtn);
  lv_label_set_text(minusLabel, "- 0.1");
  lv_obj_set_style_text_font(minusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(minusLabel, COL_TEXT, 0);
  lv_obj_center(minusLabel);

  lv_obj_t* rpmPlusBtn = lv_button_create(screen);
  lv_obj_set_size(rpmPlusBtn, 80, 42);
  lv_obj_set_pos(rpmPlusBtn, 704, 204);
  lv_obj_set_style_bg_color(rpmPlusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(rpmPlusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(rpmPlusBtn, 1, 0);
  lv_obj_set_style_border_color(rpmPlusBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(rpmPlusBtn, 0, 0);
  lv_obj_set_style_pad_all(rpmPlusBtn, 0, 0);
  lv_obj_add_event_cb(rpmPlusBtn, rpm_minus_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

  lv_obj_t* plusLabel = lv_label_create(rpmPlusBtn);
  lv_label_set_text(plusLabel, "+ 0.1");
  lv_obj_set_style_text_font(plusLabel, FONT_SMALL, 0);
  lv_obj_set_style_text_color(plusLabel, COL_TEXT, 0);
  lv_obj_center(plusLabel);

  // ── Separator at y=270 ──
  lv_obj_t* line2 = lv_obj_create(screen);
  lv_obj_set_size(line2, SCREEN_W, 1);
  lv_obj_set_pos(line2, 0, 270);
  lv_obj_set_style_bg_color(line2, COL_SEPARATOR, 0);
  lv_obj_set_style_pad_all(line2, 0, 0);
  lv_obj_set_style_border_width(line2, 0, 0);
  lv_obj_set_style_radius(line2, 0, 0);
  lv_obj_remove_flag(line2, LV_OBJ_FLAG_SCROLLABLE);

  // ── Mode-specific settings link button (SVG: y=280, 760x52) ──
  modeSettingsBtn = lv_button_create(screen);
  lv_obj_set_size(modeSettingsBtn, 760, 52);
  lv_obj_set_pos(modeSettingsBtn, 20, 280);
  lv_obj_set_style_bg_color(modeSettingsBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(modeSettingsBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(modeSettingsBtn, 1, 0);
  lv_obj_set_style_border_color(modeSettingsBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(modeSettingsBtn, 0, 0);
  lv_obj_set_style_pad_all(modeSettingsBtn, 0, 0);
  lv_obj_add_event_cb(modeSettingsBtn, mode_settings_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsLabel = lv_label_create(modeSettingsBtn);
  lv_obj_set_style_text_font(settingsLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(settingsLabel, COL_TEXT, 0);
  lv_obj_set_pos(settingsLabel, 16, 16);

  // Set initial text
  update_mode_settings_text();

  // ── Separator at y=342 ──
  lv_obj_t* line3 = lv_obj_create(screen);
  lv_obj_set_size(line3, SCREEN_W, 1);
  lv_obj_set_pos(line3, 0, 342);
  lv_obj_set_style_bg_color(line3, COL_SEPARATOR, 0);
  lv_obj_set_style_pad_all(line3, 0, 0);
  lv_obj_set_style_border_width(line3, 0, 0);
  lv_obj_set_style_radius(line3, 0, 0);
  lv_obj_remove_flag(line3, LV_OBJ_FLAG_SCROLLABLE);

  // ── CANCEL button (SVG: 120,400,260x52, fill=#141414, stroke=#444) ──
  lv_obj_t* cancelBtn = lv_button_create(screen);
  lv_obj_set_size(cancelBtn, 260, 52);
  lv_obj_set_pos(cancelBtn, 120, 400);
  lv_obj_set_style_bg_color(cancelBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(cancelBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(cancelBtn, 1, 0);
  lv_obj_set_style_border_color(cancelBtn, lv_color_hex(0x444444), 0);
  lv_obj_set_style_shadow_width(cancelBtn, 0, 0);
  lv_obj_set_style_pad_all(cancelBtn, 0, 0);
  lv_obj_add_event_cb(cancelBtn, cancel_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLabel, "CANCEL");
  lv_obj_set_style_text_font(cancelLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(cancelLabel, COL_TEXT, 0);
  lv_obj_center(cancelLabel);

  // ── SAVE button (SVG: 420,400,260x52, fill=#1A1600, stroke=#FF9500) ──
  lv_obj_t* saveBtn = lv_button_create(screen);
  lv_obj_set_size(saveBtn, 260, 52);
  lv_obj_set_pos(saveBtn, 420, 400);
  lv_obj_set_style_bg_color(saveBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(saveBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(saveBtn, 2, 0);
  lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(saveBtn, 0, 0);
  lv_obj_set_style_pad_all(saveBtn, 0, 0);
  lv_obj_add_event_cb(saveBtn, save_preset_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* saveLabel = lv_label_create(saveBtn);
  lv_label_set_text(saveLabel, "SAVE");
  lv_obj_set_style_text_font(saveLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(saveLabel, COL_ACCENT, 0);
  lv_obj_center(saveLabel);

  LOG_I("Screen program edit: v2.0 layout created slot %d", slot);
}

// ───────────────────────────────────────────────────────────────────────────────
// UPDATE UI with current preset values
// ───────────────────────────────────────────────────────────────────────────────
void screen_program_edit_update_ui() {
    // Update RPM label
    if (rpmLabel) {
        lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
    }

    // Update mode settings text
    update_mode_settings_text();

    // Restyle mode buttons
    restyle_mode_buttons();
}

// ───────────────────────────────────────────────────────────────────────────────
// PUBLIC API - Get current preset being edited
// ───────────────────────────────────────────────────────────────────────────────
Preset* screen_program_edit_get_preset() {
  return &editPreset;
}
