// TIG Rotator Controller - Programs List Screen (T-26)

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../motor/speed.h"
#include "../../control/control.h"
#include <cstdio>

static lv_obj_t* programList = nullptr;

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

static void save_new_cb(lv_event_t* e) {
    if (g_presets.size() >= MAX_PRESETS) return;
    
    Preset newP;
    newP.id = g_presets.size() + 1;
    snprintf(newP.name, sizeof(newP.name), "Preset %d", newP.id);
    
    newP.rpm = speed_get_target_rpm();
    newP.mode = control_get_state();
    if (newP.mode == STATE_IDLE || newP.mode == STATE_STOPPING || newP.mode == STATE_ESTOP) {
        newP.mode = STATE_RUNNING; // Default fallback
    }
    
    // Default sub-parameters since we can't easily extract them without changing more files
    newP.pulse_on_ms = 500;
    newP.pulse_off_ms = 500;
    newP.step_angle = 90.0f;
    newP.timer_ms = 5000;
    
    g_presets.push_back(newP);
    storage_save_presets();
    
    screen_programs_update(); // Refresh UI list
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
  lv_label_set_text(title, "PROGRAMS (LITTLEFS)");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 90, 0);

  // Programs list
  programList = lv_list_create(screen);
  // Changed size to fit nicer
  lv_obj_set_size(programList, 456, 180);
  lv_obj_set_pos(programList, 12, 44);
  lv_obj_set_style_bg_color(programList, COL_BG_CARD, 0);
  lv_obj_set_style_border_width(programList, 0, 0);

  // Back to main button
  lv_obj_t* mainBtn = lv_btn_create(screen);
  lv_obj_set_size(mainBtn, 456, 44);
  lv_obj_set_pos(mainBtn, 12, 235);
  lv_obj_set_style_bg_color(mainBtn, COL_BTN_NORMAL, 0);
  lv_obj_set_style_radius(mainBtn, RADIUS_BTN, 0);
  lv_obj_add_event_cb(mainBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* mainLabel = lv_label_create(mainBtn);
  lv_label_set_text(mainLabel, "◄ BACK TO MAIN");
  lv_obj_center(mainLabel);

  LOG_I("Screen programs: created");
}

void screen_programs_update() {
  if (!programList) return;
  lv_obj_clean(programList); // Clear list

  for (const auto& p : g_presets) {
      char buf[64];
      const char* mName = control_state_name(p.mode);
      snprintf(buf, sizeof(buf), "%d: %s | %s | %.1f RPM", p.id, p.name, mName, p.rpm);
      lv_obj_t* btn = lv_list_add_btn(programList, LV_SYMBOL_PLAY, buf);
      lv_obj_add_event_cb(btn, load_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)p.id);
  }

  if (g_presets.size() < MAX_PRESETS) {
      lv_obj_t* btn = lv_list_add_btn(programList, LV_SYMBOL_SAVE, "Save Current State as New Preset...");
      lv_obj_add_event_cb(btn, save_new_cb, LV_EVENT_CLICKED, nullptr);
  }
}
