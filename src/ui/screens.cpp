#include <Arduino.h>
#include "screens.h"
#include "theme.h"
#include "../config.h"

static ScreenId currentScreen = SCREEN_NONE;
lv_obj_t* screenRoots[SCREEN_COUNT] = { nullptr };

void screens_init() {
  LOG_I("Screens init: creating all screens");

  for (int i = 0; i < SCREEN_COUNT; i++) {
    screenRoots[i] = lv_obj_create(nullptr);
    lv_obj_set_size(screenRoots[i], SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(screenRoots[i], COL_BG, 0);
    lv_obj_set_style_bg_opa(screenRoots[i], LV_OPA_COVER, 0);
  }

  screen_boot_create();
  screen_main_create();
  screen_menu_create();
  screen_pulse_create();
  screen_step_create();
  screen_jog_create();
  screen_timer_create();
  screen_programs_create();
  screen_settings_create();
  screen_confirm_create_static();
  estop_overlay_create();

  LOG_I("Screens init complete");
}

void screens_show(ScreenId id) {
  if (id < 0 || id >= SCREEN_COUNT) return;
  if (screenRoots[id] == nullptr) return;

  currentScreen = id;
  lv_scr_load(screenRoots[id]);
  LOG_D("Screen show: %d", id);
}

ScreenId screens_get_current() {
  return currentScreen;
}

bool screens_is_active(ScreenId id) {
  return (currentScreen == id);
}

void screens_update_current() {
  switch (currentScreen) {
    case SCREEN_BOOT:
      screen_boot_update();
      break;
    case SCREEN_MAIN:
      screen_main_update();
      break;
    case SCREEN_PULSE:
      screen_pulse_update();
      break;
    case SCREEN_STEP:
      screen_step_update();
      break;
    case SCREEN_JOG:
      screen_jog_update();
      break;
    case SCREEN_TIMER:
      screen_timer_update();
      break;
    case SCREEN_PROGRAMS:
      screen_programs_update();
      break;
    default:
      break;
  }
}

void screens_set_back_button(lv_obj_t* btn, ScreenId dest) {
  lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    ScreenId dest = (ScreenId)(intptr_t)lv_event_get_user_data(e);
    screens_show(dest);
  }, LV_EVENT_CLICKED, (void*)(intptr_t)dest);
}
