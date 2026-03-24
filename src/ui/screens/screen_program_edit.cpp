// TIG Rotator Controller - Program Edit Screen
// Create/edit custom program presets with full parameter control

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
static lv_obj_t* pulseOnLabel = nullptr;
static lv_obj_t* pulseOffLabel = nullptr;
static lv_obj_t* stepAngleLabel = nullptr;
static lv_obj_t* timerLabel = nullptr;

// Mode buttons (container for all 4 buttons)
static lv_obj_t* modeButtonsContainer = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

static void mode_select_cb(lv_event_t* e) {
  SystemState mode = (SystemState)(size_t)lv_event_get_user_data(e);
  editPreset.mode = mode;

  // Update button styles - reset all, highlight selected
  lv_obj_t* btn = lv_event_get_target(e);
  if (modeButtonsContainer) {
      for (int i = 0; i < 4; i++) {
          lv_obj_t* b = lv_obj_get_child(modeButtonsContainer, i);
          if (b) {
              lv_obj_set_style_border_width(b, 1, 0);
              lv_obj_set_style_border_color(b, COL_BORDER_SPD, 0);
          }
      }
  }
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COL_ACCENT, 0);

  // Show/hide relevant fields based on mode
  switch (mode) {
    case STATE_RUNNING:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_PULSE:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_STEP:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_TIMER:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_NONE);
      break;
    default:
      break;
  }
}

static void rpm_adj_cb(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_target(e);
  lv_obj_t* label = lv_obj_get_child(btn, 0);
  int dir = (intptr_t)lv_event_get_user_data(e);

  float val = editPreset.rpm;
  if (dir > 0) val += 0.1f;
  else if (dir < 0 && val > 0.1f) val -= 0.1f;

  // Clamp to range
  if (val < MIN_RPM) val = MIN_RPM;
  if (val > MAX_RPM) val = MAX_RPM;

  editPreset.rpm = val;
  lv_label_set_text_fmt(label, "%.1f", val);
}

static void pulse_adj_cb(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_target(e);
  lv_obj_t* label = lv_obj_get_child(btn->parent, 1); // Get sibling label
  int delta = (intptr_t)lv_event_get_user_data(e);

  if (e->code == LV_EVENT_SHORT_CLICKED) {
    uint32_t* val = (delta > 0) ? &editPreset.pulse_on_ms : &editPreset.pulse_off_ms;
    *val += (int)delta;

    // Clamp range
    if (*val < 100) *val = 100;
    if (*val > 10000) *val = 10000;

    lv_label_set_text_fmt(label, "%d ms", *val);
  }
}

static void step_angle_cb(lv_event_t* e) {
  float angle = (float)(intptr_t)lv_event_get_user_data(e);
  editPreset.step_angle = angle;

  // Update button styles
  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_obj_get_child(stepAngleLabel->parent, i);
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
  }
  lv_obj_set_style_bg_color(lv_event_get_target(e), COL_ACCENT, 0);
}

static void timer_adj_cb(lv_event_t* e) {
  lv_obj_t* btn = lv_event_get_target(e);
  lv_obj_t* label = lv_obj_get_child(btn->parent, 1);
  int delta = (intptr_t)lv_event_get_user_data(e);

  editPreset.timer_ms += delta * 1000;  // Convert to ms

  // Clamp range
  if (editPreset.timer_ms < 1000) editPreset.timer_ms = 1000;
  if (editPreset.timer_ms > 3600000) editPreset.timer_ms = 3600000;

  uint32_t sec = editPreset.timer_ms / 1000;
  lv_label_set_text_fmt(label, "%d sec", (int)sec);
}

