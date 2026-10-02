// TIG Rotator Controller - Programs List Screen
// POST mockup #9: simple rows, chevron; tap left = run, tap right = edit; delete in program edit

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/program_executor.h"
#include "../../motor/speed.h"
#include <atomic>
#include <cstdio>
#include <vector>

static lv_obj_t* programList = nullptr;
static lv_obj_t* newBtn = nullptr;
static lv_obj_t* countLabel = nullptr;
static std::atomic<bool> programsDirty{true};
static size_t lastPresetCount = 0;

static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static Preset reviewedPreset;
static void start_reviewed_preset() {
  ProgramExecutorResult result = program_executor_start_preset(&reviewedPreset);
  if (result != PROGRAM_EXEC_OK) LOG_W("Program start blocked: %s", program_executor_result_name(result));
}
static void load_preset_cb(lv_event_t* e) {
  int id = (int)(size_t)lv_event_get_user_data(e);
  if (!storage_get_preset(id, &reviewedPreset)) return;
  char details[280];
  const Preset& p = reviewedPreset;
  const char* mode = p.mode == STATE_PULSE ? "Pulse" : p.mode == STATE_STEP ? "Step" : "Continuous";
  snprintf(
      details, sizeof(details),
      "%s\n%s / %.3f RPM / %s\nPart diameter: %.0f mm\nConfirm starts motion. Cancel keeps the motor idle.",
      p.name, mode, (double)p.rpm, p.direction == DIR_CCW ? "CCW" : "CW",
      (double)(p.workpiece_diameter_mm > 0 ? p.workpiece_diameter_mm : D_EMNE * 1000.0f));
  screen_confirm_create("START PROGRAM", details, start_reviewed_preset, nullptr, SCREEN_MAIN);
}

