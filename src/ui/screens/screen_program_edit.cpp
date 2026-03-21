// TIG Rotator Controller - Program Edit Screen (T-26)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_program_edit_create(int slot) {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAM_EDIT];

  // Clear previous content
  lv_obj_clean(screen);

  // Header
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t* cancelBtn = lv_btn_create(header);
  lv_obj_set_size(cancelBtn, 100, STATUS_BAR_H);
  lv_obj_set_style_bg_color(cancelBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(cancelBtn, 0, 0);
  lv_obj_add_event_cb(cancelBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* cancelLabel = lv_label_create(cancelBtn);
  lv_label_set_text(cancelLabel, "✗ CANCEL");
  lv_obj_set_style_text_color(cancelLabel, COL_ACCENT, 0);
  lv_obj_center(cancelLabel);

  lv_obj_t* title = lv_label_create(header);
  char titleBuf[32];
  snprintf(titleBuf, 32, "EDIT PROGRAM [Slot %d]", slot);
  lv_label_set_text(title, titleBuf);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

  // Name input
  lv_obj_t* namePanel = lv_obj_create(screen);
  lv_obj_set_size(namePanel, 456, 44);
  lv_obj_set_pos(namePanel, 12, 44);
  lv_obj_set_style_bg_color(namePanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(namePanel, RADIUS_CARD, 0);

  lv_obj_t* nameLabel = lv_label_create(namePanel);
  lv_label_set_text(nameLabel, "NAME");
  lv_obj_set_style_text_color(nameLabel, COL_TEXT_LABEL, 0);
  lv_obj_align(nameLabel, LV_ALIGN_LEFT_MID, 8, 0);

  lv_obj_t* nameInput = lv_textarea_create(screen);
  lv_obj_set_size(nameInput, 456, 44);
  lv_obj_set_pos(nameInput, 12, 92);
  lv_textarea_set_placeholder_text(nameInput, "Enter program name...");
  lv_textarea_set_max_length(nameInput, 20);
  lv_textarea_set_one_line(nameInput, true);

  // Mode buttons
  lv_obj_t* modeLabel = lv_label_create(screen);
  lv_label_set_text(modeLabel, "MODE");
  lv_obj_set_style_text_color(modeLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(modeLabel, 12, 150);

  // TODO: Add full form implementation

  // Back button
  lv_obj_t* backBtn = lv_btn_create(screen);
  lv_obj_set_size(backBtn, 456, 52);
  lv_obj_set_pos(backBtn, 12, 260);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_center(backLabel);

  LOG_I("Screen program edit: created slot %d", slot);
}
