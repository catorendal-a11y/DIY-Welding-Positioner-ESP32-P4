// TIG Rotator Controller - Settings Screen (T-27)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }

static void reset_event_cb(lv_event_t* e) {
  // Factory reset - confirm dialog
  // TODO: Implement confirmation
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];

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
  lv_label_set_text(title, "⚙ SETTINGS");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // Settings sliders
  const char* settings[] = {"MAX RPM", "JOG SPEED", "ACCELERATION"};
  const float values[] = {MAX_RPM, 0.5f, 5000.0f};
  const int yPos[] = {50, 98, 146};

  for (int i = 0; i < 3; i++) {
    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, settings[i]);
    lv_obj_set_style_text_color(label, COL_TEXT_LABEL, 0);
    lv_obj_set_pos(label, 12, yPos[i]);

    lv_obj_t* slider = lv_slider_create(screen);
    lv_obj_set_size(slider, 300, 20);
    lv_obj_set_pos(slider, 100, yPos[i] + 2);

    lv_obj_t* valueLabel = lv_label_create(screen);
    char buf[16];
    snprintf(buf, 16, "%.1f", values[i]);
    lv_label_set_text(valueLabel, buf);
    lv_obj_align(valueLabel, LV_ALIGN_RIGHT_MID, -12, yPos[i] + 12);

    // Warning label under MAX RPM slider (i=0)
    if (i == 0) {
      lv_obj_t* rpmWarnLabel = lv_label_create(screen);
      lv_label_set_text(rpmWarnLabel, "⚠ Above 3.0 RPM: verify no step jitter under TIG arc start");
      lv_obj_set_style_text_color(rpmWarnLabel, COL_AMBER, 0);
      lv_obj_set_style_text_font(rpmWarnLabel, &lv_font_montserrat_14, 0);
      lv_obj_set_pos(rpmWarnLabel, 12, yPos[i] + 28);
    }
  }

  // Microstep selection
  lv_obj_t* microLabel = lv_label_create(screen);
  lv_label_set_text(microLabel, "MICROSTEP");
  lv_obj_set_style_text_color(microLabel, COL_TEXT_LABEL, 0);
  lv_obj_set_pos(microLabel, 12, 194);

  const char* microOptions[] = {"1/2", "1/4", "1/8", "1/16", "1/32"};
  for (int i = 0; i < 5; i++) {
    lv_obj_t* btn = lv_btn_create(screen);
    lv_obj_set_size(btn, 80, 36);
    lv_obj_set_pos(btn, 12 + i * 92, 218);
    lv_obj_set_style_radius(btn, 6, 0);

    if (i == 2) {  // 1/8 is default
      lv_obj_set_style_bg_color(btn, COL_ACCENT, 0);
    } else {
      lv_obj_set_style_bg_color(btn, COL_BTN_NORMAL, 0);
    }

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, microOptions[i]);
    lv_obj_center(label);
  }

  // Warning
  lv_obj_t* warnLabel = lv_label_create(screen);
  lv_label_set_text(warnLabel, "⚠ Change requires restart. Update TB6600 DIP switches too.");
  lv_obj_set_style_text_color(warnLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(warnLabel, 12, 258);

  // System info panel
  lv_obj_t* infoPanel = lv_obj_create(screen);
  lv_obj_set_size(infoPanel, 456, 44);
  lv_obj_set_pos(infoPanel, 12, 276);  // Will be moved if needed
  lv_obj_set_style_bg_color(infoPanel, COL_BG_CARD, 0);
  lv_obj_set_style_radius(infoPanel, RADIUS_CARD, 0);

  lv_obj_t* infoLabel = lv_label_create(infoPanel);
  lv_label_set_text(infoLabel, "Firmware v1.0   Flash 16MB   PSRAM 2MB");
  lv_obj_set_style_text_color(infoLabel, COL_TEXT_DIM, 0);
  lv_obj_center(infoLabel);

  LOG_I("Screen settings: created");
}