static void edit_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)lv_event_get_user_data(e);
  screens_set_edit_slot(slot);
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void new_program_cb(lv_event_t* e) {
  (void)e;
  screens_set_edit_slot(-1);
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void format_details(char* buf, size_t len, const Preset& p) {
  uint8_t mask = p.mode_mask ? p.mode_mask : preset_mode_to_mask(p.mode);
  mask = (uint8_t)(mask & PRESET_MASK_ALL);

  if (preset_mask_popcount(mask) > 1) {
    char tags[8];
    int tp = 0;
    if (mask & PRESET_MASK_CONT) tags[tp++] = 'C';
    if (mask & PRESET_MASK_PULSE) tags[tp++] = 'P';
    if (mask & PRESET_MASK_STEP) tags[tp++] = 'S';
    tags[tp] = '\0';
    const char* runC = "C";
    if (p.mode == STATE_PULSE)
      runC = "P";
    else if (p.mode == STATE_STEP)
      runC = "S";
    if (p.workpiece_diameter_mm >= 1.0f) {
      snprintf(buf, len, "[%s] run:%s | %.1f RPM | OD %.0f mm", tags, runC, p.rpm, p.workpiece_diameter_mm);
    } else {
      snprintf(buf, len, "[%s] run:%s | %.1f RPM", tags, runC, p.rpm);
    }
    return;
  }

  const char* mName = "";
  switch (p.mode) {
    case STATE_RUNNING:
      mName = "CONT";
      break;
    case STATE_PULSE:
      mName = "PULSE";
      break;
    case STATE_STEP:
      mName = "STEP";
      break;
    default:
      mName = "CONT";
      break;
  }

  if (p.mode == STATE_PULSE) {
    snprintf(buf, len, "%s / %.1f RPM / %.1f/%.1fs", mName, p.rpm, p.pulse_on_ms / 1000.0f,
             p.pulse_off_ms / 1000.0f);
  } else if (p.mode == STATE_STEP) {
    char od[18] = "";
    if (p.workpiece_diameter_mm >= 1.0f) {
      snprintf(od, sizeof(od), " | OD %.0f", p.workpiece_diameter_mm);
    }
    if (p.step_repeats > 1) {
      snprintf(buf, len, "%s / %.1f RPM / %.0f deg x%u%s", mName, p.rpm, p.step_angle,
               (unsigned)p.step_repeats, od);
    } else {
      snprintf(buf, len, "%s / %.1f RPM / %.0f deg%s", mName, p.rpm, p.step_angle, od);
    }
  } else {
    snprintf(buf, len, "%s / %.1f RPM", mName, p.rpm);
  }
}

void screen_programs_invalidate_widgets() {
  programList = nullptr;
  countLabel = nullptr;
  newBtn = nullptr;
  programsDirty.store(true, std::memory_order_relaxed);
}

void screen_programs_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];
  lv_obj_clean(screen);
  ui_create_header(screen, "Welding programs", "", &countLabel);
  programList = lv_obj_create(screen);
  lv_obj_set_size(programList, 800, 284);
  lv_obj_set_pos(programList, 0, 94);
  lv_obj_set_style_bg_opa(programList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(programList, 0, 0);
  lv_obj_set_style_pad_all(programList, 0, 0);
  lv_obj_set_scrollbar_mode(programList, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(programList, LV_DIR_VER);
  ui_create_btn(screen, 24, 408, 152, 56, "<  BACK", FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  ui_create_text(screen, 192, 424, 260, "Tap a program to review", FONT_NORMAL, COL_TEXT_DIM);
  newBtn =
      ui_create_btn(screen, 496, 408, 280, 56, "+ NEW", FONT_BTN, UI_BTN_ACCENT, new_program_cb, nullptr);
  programsDirty.store(true, std::memory_order_relaxed);
}

void screen_programs_update() {
  if (!programList || !newBtn || !countLabel) return;

  std::vector<Preset> snapshot;
  size_t presetCount = 0;
  {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    presetCount = g_presets.size();
    if (!programsDirty.load(std::memory_order_relaxed) && presetCount == lastPresetCount) {
      xSemaphoreGive(g_presets_mutex);
      return;
    }
    programsDirty.store(false, std::memory_order_relaxed);
    lastPresetCount = presetCount;
    snapshot = g_presets;
    xSemaphoreGive(g_presets_mutex);
  }

  lv_obj_clean(programList);
  bool isFull = presetCount >= MAX_PRESETS;
  bool isEmpty = snapshot.empty();

  lv_label_set_text_fmt(countLabel, "%d/%d", (int)presetCount, MAX_PRESETS);

  if (isFull) {
    ui_btn_style_post(newBtn, UI_BTN_NORMAL);
    lv_obj_set_style_opa(newBtn, LV_OPA_50, 0);
    lv_obj_remove_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* nl = lv_obj_get_child(newBtn, 0);
    if (nl) lv_obj_set_style_text_color(nl, COL_TEXT_VDIM, 0);
  } else {
    ui_btn_style_post(newBtn, UI_BTN_ACCENT);
    lv_obj_set_style_opa(newBtn, LV_OPA_COVER, 0);
    lv_obj_add_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* nl = lv_obj_get_child(newBtn, 0);
    if (nl) lv_obj_set_style_text_color(nl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  }

  const int cardW = 752;
  const int cardH = 82;
  const int cardGap = 12;
  const int cardX = 24;
  const int runHitW = 620;
  const int editHitW = cardW - runHitW;
  int yPos = 0;

  for (size_t i = 0; i < presetCount; i++) {
    const auto& p = snapshot[i];
    const bool hilite = (i == 0);

    lv_obj_t* card = lv_obj_create(programList);
    lv_obj_set_size(card, cardW, cardH);
    lv_obj_set_pos(card, cardX, yPos);
    lv_obj_set_style_bg_color(card, hilite ? COL_BG_ACTIVE : COL_BG_CARD, 0);
    lv_obj_set_style_radius(card, RADIUS_CARD, 0);
    lv_obj_set_style_border_width(card, hilite ? 2 : 1, 0);
    lv_obj_set_style_border_color(card, hilite ? COL_ACCENT : COL_BORDER, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);

    char detailBuf[96];
    format_details(detailBuf, sizeof(detailBuf), p);

    lv_obj_t* nameLabel = lv_label_create(card);
    lv_label_set_text(nameLabel, p.name);
    lv_obj_set_style_text_font(nameLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(nameLabel, hilite ? COL_ACCENT : COL_TEXT_BRIGHT, 0);
    lv_obj_set_pos(nameLabel, 22, 12);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nameLabel, runHitW - 36);

    lv_obj_t* detailLabel = lv_label_create(card);
    lv_label_set_text(detailLabel, detailBuf);
    lv_obj_set_style_text_font(detailLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(detailLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(detailLabel, 22, 38);
    lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(detailLabel, runHitW - 36);

    lv_obj_t* runBtn = lv_button_create(card);
    lv_obj_set_size(runBtn, runHitW, cardH);
    lv_obj_set_pos(runBtn, 0, 0);
    lv_obj_set_style_bg_opa(runBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(runBtn, 0, 0);
    lv_obj_set_style_shadow_width(runBtn, 0, 0);
    lv_obj_add_event_cb(runBtn, load_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)p.id);

    lv_obj_t* editBtn = lv_button_create(card);
    lv_obj_set_size(editBtn, editHitW, cardH);
    lv_obj_set_pos(editBtn, runHitW, 0);
    lv_obj_set_style_bg_opa(editBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(editBtn, 0, 0);
    lv_obj_set_style_shadow_width(editBtn, 0, 0);
    lv_obj_add_event_cb(editBtn, edit_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)i);

    lv_obj_t* chev = lv_label_create(editBtn);
    lv_label_set_text(chev, "EDIT");
    lv_obj_set_style_text_font(chev, FONT_BTN, 0);
    lv_obj_set_style_text_color(chev, hilite ? COL_ACCENT : COL_TEXT_VDIM, 0);
    lv_obj_center(chev);

    yPos += cardH + cardGap;
  }

  if (isEmpty) {
    lv_obj_t* emptyLabel = lv_label_create(programList);
    lv_label_set_text(emptyLabel, "No programs yet.\nTap + NEW to create one.");
    lv_obj_set_style_text_font(emptyLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(emptyLabel, COL_TEXT_DIM, 0);
    lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
  }
}

void screen_programs_mark_dirty() { programsDirty.store(true, std::memory_order_relaxed); }
