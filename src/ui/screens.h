// TIG Rotator Controller - Screen Management
// All 12 screens for the TIG rotator controller UI

#pragma once
#include "lvgl.h"
#include "../storage/storage.h"  // For Preset type
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// GLOBAL SCREEN ROOTS ARRAY
// ───────────────────────────────────────────────────────────────────────────────
extern lv_obj_t* screenRoots[];

extern std::atomic<bool> motorConfigApplyPending;

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
  SCREEN_CONFIRM,        // Confirmation dialog
  SCREEN_BOOT,           // Boot screen
  SCREEN_EDIT_PULSE,     // Pulse edit (preset quick edit)
  SCREEN_EDIT_STEP,      // Step edit (preset quick edit)
  SCREEN_EDIT_TIMER,     // Timer edit (preset quick edit)
  SCREEN_EDIT_CONT,      // Continuous edit (preset quick edit)
  SCREEN_WIFI,         // WiFi + BLE connectivity settings
  SCREEN_BT,           // Bluetooth settings
  SCREEN_SYSINFO,      // System info
  SCREEN_CALIBRATION,   // Motor calibration
  SCREEN_MOTOR_CONFIG,  // Motor configuration
  SCREEN_DISPLAY,        // Display settings
  SCREEN_ABOUT,          // About screen
  SCREEN_COUNT           // MUST be last — total number of screens
} ScreenId;

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN MANAGEMENT
// ───────────────────────────────────────────────────────────────────────────────
void screens_init();                    // Initialize all screens
void screens_reinit();                  // Destroy and recreate all screens (theme change)
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
void screen_boot_create();
void screen_confirm_create_static();  // Static init
void screen_confirm_create(const char* title, const char* message,
                           void (*on_confirm)(), void (*on_cancel)());

// Boot screen
void screen_boot_update(int percent, const char* status);

// Jog screen update
void screen_jog_update();

// ───────────────────────────────────────────────────────────────────────────────
// EDIT SCREENS (mode-specific preset editing)
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_pulse_create();
void screen_edit_step_create();
void screen_edit_timer_create();
void screen_edit_cont_create();

void screen_wifi_create();
void screen_wifi_update();
void screen_bt_create();
void screen_bt_update();
void screen_sysinfo_create();
void screen_sysinfo_update();
void screen_calibration_create();
void screen_calibration_update();
void screen_motor_config_create();
void screen_motor_config_update();
void screen_display_create();
void screen_display_update();
void screen_display_mark_dirty();
void screen_about_create();
void screen_about_update();

// Update functions (called when returning to these screens)
void screen_edit_pulse_update();
void screen_edit_step_update();
void screen_edit_timer_update();
void screen_edit_cont_update();

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_update();
void screen_pulse_update();
void screen_step_update();
void screen_timer_update();
void screen_programs_update();
void screen_programs_mark_dirty();

// ───────────────────────────────────────────────────────────────────────────────
// ESTOP OVERLAY
// ───────────────────────────────────────────────────────────────────────────────
void estop_overlay_create();   // Create overlay (initially hidden)
void estop_overlay_destroy();  // Destroy overlay (for theme reinit)
void estop_overlay_show();     // Show overlay
void estop_overlay_hide();     // Hide overlay
void estop_overlay_update();   // Update overlay state
bool estop_overlay_visible();  // Check if overlay is visible

// ───────────────────────────────────────────────────────────────────────────────
// HELPERS
// ───────────────────────────────────────────────────────────────────────────────
void screens_set_back_button(lv_obj_t* btn, ScreenId dest);  // Configure back button
void screens_set_edit_slot(int slot);  // Set slot for SCREEN_PROGRAM_EDIT creation
Preset* screen_program_edit_get_preset();  // Get current preset being edited
void screen_program_edit_update_ui();  // Update UI with current preset values
