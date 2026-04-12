// TIG Rotator Controller - Screen Management Implementation
// Handles navigation between all 12 screens

#include <Arduino.h>
#include "screens.h"
#include "theme.h"
#include "../config.h"
#include "../motor/speed.h"
#include "freertos/task.h"

// lvglHandle defined in main.cpp — used for DEBUG stack watermark logging
extern TaskHandle_t lvglHandle;

SemaphoreHandle_t g_lvgl_mutex = nullptr;

void lvgl_lock() {
  if (g_lvgl_mutex) xSemaphoreTake(g_lvgl_mutex, portMAX_DELAY);
}

void lvgl_unlock() {
  if (g_lvgl_mutex) xSemaphoreGive(g_lvgl_mutex);
}

// ───────────────────────────────────────────────────────────────────────────────
// GLOBALS
// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static ScreenId currentScreen = SCREEN_NONE;
static volatile ScreenId pendingScreen = SCREEN_NONE;
static volatile bool themeReinitPending = false;
lv_obj_t* screenRoots[SCREEN_COUNT] = { nullptr };  // Extern for screen files
static bool screenCreated[SCREEN_COUNT] = {};
static int pendingEditSlot = -1;

static bool screen_needs_rebuild(ScreenId id) {
  return id == SCREEN_PROGRAM_EDIT || id == SCREEN_EDIT_CONT
      || id == SCREEN_EDIT_PULSE  || id == SCREEN_EDIT_STEP
      || id == SCREEN_STEP;
}

