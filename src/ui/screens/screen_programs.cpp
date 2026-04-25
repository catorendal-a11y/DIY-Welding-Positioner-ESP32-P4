// TIG Rotator Controller - Programs List Screen
// Brutalist v2.0 design matching new_ui.svg SCREEN_PROGRAMS

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/program_executor.h"
#include <atomic>
#include <cstdio>
#include <vector>

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* programList = nullptr;
static lv_obj_t* newBtn = nullptr;
static lv_obj_t* countLabel = nullptr;
static int deleteSlot = -1;
static std::atomic<bool> programsDirty{true};
static size_t lastPresetCount = 0;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void load_preset_cb(lv_event_t* e) {
  int id = (int)(size_t)(lv_obj_t*)lv_event_get_user_data(e);
  Preset p;
  if (storage_get_preset(id, &p)) {
    LOG_I("Loading Preset %d: %s", id, p.name);
    if (p.mode == STATE_PULSE) {
      // Main "PULSE" side button uses its own on/off ms — match the program so UI and next tap agree.
      screen_main_set_program_pulse_times(p.pulse_on_ms, p.pulse_off_ms);
    }

    ProgramExecutorResult result = program_executor_start_preset(&p);
    if (result == PROGRAM_EXEC_OK) {
      screens_show(SCREEN_MAIN);
    } else {
      LOG_W("Program start failed: %s", program_executor_result_name(result));
    }
  }
}

static void edit_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)(lv_obj_t*)lv_event_get_user_data(e);
  screens_set_edit_slot(slot);
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// DELETE CONFIRMATION
// ───────────────────────────────────────────────────────────────────────────────
static void do_delete_preset() {
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (deleteSlot >= 0 && deleteSlot < (int)g_presets.size()) {
    g_presets.erase(g_presets.begin() + deleteSlot);
    for (size_t i = 0; i < g_presets.size(); i++) {
      g_presets[i].id = i + 1;
    }
    xSemaphoreGive(g_presets_mutex);
    storage_save_presets();
  } else {
    xSemaphoreGive(g_presets_mutex);
  }
  deleteSlot = -1;
  programsDirty.store(true, std::memory_order_relaxed);
}

static void delete_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)(lv_obj_t*)lv_event_get_user_data(e);
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (slot >= 0 && slot < (int)g_presets.size()) {
    deleteSlot = slot;
    char buf[64];
    const auto& p = g_presets[slot];
    snprintf(buf, sizeof(buf), "Delete \"%s\"?", p.name);
    xSemaphoreGive(g_presets_mutex);
    screen_confirm_create(buf, "This action cannot be undone.", do_delete_preset, nullptr);
  } else {
    xSemaphoreGive(g_presets_mutex);
  }
}

