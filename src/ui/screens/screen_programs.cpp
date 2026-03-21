// TIG Rotator Controller - Programs List Screen (T-26)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

static void edit_event_cb(lv_event_t* e) {
  int slot = (int)(size_t)lv_event_get_user_data(e);
  screens_show(SCREEN_PROGRAM_EDIT);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_programs_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];

  // Header
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, STATUS_BAR_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(header, 0, 0);

  lv_obj_t* backBtn = lv_btn_create(header);
  lv_obj_set_size(backBtn, 80, STATUS_BAR_H);
  lv_obj_set_style_bg_color(backBtn, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "◄ BACK");
  lv_obj_set_style_text_color(backLabel, COL_ACCENT, 0);
  lv_obj_center(backLabel);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "PROGRAMS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // Programs list placeholder
  lv_obj_t* list = lv_list_create(screen);
  lv_obj_set_size(list, 456, 200);
  lv_obj_set_pos(list, 12, 44);
  lv_obj_set_style_bg_color(list, COL_BG_CARD, 0);

  // Add placeholder items
  for (int i = 1; i <= 8; i++) {
    char buf[32];
    snprintf(buf, 32, "Slot %d — Empty", i);
    lv_list_add_btn(list, LV_SYMBOL_SAVE, buf);
  }

  // Back to main button
  lv_obj_t* mainBtn = lv_btn_create(screen);
  lv_obj_set_size(mainBtn, 456, 44);
  lv_obj_set_pos(mainBtn, 12, 260);
  lv_obj_set_style_bg_color(mainBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(mainBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(mainBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* mainLabel = lv_label_create(mainBtn);
  lv_label_set_text(mainLabel, "◄ BACK TO MAIN");
  lv_obj_center(mainLabel);

  LOG_I("Screen programs: created");
}

void screen_programs_update() {
  // Update program list
}