static void save_preset_cb(lv_event_t* e) {
  // Validate name
  const char* name = lv_textarea_get_text(nameInput);
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

  // Copy name
  strncpy(g_presets[editPreset.id - 1].name, name, 31);
  g_presets[editPreset.id - 1].name[31] = '\0';

  // Save to LittleFS
  storage_save_presets();

  // Go back to programs list
  screens_show(SCREEN_PROGRAMS);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_program_edit_create(int slot) {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAM_EDIT];
  lv_obj_clean(screen);

  editSlot = slot;

  // Load preset data if editing existing
  if (slot >= 0 && slot < (int)g_presets.size()) {
    editPreset = g_presets[slot];
  } else {
    // New preset - initialize with current values
    editPreset.id = 0;
    snprintf(editPreset.name, sizeof(editPreset.name), "New Program");
    editPreset.mode = control_get_state();
    if (editPreset.mode == STATE_IDLE || editPreset.mode == STATE_ESTOP) {
      editPreset.mode = STATE_RUNNING;
    }
    editPreset.rpm = speed_get_target_rpm();
    editPreset.pulse_on_ms = 500;
    editPreset.pulse_off_ms = 500;
    editPreset.step_angle = 90.0f;
    editPreset.timer_ms = 30000;
  }

  // ── Header with title and cancel button ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 56);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  lv_obj_t* cancelBtn = lv_btn_create(header);
  lv_obj_set_size(cancelBtn, 80, 44);
  lv_obj_set_pos(cancelBtn, 16, 6);
  lv_obj_set_style_bg_color(cancelBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(cancelBtn, 6, 0);
  lv_obj_set_style_border_width(cancelBtn, 1, 0);
  lv_obj_set_style_border_color(cancelBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(cancelBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLabel, "CANCEL");
  lv_obj_set_style_text_font(cancelLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(cancelLabel, COL_TEXT_DIM, 0);
  lv_obj_center(cancelLabel);

  lv_obj_t* title = lv_label_create(header);
  char titleBuf[40];
  snprintf(titleBuf, 40, slot >= 0 ? "EDIT PROGRAM [%d]" : "NEW PROGRAM", slot + 1);
  lv_label_set_text(title, titleBuf);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // ── Name input row ──
  lv_obj_t* nameLabel = lv_label_create(screen);
  lv_label_set_text(nameLabel, "PROGRAM NAME");
  lv_obj_set_style_text_color(nameLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(nameLabel, 20, 70);

  nameInput = lv_textarea_create(screen);
  lv_obj_set_size(nameInput, 760, 44);
  lv_obj_set_pos(nameInput, 20, 95);
  lv_textarea_set_placeholder_text(nameInput, "Enter program name...");
  lv_textarea_set_max_length(nameInput, 31);
  lv_textarea_set_one_line(nameInput, true);
  lv_textarea_set_text(nameInput, editPreset.name);
  lv_obj_set_style_bg_color(nameInput, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(nameInput, 6, 0);

  // ── Mode selection (4 buttons) ──
  lv_obj_t* modeTitle = lv_label_create(screen);
  lv_label_set_text(modeTitle, "OPERATION MODE");
  lv_obj_set_style_text_color(modeTitle, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(modeTitle, 20, 155);

  modeButtonsContainer = lv_obj_create(screen);
  lv_obj_set_size(modeButtonsContainer, 760, 50);
  lv_obj_set_pos(modeButtonsContainer, 20, 180);
  lv_obj_set_style_pad_all(modeButtonsContainer, 4, 0);

  const struct ModeBtn {
    const char* label;
    SystemState mode;
  } modes[] = {
    { "CONTINUOUS", STATE_RUNNING },
    { "PULSE", STATE_PULSE },
    { "STEP", STATE_STEP },
    { "TIMER", STATE_TIMER }
  };

  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_btn_create(modeButtonsContainer);
    lv_obj_set_size(btn, 178, 42);
    lv_obj_set_pos(btn, i * 190, 4);
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER_SPD, 0);
    lv_obj_add_event_cb(btn, mode_select_cb, LV_EVENT_CLICKED, (void*)(size_t)modes[i].mode);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, modes[i].label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_center(lbl);

    // Highlight current mode
    if (editPreset.mode == modes[i].mode) {
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    }
  }

  // ── RPM control ──
  lv_obj_t* rpmTitle = lv_label_create(screen);
  lv_label_set_text(rpmTitle, "SPEED (RPM)");
  lv_obj_set_style_text_color(rpmTitle, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(rpmTitle, 20, 250);

  lv_obj_t* rpmBox = lv_obj_create(screen);
  lv_obj_set_size(rpmBox, 760, 50);
  lv_obj_set_pos(rpmBox, 20, 275);
  lv_obj_set_style_bg_color(rpmBox, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(rpmBox, 6, 0);
  lv_obj_set_style_pad_all(rpmBox, 0, 0);

  lv_obj_t* rpmMinus = lv_btn_create(rpmBox);
  lv_obj_set_size(rpmMinus, 100, 42);
  lv_obj_set_pos(rpmMinus, 4, 4);
  lv_obj_set_style_bg_color(rpmMinus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(rpmMinus, rpm_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)-1);

  lv_obj_t* minusLabel = lv_label_create(rpmMinus);
  lv_label_set_text(minusLabel, "−");
  lv_obj_set_style_text_font(minusLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(minusLabel);

  rpmLabel = lv_label_create(rpmBox);
  lv_label_set_text_fmt(rpmLabel, "%.1f", editPreset.rpm);
  lv_obj_set_style_text_font(rpmLabel, &lv_font_montserrat_20, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* rpmPlus = lv_btn_create(rpmBox);
  lv_obj_set_size(rpmPlus, 100, 42);
  lv_obj_set_align(rpmPlus, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_color(rpmPlus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(rpmPlus, rpm_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)1);

  lv_obj_t* plusLabel = lv_label_create(rpmPlus);
  lv_label_set_text(plusLabel, "+");
  lv_obj_set_style_text_font(plusLabel, &lv_font_montserrat_24, 0);
  lv_obj_center(plusLabel);

  // ── Pulse settings (shown only for PULSE mode) ──
  pulseOnLabel = lv_label_create(screen);
  lv_label_set_text(pulseOnLabel, "PULSE ON TIME");
  lv_obj_set_style_text_color(pulseOnLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(pulseOnLabel, 20, 340);

  lv_obj_t* pulseOnBox = lv_obj_create(screen);
  lv_obj_set_size(pulseOnBox, 370, 50);
  lv_obj_set_pos(pulseOnBox, 20, 365);
  lv_obj_set_style_bg_color(pulseOnBox, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(pulseOnBox, 6, 0);
  lv_obj_set_style_pad_all(pulseOnBox, 0, 0);

  lv_obj_t* onMinus = lv_btn_create(pulseOnBox);
  lv_obj_set_size(onMinus, 80, 36);
  lv_obj_set_pos(onMinus, 4, 7);
  lv_obj_set_style_bg_color(onMinus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(onMinus, pulse_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)-100);

  lv_obj_t* onValLabel = lv_label_create(pulseOnBox);
  lv_label_set_text_fmt(onValLabel, "%d ms", editPreset.pulse_on_ms);
  lv_obj_set_style_text_font(onValLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(onValLabel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* onPlus = lv_btn_create(pulseOnBox);
  lv_obj_set_size(onPlus, 80, 36);
  lv_obj_set_align(onPlus, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_color(onPlus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(onPlus, pulse_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)100);

  lv_obj_t* onPlusLabel = lv_label_create(onPlus);
  lv_label_set_text(onPlusLabel, "+");
  lv_obj_set_style_text_font(onPlusLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(onPlusLabel);

  pulseOffLabel = lv_label_create(screen);
  lv_label_set_text(pulseOffLabel, "PULSE OFF TIME");
  lv_obj_set_style_text_color(pulseOffLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(pulseOffLabel, 410, 340);

  lv_obj_t* pulseOffBox = lv_obj_create(screen);
  lv_obj_set_size(pulseOffBox, 370, 50);
  lv_obj_set_pos(pulseOffBox, 410, 365);
  lv_obj_set_style_bg_color(pulseOffBox, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(pulseOffBox, 6, 0);
  lv_obj_set_style_pad_all(pulseOffBox, 0, 0);

  lv_obj_t* offMinus = lv_btn_create(pulseOffBox);
  lv_obj_set_size(offMinus, 80, 36);
  lv_obj_set_pos(offMinus, 4, 7);
  lv_obj_set_style_bg_color(offMinus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(offMinus, pulse_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)-100);

  lv_obj_t* offValLabel = lv_label_create(pulseOffBox);
  lv_label_set_text_fmt(offValLabel, "%d ms", editPreset.pulse_off_ms);
  lv_obj_set_style_text_font(offValLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(offValLabel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* offPlus = lv_btn_create(pulseOffBox);
  lv_obj_set_size(offPlus, 80, 36);
  lv_obj_set_align(offPlus, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_color(offPlus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(offPlus, pulse_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)100);

  lv_obj_t* offPlusLabel = lv_label_create(offPlus);
  lv_label_set_text(offPlusLabel, "+");
  lv_obj_set_style_text_font(offPlusLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(offPlusLabel);

  // ── Step angle (shown only for STEP mode) ──
  stepAngleLabel = lv_label_create(screen);
  lv_label_set_text(stepAngleLabel, "STEP ANGLE");
  lv_obj_set_style_text_color(stepAngleLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(stepAngleLabel, 20, 340);

  lv_obj_t* stepBox = lv_obj_create(screen);
  lv_obj_set_size(stepBox, 760, 50);
  lv_obj_set_pos(stepBox, 20, 365);
  lv_obj_set_style_bg_color(stepBox, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(stepBox, 6, 0);

  const float angles[] = {45.0f, 90.0f, 180.0f, 360.0f};
  for (int i = 0; i < 4; i++) {
    lv_obj_t* btn = lv_btn_create(stepBox);
    lv_obj_set_size(btn, 175, 42);
    lv_obj_set_pos(btn, 10 + i * 188, 4);
    lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER_SPD, 0);
    lv_obj_add_event_cb(btn, step_angle_cb, LV_EVENT_CLICKED, (void*)(intptr_t)angles[i]);

    lv_obj_t* lbl = lv_label_create(btn);
    char buf[16];
    snprintf(buf, 16, "%.0f°", angles[i]);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, (editPreset.step_angle == angles[i]) ? COL_ACCENT : COL_TEXT_DIM, 0);
    lv_obj_center(lbl);

    // Highlight current selection
    if (editPreset.step_angle == angles[i]) {
      lv_obj_set_style_border_width(btn, 2, 0);
      lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    }
  }

  // ── Timer (shown only for TIMER mode) ──
  timerLabel = lv_label_create(screen);
  lv_label_set_text(timerLabel, "DURATION");
  lv_obj_set_style_text_color(timerLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(timerLabel, 20, 340);

  lv_obj_t* timerBox = lv_obj_create(screen);
  lv_obj_set_size(timerBox, 760, 50);
  lv_obj_set_pos(timerBox, 20, 365);
  lv_obj_set_style_bg_color(timerBox, COL_BG_INPUT, 0);
  lv_obj_set_style_radius(timerBox, 6, 0);
  lv_obj_set_style_pad_all(timerBox, 0, 0);

  lv_obj_t* timerMinus = lv_btn_create(timerBox);
  lv_obj_set_size(timerMinus, 80, 36);
  lv_obj_set_pos(timerMinus, 4, 7);
  lv_obj_set_style_bg_color(timerMinus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(timerMinus, timer_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)-10);

  lv_obj_t* timerValLabel = lv_label_create(timerBox);
  uint32_t sec = editPreset.timer_ms / 1000;
  lv_label_set_text_fmt(timerValLabel, "%d sec", (int)sec);
  lv_obj_set_style_text_font(timerValLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(timerValLabel, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t* timerPlus = lv_btn_create(timerBox);
  lv_obj_set_size(timerPlus, 80, 36);
  lv_obj_set_align(timerPlus, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_set_style_bg_color(timerPlus, COL_BTN_NORMAL, 0);
  lv_obj_add_event_cb(timerPlus, timer_adj_cb, LV_EVENT_SHORT_CLICKED, (void*)10);

  lv_obj_t* timerPlusLabel = lv_label_create(timerPlus);
  lv_label_set_text(timerPlusLabel, "+");
  lv_obj_set_style_text_font(timerPlusLabel, &lv_font_montserrat_20, 0);
  lv_obj_center(timerPlusLabel);

  // ── SAVE button ──
  lv_obj_t* saveBtn = lv_btn_create(screen);
  lv_obj_set_size(saveBtn, 760, 56);
  lv_obj_set_pos(saveBtn, 20, 430);
  lv_obj_set_style_bg_color(saveBtn, COL_GREEN_DARK, 0);
  lv_obj_set_style_radius(saveBtn, 8, 0);
  lv_obj_set_style_border_width(saveBtn, 2, 0);
  lv_obj_set_style_border_color(saveBtn, COL_GREEN, 0);
  lv_obj_add_event_cb(saveBtn, save_preset_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* saveLabel = lv_label_create(saveBtn);
  lv_label_set_text(saveLabel, "SAVE PROGRAM");
  lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(saveLabel, COL_GREEN, 0);
  lv_obj_center(saveLabel);

  // Show/hide fields based on initial mode
  // Manually trigger mode visibility update
  switch (editPreset.mode) {
    case STATE_RUNNING:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_PULSE:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_STEP:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_HIDDEN);
      break;
    case STATE_TIMER:
      lv_obj_add_flag(rpmLabel, LV_OBJ_FLAG_NONE);
      lv_obj_add_flag(pulseOnLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(pulseOffLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(stepAngleLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(timerLabel, LV_OBJ_FLAG_NONE);
      break;
    default:
      break;
  }

  LOG_I("Screen program edit: created slot %d", slot);
}