static void new_program_cb(lv_event_t* e) {
  screens_set_edit_slot(-1);
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
// Format program detail string for the card subtitle
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
    if (p.mode == STATE_PULSE) runC = "P";
    else if (p.mode == STATE_STEP) runC = "S";
    snprintf(buf, len, "[%s] run:%s | %.1f RPM", tags, runC, p.rpm);
    return;
  }

  const char* mName = "";
  switch (p.mode) {
    case STATE_RUNNING: mName = "CONT"; break;
    case STATE_PULSE:   mName = "PULSE"; break;
    case STATE_STEP:    mName = "STEP"; break;
    default:            mName = "CONT"; break;
  }

  if (p.mode == STATE_PULSE) {
    snprintf(buf, len, "%s | %.1f RPM | %.1f/%.1fs",
             mName, p.rpm,
             p.pulse_on_ms / 1000.0f, p.pulse_off_ms / 1000.0f);
  } else if (p.mode == STATE_STEP) {
    if (p.step_repeats > 1) {
      snprintf(buf, len, "%s | %.1f RPM | %.0f deg x%u", mName, p.rpm, p.step_angle,
               (unsigned)p.step_repeats);
    } else {
      snprintf(buf, len, "%s | %.1f RPM | %.0f deg", mName, p.rpm, p.step_angle);
    }
  } else {
    snprintf(buf, len, "%s | %.1f RPM", mName, p.rpm);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE -- new_ui.svg SCREEN_PROGRAMS
// Header (0,0,800,32): #090909, "PROGRAMS (N/M)" at (12,22) #FF9500 bold
// "+ NEW PROGRAM" button at (628,6,162x20): .ba style
// Line at y=32
// Cards start at y=40, each 784x56 with 8px gap
// BACK button at (16,416,180x36) .sm style
// ───────────────────────────────────────────────────────────────────────────────
void screen_programs_invalidate_widgets() {
  programList = nullptr;
  countLabel = nullptr;
  newBtn = nullptr;
  programsDirty.store(true, std::memory_order_relaxed);
}

void screen_programs_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Header ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 38);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "PROGRAMS");
  lv_obj_set_style_text_font(title, FONT_LARGE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, 14, 6);

  // Count label
  countLabel = lv_label_create(header);
  lv_label_set_text(countLabel, "");
  lv_obj_set_style_text_font(countLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(countLabel, COL_ACCENT, 0);
  lv_obj_set_pos(countLabel, 160, 11);

  // "+ NEW" button
  newBtn = lv_button_create(header);
  lv_obj_set_size(newBtn, 192, 34);
  lv_obj_set_pos(newBtn, 596, 2);
  lv_obj_set_style_bg_color(newBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(newBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(newBtn, 1, 0);
  lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(newBtn, 0, 0);
  lv_obj_set_style_pad_all(newBtn, 0, 0);
  lv_obj_add_event_cb(newBtn, new_program_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* newLabel = lv_label_create(newBtn);
  lv_label_set_text(newLabel, "+ NEW");
  lv_obj_set_style_text_font(newLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(newLabel, COL_ACCENT, 0);
  lv_obj_center(newLabel);

  ui_create_separator(screen, 38);

  // ── Program cards list (scrollable) ──
  // Leave a strip below for the hint so it does not paint over the last cards.
  programList = lv_obj_create(screen);
  lv_obj_set_size(programList, SCREEN_W, 348);
  lv_obj_set_pos(programList, 0, 46);
  lv_obj_set_style_bg_color(programList, COL_BG, 0);
  lv_obj_set_style_radius(programList, 0, 0);
  lv_obj_set_style_border_width(programList, 0, 0);
  lv_obj_set_style_pad_all(programList, 0, 0);
  // Vertical scrollbar can steal width and clip the right edge of wide cards.
  lv_obj_set_scrollbar_mode(programList, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(programList, LV_DIR_VER);

  // ── "scroll for more" hint (between list and BACK; was y=396 inside list area) ──
  lv_obj_t* scrollHint = lv_label_create(screen);
  lv_label_set_text(scrollHint, "scroll for more");
  lv_obj_set_style_text_font(scrollHint, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(scrollHint, COL_TEXT_VDIM, 0);
  lv_obj_align(scrollHint, LV_ALIGN_TOP_MID, 0, 400);

  ui_create_btn(screen, 10, 424, 240, 52, "<  BACK", FONT_LARGE, UI_BTN_NORMAL, back_event_cb, nullptr);

  LOG_I("Screen programs: v2.0 layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// UPDATE -- rebuild card list from g_presets
// ───────────────────────────────────────────────────────────────────────────────
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

  // LVGL work must NOT run under g_presets_mutex (long section + re-entrancy risk).
  lv_obj_clean(programList);
  bool isFull = presetCount >= MAX_PRESETS;
  bool isEmpty = snapshot.empty();

  lv_label_set_text_fmt(countLabel, "(%d/%d)", (int)presetCount, MAX_PRESETS);

  if (isFull) {
    lv_obj_set_style_bg_color(newBtn, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(newBtn, COL_BORDER, 0);
    lv_obj_set_style_opa(newBtn, LV_OPA_50, 0);
    lv_obj_remove_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_obj_set_style_bg_color(newBtn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
    lv_obj_set_style_opa(newBtn, LV_OPA_COVER, 0);
    lv_obj_add_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
  }

  const int cardW = 776;
  const int cardH = 86;
  const int cardGap = 10;
  const int cardX = (SCREEN_W - cardW) / 2;
  // Text column ends before PLAY/EDIT/DEL (first button at cardW - 246).
  const int cardTextLeft = 60;
  const int cardTextMaxW = (cardW - 246 - 8) - cardTextLeft;
  int yPos = 0;

  for (size_t i = 0; i < presetCount; i++) {
    const auto& p = snapshot[i];

    // ── Card background (SVG: 784x56, fill=#0D0D0D odd / #0B0B0B even) ──
    lv_obj_t* card = lv_obj_create(programList);
    lv_obj_set_size(card, cardW, cardH);
    lv_obj_set_pos(card, cardX, yPos);
    lv_obj_set_style_bg_color(card, (i % 2 == 0) ? COL_BG_CARD : COL_BG_CARD_ALT, 0);
    lv_obj_set_style_radius(card, RADIUS_CARD, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_BORDER, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_CLICKABLE);

    // ── Green left accent bar ──
    lv_obj_t* accent = lv_obj_create(card);
    lv_obj_set_size(accent, 3, cardH - 10);
    lv_obj_set_pos(accent, 0, 5);
    lv_obj_set_style_bg_color(accent, COL_GREEN, 0);
    lv_obj_set_style_radius(accent, 1, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_pad_all(accent, 0, 0);
    lv_obj_remove_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(accent, LV_OBJ_FLAG_CLICKABLE);

    // ── P01 label ──
    lv_obj_t* idLabel = lv_label_create(card);
    char idBuf[8];
    snprintf(idBuf, sizeof(idBuf), "P%02d", p.id);
    lv_label_set_text(idLabel, idBuf);
    lv_obj_set_style_text_font(idLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(idLabel, COL_GREEN, 0);
    lv_obj_set_pos(idLabel, 12, 8);

    // ── Name label ──
    lv_obj_t* nameLabel = lv_label_create(card);
    lv_label_set_text(nameLabel, p.name);
    lv_obj_set_style_text_font(nameLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(nameLabel, COL_TEXT_BRIGHT, 0);
    lv_obj_set_pos(nameLabel, cardTextLeft, 8);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nameLabel, cardTextMaxW);

    // ── Details label ──
    char detailBuf[80];
    format_details(detailBuf, sizeof(detailBuf), p);
    lv_obj_t* detailLabel = lv_label_create(card);
    lv_label_set_text(detailLabel, detailBuf);
    lv_obj_set_style_text_font(detailLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(detailLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(detailLabel, cardTextLeft, 32);
    lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(detailLabel, cardTextMaxW);

    // ── Action buttons — 76x58 each ──
    // PLAY button
    lv_obj_t* playBtn = lv_button_create(card);
    lv_obj_set_size(playBtn, 76, 58);
    lv_obj_set_pos(playBtn, cardW - 246, 14);
    lv_obj_set_style_bg_color(playBtn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_radius(playBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(playBtn, 1, 0);
    lv_obj_set_style_border_color(playBtn, COL_ACCENT, 0);
    lv_obj_set_style_shadow_width(playBtn, 0, 0);
    lv_obj_set_style_pad_all(playBtn, 0, 0);
    lv_obj_add_event_cb(playBtn, load_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)p.id);

    lv_obj_t* playLabel = lv_label_create(playBtn);
    lv_label_set_text(playLabel, "RUN");
    lv_obj_set_style_text_font(playLabel, FONT_BTN, 0);
    lv_obj_set_style_text_color(playLabel, COL_ACCENT, 0);
    lv_obj_center(playLabel);

    // EDIT button
    lv_obj_t* editBtn = lv_button_create(card);
    lv_obj_set_size(editBtn, 76, 58);
    lv_obj_set_pos(editBtn, cardW - 164, 14);
    lv_obj_set_style_bg_color(editBtn, COL_BTN_BG, 0);
    lv_obj_set_style_radius(editBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(editBtn, 1, 0);
    lv_obj_set_style_border_color(editBtn, COL_BORDER_SM, 0);
    lv_obj_set_style_shadow_width(editBtn, 0, 0);
    lv_obj_set_style_pad_all(editBtn, 0, 0);
    lv_obj_add_event_cb(editBtn, edit_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)i);

    lv_obj_t* editLabel = lv_label_create(editBtn);
    lv_label_set_text(editLabel, "EDIT");
    lv_obj_set_style_text_font(editLabel, FONT_BTN, 0);
    lv_obj_set_style_text_color(editLabel, COL_TEXT, 0);
    lv_obj_center(editLabel);

    // DELETE button
    lv_obj_t* delBtn = lv_button_create(card);
    lv_obj_set_size(delBtn, 76, 58);
    lv_obj_set_pos(delBtn, cardW - 82, 14);
    lv_obj_set_style_bg_color(delBtn, COL_BG_DANGER, 0);
    lv_obj_set_style_radius(delBtn, RADIUS_BTN, 0);
    lv_obj_set_style_border_width(delBtn, 1, 0);
    lv_obj_set_style_border_color(delBtn, COL_RED, 0);
    lv_obj_set_style_shadow_width(delBtn, 0, 0);
    lv_obj_set_style_pad_all(delBtn, 0, 0);
    lv_obj_add_event_cb(delBtn, delete_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)i);

    lv_obj_t* delLabel = lv_label_create(delBtn);
    lv_label_set_text(delLabel, "DEL");
    lv_obj_set_style_text_font(delLabel, FONT_BTN, 0);
    lv_obj_set_style_text_color(delLabel, COL_RED, 0);
    lv_obj_center(delLabel);

    yPos += cardH + cardGap;
  }

  // Placeholder rows for unused slots (only when there is at least one program —
  // otherwise the centered empty state below is enough; showing both looked broken.)
  if (!isEmpty) {
    int emptyStart = (int)snapshot.size();
    int emptyEnd = emptyStart + 4;
    if (emptyEnd > MAX_PRESETS) emptyEnd = MAX_PRESETS;

    for (int i = emptyStart; i < emptyEnd; i++) {
      lv_obj_t* emptyCard = lv_obj_create(programList);
      lv_obj_set_size(emptyCard, cardW, 44);
      lv_obj_set_pos(emptyCard, cardX, yPos);
      lv_obj_set_style_bg_color(emptyCard, COL_BG_EMPTY, 0);
      lv_obj_set_style_radius(emptyCard, RADIUS_BTN, 0);
      lv_obj_set_style_border_width(emptyCard, 0, 0);
      lv_obj_set_style_pad_all(emptyCard, 0, 0);
      lv_obj_remove_flag(emptyCard, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_remove_flag(emptyCard, LV_OBJ_FLAG_CLICKABLE);

      lv_obj_t* slotLabel = lv_label_create(emptyCard);
      char slotBuf[16];
      snprintf(slotBuf, sizeof(slotBuf), "P%02d -- empty", i + 1);
      lv_label_set_text(slotLabel, slotBuf);
      lv_obj_set_style_text_font(slotLabel, FONT_BTN, 0);
      lv_obj_set_style_text_color(slotLabel, COL_TEXT_VDIM, 0);
      lv_obj_set_pos(slotLabel, 12, 13);

      yPos += 44 + 5;
    }
  }

  if (isEmpty) {
    lv_obj_t* emptyLabel = lv_label_create(programList);
    lv_label_set_text(emptyLabel, "No programs yet.\nTap + NEW to create one.");
    lv_obj_set_style_text_font(emptyLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(emptyLabel, COL_TEXT_DIM, 0);
    lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
  }
}

void screen_programs_mark_dirty() {
  programsDirty.store(true, std::memory_order_relaxed);
}
