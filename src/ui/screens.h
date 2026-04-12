// TIG Rotator Controller - Screen Management
// All 12 screens for the TIG rotator controller UI

#pragma once
#include "lvgl.h"
#include "../storage/storage.h"  // For Preset type
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t g_lvgl_mutex;
void lvgl_lock();
void lvgl_unlock();

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
  SCREEN_EDIT_CONT,      // Continuous edit (preset quick edit)
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
void screens_show(ScreenId id);         // Show specific screen (safe from callbacks)
void screens_request_show(ScreenId id); // Deferred show — processed before lv_timer_handler
void screens_request_theme_reinit();    // Deferred theme reinit — processed before lv_timer_handler
void screens_process_pending();         // Execute deferred screen switch (call BEFORE lv_timer_handler)
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
void screen_confirm_update();

// Boot screen
void screen_boot_update(int percent, const char* status);

// Jog screen update
void screen_jog_update();

// ───────────────────────────────────────────────────────────────────────────────
// EDIT SCREENS (mode-specific preset editing)
// ───────────────────────────────────────────────────────────────────────────────
void screen_edit_pulse_create();
void screen_edit_step_create();
void screen_edit_cont_create();

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
void screen_edit_cont_update();

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE FUNCTIONS
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_update();
// After loading a pulse program preset, keep main-screen PULSE quick times in sync.
void screen_main_set_program_pulse_times(uint32_t on_ms, uint32_t off_ms);
void screen_pulse_update();
void screen_step_update();
void screen_timer_update();
void screen_programs_update();
void screen_programs_mark_dirty();
void screen_programs_invalidate_widgets();
void screen_program_edit_invalidate_widgets();
void screen_step_invalidate_widgets();
void screen_main_invalidate_widgets();
void screen_pulse_invalidate_widgets();
void screen_timer_invalidate_widgets();
void screen_jog_invalidate_widgets();
void screen_display_invalidate_widgets();
void screen_sysinfo_invalidate_widgets();
void screen_motor_config_invalidate_widgets();
void screen_calibration_invalidate_widgets();
void screen_edit_cont_invalidate_widgets();
void screen_edit_pulse_invalidate_widgets();
void screen_edit_step_invalidate_widgets();

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
// HELPERS — shared widgets (lvglTask only)
// ───────────────────────────────────────────────────────────────────────────────
typedef enum {
  UI_BTN_NORMAL = 0,
  UI_BTN_ACCENT,
  UI_BTN_DANGER
} UiBtnStyle;

lv_obj_t* ui_create_header(lv_obj_t* parent, const char* title);
lv_obj_t* ui_create_settings_header(lv_obj_t* parent, const char* title);
lv_obj_t* ui_create_separator(lv_obj_t* parent, lv_coord_t y);
lv_obj_t* ui_create_separator_line(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_color_t color);
lv_obj_t* ui_create_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        const char* text, const lv_font_t* label_font, UiBtnStyle style,
                        lv_event_cb_t cb, void* user_data);
lv_obj_t* ui_create_pm_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, const char* text,
                           const lv_font_t* label_font, lv_event_cb_t cb, void* user_data);
void ui_create_action_bar(lv_obj_t* parent, lv_coord_t pad_x, lv_coord_t footer_y, lv_coord_t footer_h,
                          lv_coord_t gap, lv_coord_t left_w, lv_coord_t right_w,
                          const char* left_text, lv_event_cb_t left_cb,
                          const char* right_text, UiBtnStyle right_style, lv_event_cb_t right_cb);
void ui_create_action_bar_three(lv_obj_t* parent, lv_coord_t pad_x, lv_coord_t y, lv_coord_t h, lv_coord_t gap,
                                lv_coord_t btn_w,
                                const char* left_text, lv_event_cb_t left_cb, UiBtnStyle left_style,
                                const char* mid_text, lv_event_cb_t mid_cb, UiBtnStyle mid_style,
                                const char* right_text, lv_event_cb_t right_cb, UiBtnStyle right_style,
                                lv_obj_t** out_left_btn, lv_obj_t** out_mid_btn, lv_obj_t** out_right_btn);

void screens_set_back_button(lv_obj_t* btn, ScreenId dest);  // Configure back button
void screens_set_edit_slot(int slot);  // Set slot for SCREEN_PROGRAM_EDIT creation
Preset* screen_program_edit_get_preset();  // Get current preset being edited
void screen_program_edit_update_ui();  // Update UI with current preset values
void screen_program_edit_poll_keyboard(); // Deferred keyboard close (call each LVGL tick while on edit screen)
