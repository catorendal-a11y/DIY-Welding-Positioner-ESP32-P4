#pragma once
#include "lvgl.h"
#include "../storage/storage.h"

extern lv_obj_t* screenRoots[];

typedef enum {
  SCREEN_NONE = 0,
  SCREEN_BOOT,
  SCREEN_MAIN,
  SCREEN_MENU,
  SCREEN_PULSE,
  SCREEN_STEP,
  SCREEN_JOG,
  SCREEN_TIMER,
  SCREEN_PROGRAMS,
  SCREEN_PROGRAM_EDIT,
  SCREEN_SETTINGS,
  SCREEN_CONFIRM,
  SCREEN_EDIT_PULSE,
  SCREEN_EDIT_STEP,
  SCREEN_EDIT_TIMER
} ScreenId;

#define SCREEN_COUNT (SCREEN_EDIT_TIMER + 1)

void screens_init();
void screens_show(ScreenId id);
ScreenId screens_get_current();
bool screens_is_active(ScreenId id);
void screens_update_current();

void screen_boot_create();
void screen_main_create();
void screen_menu_create();
void screen_pulse_create();
void screen_step_create();
void screen_jog_create();
void screen_timer_create();
void screen_programs_create();
void screen_program_edit_create(int slot);
Preset* screen_program_edit_get_preset();
void screen_settings_create();
void screen_confirm_create_static();
void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)());
void screen_edit_pulse_create();
void screen_edit_step_create();
void screen_edit_timer_create();

void screen_boot_update();
void screen_main_update();
void screen_pulse_update();
void screen_step_update();
void screen_jog_update();
void screen_timer_update();
void screen_programs_update();

void estop_overlay_create();
void estop_overlay_show();
void estop_overlay_hide();
void estop_overlay_update();
bool estop_overlay_visible();

void screens_set_back_button(lv_obj_t* btn, ScreenId dest);

void screen_boot_set_progress(int percent, const char* message);
void screen_boot_increment(int delta);
