// TIG Rotator Controller - Screen Management
// All 12 screens for the TIG rotator controller UI

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// GLOBAL SCREEN ROOTS ARRAY
// ───────────────────────────────────────────────────────────────────────────────
extern lv_obj_t* screenRoots[];

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN ENUM
// ───────────────────────────────────────────────────────────────────────────────
typedef enum {
  SCREEN_NONE = 0,
  SCREEN_MAIN,           // Main screen - RPM gauge, start/stop
  SCREEN_MENU,           // Advanced menu
  SCREEN_PULSE,          // Pulse mode setup
  SCREEN_STEP,           // Step mode setup
  SCREEN_JOG,            // Jog mode
  SCREEN_TIMER,          // Timer mode setup
  SCREEN_PROGRAMS,       // Programs list
  SCREEN_PROGRAM_EDIT,   // Program edit
  SCREEN_SETTINGS,       // Settings
  SCREEN_CONFIRM         // Confirmation dialog
} ScreenId;

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN MANAGEMENT
// ───────────────────────────────────────────────────────────────────────────────
void screens_init();                    // Initialize all screens
void screens_show(ScreenId id);         // Show specific screen
ScreenId screens_get_current();         // Get current screen
bool screens_is_active(ScreenId id);    // Check if screen is active
void screens_update_current();          // Update current screen (call from lvglTask)

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATION FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create();
void screen_menu_create();
void screen_pulse_create();
void screen_step_create();
void screen_jog_create();
void screen_timer_create();
void screen_programs_create();
void screen_program_edit_create(int slot);
void screen_settings_create();
void screen_confirm_create_static();  // Static init
void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)());

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_update();
void screen_pulse_update();
void screen_step_update();
void screen_timer_update();
void screen_programs_update();

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP OVERLAY
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_create();   // Create overlay (initially hidden)
void estop_overlay_show();     // Show overlay
void estop_overlay_hide();     // Hide overlay
void estop_overlay_update();   // Update overlay state
bool estop_overlay_visible();  // Check if overlay is visible

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
void screens_set_back_button(lv_obj_t* btn, ScreenId dest);  // Configure back button
