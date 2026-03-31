// TIG Rotator Controller - Programs List Screen
// Full CRUD: Create, Read, Update, Delete programs from UI

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../motor/speed.h"
#include "../../control/control.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* programList = nullptr;
static lv_obj_t* newBtn = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

static void load_preset_cb(lv_event_t* e) {
  int id = (int)(size_t)lv_event_get_user_data(e);
  Preset* p = storage_get_preset(id);
  if (p) {
      LOG_I("Loading Preset %d: %s", id, p->name);
      speed_slider_set(p->rpm);

      if (p->mode == STATE_RUNNING) {
          control_start_continuous();
      } else if (p->mode == STATE_PULSE) {
          control_start_pulse(p->pulse_on_ms, p->pulse_off_ms);
      } else if (p->mode == STATE_STEP) {
          control_start_step(p->step_angle);
      } else if (p->mode == STATE_TIMER) {
          control_start_timer(p->timer_ms / 1000);
      }

      screens_show(SCREEN_MAIN);
  }
}

static void edit_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)lv_event_get_user_data(e);
  screen_program_edit_create(slot);
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void delete_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)lv_event_get_user_data(e);
  if (slot >= 0 && slot < (int)g_presets.size()) {
      g_presets.erase(g_presets.begin() + slot);
      // Renumber remaining presets
      for (size_t i = 0; i < g_presets.size(); i++) {
          g_presets[i].id = i + 1;
      }
      storage_save_presets();
      screen_programs_update();
  }
}