static void create_screen(ScreenId id) {
  if (!screenRoots[id]) {
    screenRoots[id] = lv_obj_create(nullptr);
    lv_obj_set_size(screenRoots[id], SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(screenRoots[id], COL_BG, 0);
    lv_obj_set_style_bg_opa(screenRoots[id], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screenRoots[id], 0, 0);
    lv_obj_set_style_pad_all(screenRoots[id], 0, 0);
    lv_obj_set_style_radius(screenRoots[id], 0, 0);
    lv_obj_remove_flag(screenRoots[id], LV_OBJ_FLAG_SCROLLABLE);
  }

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
    case SCREEN_SYSINFO:     screen_sysinfo_create(); break;
    case SCREEN_CALIBRATION: screen_calibration_create(); break;
    case SCREEN_MOTOR_CONFIG: screen_motor_config_create(); break;
    case SCREEN_DISPLAY:     screen_display_create(); break;
    case SCREEN_ABOUT:       screen_about_create(); break;
    case SCREEN_EDIT_PULSE:  screen_edit_pulse_create(); break;
    case SCREEN_EDIT_STEP:   screen_edit_step_create(); break;
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
  if (!g_lvgl_mutex) {
    g_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (!g_lvgl_mutex) {
      LOG_E("Screens init: failed to create LVGL mutex");
      return;
    }
  }
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

  screen_main_invalidate_widgets();
  screen_pulse_invalidate_widgets();
  screen_timer_invalidate_widgets();
  screen_jog_invalidate_widgets();
  screen_programs_invalidate_widgets();
  screen_program_edit_invalidate_widgets();
  screen_step_invalidate_widgets();
  screen_display_invalidate_widgets();
  screen_sysinfo_invalidate_widgets();
  screen_motor_config_invalidate_widgets();
  screen_calibration_invalidate_widgets();
  screen_edit_cont_invalidate_widgets();
  screen_edit_pulse_invalidate_widgets();
  screen_edit_step_invalidate_widgets();

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

  ScreenId prev = currentScreen;
  if (prev == SCREEN_STEP && id != SCREEN_STEP) {
    speed_set_slider_priority(false);
  }
  if (id == SCREEN_STEP) {
    speed_set_slider_priority(true);  // before create_screen: avoid pot clobber during build
  }

  if (screen_needs_rebuild(id)) {
    screenCreated[id] = false;
  }

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

void screens_request_show(ScreenId id) {
  pendingScreen = id;
}

void screens_request_theme_reinit() {
  themeReinitPending = true;
}

void screens_process_pending() {
  if (themeReinitPending) {
    themeReinitPending = false;
    theme_refresh();
    return;
  }
  if (pendingScreen != SCREEN_NONE) {
    ScreenId id = pendingScreen;
    pendingScreen = SCREEN_NONE;
    screens_show(id);
  }
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
    case SCREEN_PROGRAM_EDIT:
      screen_program_edit_update_ui();
      break;
    case SCREEN_EDIT_PULSE:
      screen_edit_pulse_update();
      break;
    case SCREEN_EDIT_STEP:
      screen_edit_step_update();
      break;
    case SCREEN_EDIT_CONT:
      screen_edit_cont_update();
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
    case SCREEN_NONE:
    case SCREEN_MENU:
    case SCREEN_SETTINGS:
    case SCREEN_BOOT:
    case SCREEN_COUNT:
      break;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SHARED UI WIDGETS (lvglTask only)
// ───────────────────────────────────────────────────────────────────────────────
lv_obj_t* ui_create_header(lv_obj_t* parent, const char* title,
                           lv_coord_t header_h, const lv_font_t* title_font, lv_coord_t title_y) {
  lv_obj_t* header = lv_obj_create(parent);
  lv_obj_set_size(header, SCREEN_W, header_h);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* title_lbl = lv_label_create(header);
  lv_label_set_text(title_lbl, title);
  lv_obj_set_style_text_font(title_lbl, title_font, 0);
  lv_obj_set_style_text_color(title_lbl, COL_ACCENT, 0);
  lv_obj_set_pos(title_lbl, PAD_X, title_y);
  return header;
}

lv_obj_t* ui_create_separator(lv_obj_t* parent, lv_coord_t y) {
  lv_obj_t* line = lv_obj_create(parent);
  lv_obj_set_size(line, SCREEN_W, 1);
  lv_obj_set_pos(line, 0, y);
  lv_obj_set_style_bg_color(line, COL_BORDER, 0);
  lv_obj_set_style_pad_all(line, 0, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_radius(line, 0, 0);
  lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
  return line;
}

lv_obj_t* ui_create_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        const char* text, const lv_font_t* label_font, bool accent, bool danger,
                        lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_color_t bg = danger ? COL_BTN_DANGER : (accent ? COL_BG_ACTIVE : COL_BTN_BG);
  lv_color_t bor = danger ? COL_BORDER_DNG : (accent ? COL_ACCENT : COL_BORDER);
  lv_coord_t bw = (accent || danger) ? 2 : 1;
  lv_color_t tc = danger ? COL_RED : (accent ? COL_ACCENT : COL_TEXT);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, bw, 0);
  lv_obj_set_style_border_color(btn, bor, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  if (cb) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  }
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, label_font, 0);
  lv_obj_set_style_text_color(lbl, tc, 0);
  lv_obj_center(lbl);
  return btn;
}

lv_obj_t* ui_create_pm_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, const char* text,
                           const lv_font_t* label_font, lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, BTN_W_PM, BTN_H_PM);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER_SM, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  if (cb) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  }
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, label_font, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

void ui_create_action_bar(lv_obj_t* parent, lv_coord_t pad_x, lv_coord_t footer_y, lv_coord_t footer_h,
                          lv_coord_t gap, lv_coord_t left_w, lv_coord_t right_w,
                          const char* left_text, lv_event_cb_t left_cb,
                          const char* right_text, bool right_accent, lv_event_cb_t right_cb) {
  ui_create_btn(parent, pad_x, footer_y, left_w, footer_h, left_text, FONT_SUBTITLE, false, false, left_cb, nullptr);
  ui_create_btn(parent, pad_x + left_w + gap, footer_y, right_w, footer_h, right_text, FONT_SUBTITLE, right_accent, false, right_cb, nullptr);
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
