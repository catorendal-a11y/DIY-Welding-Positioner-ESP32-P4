// TIG Rotator Controller - Screen Management Implementation
// Handles navigation between all 12 screens

#include <Arduino.h>
#include "screens.h"
#include "theme.h"
#include "../config.h"
#include "freertos/task.h"

extern TaskHandle_t lvglHandle;

// ───────────────────────────────────────────────────────────────────────────────
// GLOBALS
// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static ScreenId currentScreen = SCREEN_NONE;
lv_obj_t* screenRoots[SCREEN_COUNT] = { nullptr };  // Extern for screen files
static bool screenCreated[SCREEN_COUNT] = {};
static int pendingEditSlot = -1;

static void create_screen(ScreenId id) {
  screenRoots[id] = lv_obj_create(nullptr);
  lv_obj_set_size(screenRoots[id], SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(screenRoots[id], COL_BG, 0);
  lv_obj_set_style_bg_opa(screenRoots[id], LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(screenRoots[id], 0, 0);
  lv_obj_set_style_pad_all(screenRoots[id], 0, 0);
  lv_obj_set_style_radius(screenRoots[id], 0, 0);
  lv_obj_remove_flag(screenRoots[id], LV_OBJ_FLAG_SCROLLABLE);

  switch (id) {
    case SCREEN_BOOT:        screen_boot_create(); break;
    case SCREEN_MAIN:        screen_main_create(); break;
    case SCREEN_MENU:        screen_menu_create(); break;
    case SCREEN_PULSE:       screen_pulse_create(); break;
    case SCREEN_STEP:        screen_step_create(); break;
    case SCREEN_JOG:         screen_jog_create(); break;
    case SCREEN_TIMER:       screen_timer_create(); break;
    case SCREEN_PROGRAMS:    screen_programs_create(); break;
    case SCREEN_SETTINGS:    screen_settings_create(); break;
    case SCREEN_WIFI:        screen_wifi_create(); break;
    case SCREEN_BT:          screen_bt_create(); break;
    case SCREEN_SYSINFO:     screen_sysinfo_create(); break;
    case SCREEN_CALIBRATION: screen_calibration_create(); break;
    case SCREEN_MOTOR_CONFIG: screen_motor_config_create(); break;
    case SCREEN_DISPLAY:     screen_display_create(); break;
    case SCREEN_ABOUT:       screen_about_create(); break;
    case SCREEN_EDIT_PULSE:  screen_edit_pulse_create(); break;
    case SCREEN_EDIT_STEP:   screen_edit_step_create(); break;
    case SCREEN_EDIT_TIMER:  screen_edit_timer_create(); break;
    case SCREEN_PROGRAM_EDIT: screen_program_edit_create(pendingEditSlot); pendingEditSlot = -1; break;
    case SCREEN_EDIT_CONT:   screen_edit_cont_create(); break;
    default: break;
  }

  screenCreated[id] = true;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void screens_init() {
  LOG_I("Screens init: creating boot and main screens");

  for (int i = 0; i < SCREEN_COUNT; i++) {
    screenCreated[i] = false;
  }

  create_screen(SCREEN_BOOT);
  create_screen(SCREEN_CONFIRM);
  screen_confirm_create_static();
  estop_overlay_create();
  create_screen(SCREEN_MAIN);

  LOG_I("Screens init complete");
}

void screens_reinit() {
  ScreenId prev = currentScreen;

  estop_overlay_destroy();

  for (int i = 0; i < SCREEN_COUNT; i++) {
    if (screenRoots[i]) {
      lv_obj_delete(screenRoots[i]);
      screenRoots[i] = nullptr;
    }
    screenCreated[i] = false;
  }

  currentScreen = SCREEN_NONE;
  screens_init();

  if (prev >= 0 && prev < SCREEN_COUNT) {
    screens_show(prev);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN NAVIGATION
// ───────────────────────────────────────────────────────────────────────────────

void screens_show(ScreenId id) {
  if (id < 0 || id >= SCREEN_COUNT) return;

  if (!screenCreated[id]) {
    create_screen(id);
  }
  if (screenRoots[id] == nullptr) return;

  currentScreen = id;
  lv_screen_load(screenRoots[id]);

  if (id == SCREEN_DISPLAY) {
    screen_display_mark_dirty();
  }
  if (id == SCREEN_PROGRAMS) {
    screen_programs_mark_dirty();
  }

  #if DEBUG_BUILD
  if (lvglHandle) {
    LOG_I("Screen %d stack free: %u bytes", id, uxTaskGetStackHighWaterMark(lvglHandle) * 4);
  }
  #endif

  LOG_D("Screen show: %d", id);
}

ScreenId screens_get_current() {
  return currentScreen;
}

bool screens_is_active(ScreenId id) {
  return (currentScreen == id);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE DISPATCHER
// ───────────────────────────────────────────────────────────────────────────────
void screens_update_current() {
  switch (currentScreen) {
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
    case SCREEN_EDIT_PULSE:
      screen_edit_pulse_update();
      break;
    case SCREEN_EDIT_STEP:
      screen_edit_step_update();
      break;
    case SCREEN_EDIT_TIMER:
      screen_edit_timer_update();
      break;
    case SCREEN_EDIT_CONT:
      screen_edit_cont_update();
      break;
    case SCREEN_WIFI:
      screen_wifi_update();
      break;
    case SCREEN_BT:
      screen_bt_update();
      break;
    case SCREEN_SYSINFO:
      screen_sysinfo_update();
      break;
    case SCREEN_CALIBRATION:
      screen_calibration_update();
      break;
    case SCREEN_MOTOR_CONFIG:
      screen_motor_config_update();
      break;
    case SCREEN_DISPLAY:
      screen_display_update();
      break;
    case SCREEN_ABOUT:
      screen_about_update();
      break;
    case SCREEN_CONFIRM:
      screen_confirm_update();
      break;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// BACK BUTTON HELPER
// ───────────────────────────────────────────────────────────────────────────────
void screens_set_back_button(lv_obj_t* btn, ScreenId dest) {
  lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    ScreenId dest = (ScreenId)(size_t)lv_event_get_user_data(e);
    screens_show(dest);
  }, LV_EVENT_CLICKED, (void*)(size_t)dest);
}

void screens_set_edit_slot(int slot) {
  pendingEditSlot = slot;
}