static void new_program_cb(lv_event_t* e) {
  screen_program_edit_create(-1);  // -1 = new program
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_programs_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // ── Title bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 60);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);

  // Back button
  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, 44);
  lv_obj_set_pos(backBtn, 16, 8);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, 6, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "PROGRAMS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_TEXT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 110, 0);

  // Count label
  lv_obj_t* countLabel = lv_label_create(header);
  lv_label_set_text(countLabel, "");
  lv_obj_set_style_text_font(countLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(countLabel, COL_TEXT_DIM, 0);
  lv_obj_align(countLabel, LV_ALIGN_RIGHT_MID, -16, 0);

  // ── NEW PROGRAM button (prominent, at top) ──
  newBtn = lv_btn_create(screen);
  lv_obj_set_size(newBtn, 760, 56);
  lv_obj_set_pos(newBtn, 20, 70);
  lv_obj_set_style_bg_color(newBtn, COL_ACCENT_DARK, 0);
  lv_obj_set_style_radius(newBtn, 8, 0);
  lv_obj_set_style_border_width(newBtn, 2, 0);
  lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
  lv_obj_add_event_cb(newBtn, new_program_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* newLabel = lv_label_create(newBtn);
  lv_label_set_text(newLabel, LV_SYMBOL_PLUS " NEW PROGRAM");
  lv_obj_set_style_text_font(newLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(newLabel, COL_ACCENT, 0);
  lv_obj_center(newLabel);

  // ── Programs list (scrollable area) ──
  programList = lv_obj_create(screen);
  lv_obj_set_size(programList, 760, 320);
  lv_obj_set_pos(programList, 20, 140);
  lv_obj_set_style_bg_color(programList, COL_BG_CARD, 0);
  lv_obj_set_style_radius(programList, 8, 0);
  lv_obj_set_style_border_width(programList, 0, 0);
  lv_obj_set_style_pad_all(programList, 8, 0);
  lv_obj_set_scroll_snap_y(programList, LV_SCROLL_SNAP_NONE);

  LOG_I("Screen programs: created");
}

void screen_programs_update() {
  if (!programList) return;
  lv_obj_clean(programList);

  // Update count label in header
  lv_obj_t* header = lv_obj_get_child(lv_screen_get_active(), 0);
  lv_obj_t* countLabel = lv_obj_get_child(header, 2);  // Third child is count
  if (countLabel) {
      lv_label_set_text_fmt(countLabel, "%d / %d", (int)g_presets.size(), MAX_PRESETS);
  }

  // Update NEW button state (disable if full)
  if (g_presets.size() >= MAX_PRESETS) {
      lv_obj_set_style_bg_color(newBtn, COL_BG_CARD, 0);
      lv_obj_set_style_border_color(newBtn, COL_BORDER, 0);
      lv_obj_set_style_opa(newBtn, LV_OPA_50, 0);
  } else {
      lv_obj_set_style_bg_color(newBtn, COL_ACCENT_DARK, 0);
      lv_obj_set_style_border_color(newBtn, COL_ACCENT, 0);
      lv_obj_set_style_opa(newBtn, LV_OPA_COVER, 0);
  }

  int yPos = 8;

  for (size_t i = 0; i < g_presets.size(); i++) {
      const auto& p = g_presets[i];

      // ── Program item card ──
      lv_obj_t* card = lv_obj_create(programList);
      lv_obj_set_size(card, 744, 90);
      lv_obj_set_pos(card, 0, yPos);
      lv_obj_set_style_bg_color(card, COL_BG_INPUT, 0);
      lv_obj_set_style_radius(card, 6, 0);
      lv_obj_set_style_border_width(card, 1, 0);
      lv_obj_set_style_border_color(card, COL_BORDER, 0);
      lv_obj_set_style_pad_all(card, 12, 0);

      // Program info (left side)
      char buf[64];
      const char* mName = control_state_name(p.mode);
      snprintf(buf, sizeof(buf), "%d: %s", p.id, p.name);

      lv_obj_t* nameLabel = lv_label_create(card);
      lv_label_set_text(nameLabel, buf);
      lv_obj_set_style_text_font(nameLabel, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(nameLabel, COL_TEXT, 0);
      lv_obj_set_pos(nameLabel, 8, 8);

      // Mode + RPM info (below name)
      snprintf(buf, sizeof(buf), "%s | %.1f RPM", mName, p.rpm);
      lv_obj_t* infoLabel = lv_label_create(card);
      lv_label_set_text(infoLabel, buf);
      lv_obj_set_style_text_font(infoLabel, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(infoLabel, COL_TEXT_DIM, 0);
      lv_obj_set_pos(infoLabel, 8, 32);

      // Mode-specific details
      if (p.mode == STATE_PULSE) {
          snprintf(buf, sizeof(buf), "ON: %dms  OFF: %dms", p.pulse_on_ms, p.pulse_off_ms);
      } else if (p.mode == STATE_STEP) {
          snprintf(buf, sizeof(buf), "ANGLE: %.0f°", p.step_angle);
      } else if (p.mode == STATE_TIMER) {
          snprintf(buf, sizeof(buf), "TIME: %d sec", p.timer_ms / 1000);
      } else {
          buf[0] = '\0';
      }

      if (buf[0]) {
          lv_obj_t* detailLabel = lv_label_create(card);
          lv_label_set_text(detailLabel, buf);
          lv_obj_set_style_text_font(detailLabel, &lv_font_montserrat_12, 0);
          lv_obj_set_style_text_color(detailLabel, COL_TEXT_DIM, 0);
          lv_obj_set_pos(detailLabel, 8, 52);
      }

      // ── Action buttons (right side) ──
      // PLAY button (load & run)
      lv_obj_t* playBtn = lv_btn_create(card);
      lv_obj_set_size(playBtn, 70, 36);
      lv_obj_set_pos(playBtn, 560, 24);
      lv_obj_set_style_bg_color(playBtn, COL_GREEN_DARK, 0);
      lv_obj_set_style_radius(playBtn, 4, 0);
      lv_obj_set_style_border_width(playBtn, 1, 0);
      lv_obj_set_style_border_color(playBtn, COL_GREEN, 0);
      lv_obj_add_event_cb(playBtn, load_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)p.id);

      lv_obj_t* playLabel = lv_label_create(playBtn);
      lv_label_set_text(playLabel, LV_SYMBOL_PLAY);
      lv_obj_set_style_text_color(playLabel, COL_GREEN, 0);
      lv_obj_center(playLabel);

      // EDIT button
      lv_obj_t* editBtn = lv_btn_create(card);
      lv_obj_set_size(editBtn, 70, 36);
      lv_obj_set_pos(editBtn, 640, 24);
      lv_obj_set_style_bg_color(editBtn, COL_BTN_NORMAL, 0);
      lv_obj_set_style_radius(editBtn, 4, 0);
      lv_obj_set_style_border_width(editBtn, 1, 0);
      lv_obj_set_style_border_color(editBtn, COL_ACCENT, 0);
      lv_obj_add_event_cb(editBtn, edit_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)i);  // Pass slot index

      lv_obj_t* editLabel = lv_label_create(editBtn);
      lv_label_set_text(editLabel, LV_SYMBOL_EDIT);
      lv_obj_set_style_text_color(editLabel, COL_ACCENT, 0);
      lv_obj_center(editLabel);

      // DELETE button
      lv_obj_t* delBtn = lv_btn_create(card);
      lv_obj_set_size(delBtn, 70, 36);
      lv_obj_set_pos(delBtn, 640, 24 + 40);
      lv_obj_set_style_bg_color(delBtn, COL_RED_DARK, 0);
      lv_obj_set_style_radius(delBtn, 4, 0);
      lv_obj_set_style_border_width(delBtn, 1, 0);
      lv_obj_set_style_border_color(delBtn, COL_RED, 0);
      lv_obj_add_event_cb(delBtn, delete_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)i);  // Pass slot index

      lv_obj_t* delLabel = lv_label_create(delBtn);
      lv_label_set_text(delLabel, LV_SYMBOL_CLOSE);
      lv_obj_set_style_text_color(delLabel, COL_RED, 0);
      lv_obj_center(delLabel);

      yPos += 98;  // Card height + spacing
  }

  // Show "No programs" message if empty
  if (g_presets.empty()) {
      lv_obj_t* emptyLabel = lv_label_create(programList);
      lv_label_set_text(emptyLabel, "No programs yet.\nTap NEW PROGRAM to create one.");
      lv_obj_set_style_text_font(emptyLabel, &lv_font_montserrat_16, 0);
      lv_obj_set_style_text_color(emptyLabel, COL_TEXT_DIM, 0);
      lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
  }
}
