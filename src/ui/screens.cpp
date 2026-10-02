// TIG Rotator Controller - Screen Management Implementation
// Handles navigation between registered LVGL screen roots

#include <Arduino.h>
#include "screens.h"
#include "theme.h"
#include "../config.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include <cstring>

static ControlSnapshot uiSnapshot;
static bool uiSnapshotValid = false;
void ui_control_refresh() { ControlSnapshot next; if (control_read_snapshot(next)) { uiSnapshot = next; uiSnapshotValid = true; } }
const ControlSnapshot& ui_control_view() { return uiSnapshot; }
bool ui_control_fresh() { return control_timestamp_fresh(millis(), uiSnapshot.timestamp_ms, uiSnapshotValid); }
SystemState ui_control_state() { return safety_is_estop_locked() ? STATE_ESTOP : uiSnapshot.state; }
#include "freertos/task.h"

// lvglHandle defined in main.cpp — used for DEBUG stack watermark logging
extern TaskHandle_t lvglHandle;

SemaphoreHandle_t g_lvgl_mutex = nullptr;

lv_obj_t* ui_create_text(lv_obj_t* parent, int x, int y, int width, const char* text, const lv_font_t* font,
                         lv_color_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  return label;
}

lv_obj_t* ui_create_adjust_card(lv_obj_t* parent, int x, int y, int width, const char* title,
                                lv_event_cb_t callback, int step) {
  lv_obj_t* card = ui_create_post_card(parent, x, y, width, 124);
  ui_create_text(card, 14, 8, width - 28, title, FONT_NORMAL, COL_TEXT_DIM);
  lv_obj_t* value = ui_create_text(card, 14, 30, width - 28, "", FONT_XL, COL_TEXT);
  ui_create_btn(card, 14, 68, (width - 40) / 2, 48, "-", FONT_XL, UI_BTN_NORMAL, callback,
                (void*)(intptr_t)-step);
  ui_create_btn(card, 26 + (width - 40) / 2, 68, (width - 40) / 2, 48, "+", FONT_XL, UI_BTN_NORMAL, callback,
                (void*)(intptr_t)step);
  return value;
}

void ui_highlight_value_card(lv_obj_t* value) {
  lv_obj_t* card = lv_obj_get_parent(value);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xFF6B38), 0);
  lv_obj_set_style_border_width(card, 0, 0);
  for (uint32_t i = 0; i < lv_obj_get_child_count(card); i++) {
    lv_obj_t* child = lv_obj_get_child(card, i);
    if (lv_obj_check_type(child, &lv_label_class))
      lv_obj_set_style_text_color(child, lv_color_hex(0x11191C), 0);
  }
}

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
static lv_obj_t* staleBanner = nullptr;
static bool movement_button(lv_obj_t* obj) {
  if (!lv_obj_check_type(obj, &lv_button_class)) return false;
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) {
    auto label = lv_obj_get_child(obj, i);
    if (!lv_obj_check_type(label, &lv_label_class)) continue;
    const char* text = lv_label_get_text(label);
    if (strstr(text, "START") || strstr(text, "HOLD CW") || strstr(text, "HOLD CCW") ||
        strcmp(text, "MOVE 360") == 0 || strcmp(text, "> STEP") == 0 ||
        strcmp(text, "JOG -") == 0 || strcmp(text, "JOG +") == 0) return true;
  }
  return false;
}
static void stale_controls(lv_obj_t* obj, bool disable) {
  if (!obj) return;
  if (!disable && lv_obj_has_state(obj, LV_STATE_USER_2)) {
    lv_obj_set_state_user_2(obj, false); lv_obj_remove_state(obj, LV_STATE_DISABLED);
  }
  if (disable && movement_button(obj) && !lv_obj_has_state(obj, LV_STATE_DISABLED)) {
    lv_obj_set_state_user_2(obj, true); lv_obj_add_state(obj, LV_STATE_DISABLED);
  }
  for (uint32_t i=0; i<lv_obj_get_child_count(obj); ++i) stale_controls(lv_obj_get_child(obj, i), disable);
}
static ScreenId currentScreen = SCREEN_NONE;
static ScreenId pendingScreen = SCREEN_NONE;
static bool themeReinitPending = false;
lv_obj_t* screenRoots[SCREEN_COUNT] = {nullptr};  // Extern for screen files
static bool screenCreated[SCREEN_COUNT] = {};
static int pendingEditSlot = -2;

static bool screen_needs_rebuild(ScreenId id) {
  return id == SCREEN_SETUP || id == SCREEN_PROGRAM_EDIT || id == SCREEN_EDIT_CONT || id == SCREEN_EDIT_PULSE ||
         id == SCREEN_EDIT_STEP || id == SCREEN_STEP;
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
    lv_obj_set_scrollable(screenRoots[id], false);
  }

  switch (id) {
    case SCREEN_BOOT:
      screen_boot_create();
      break;
    case SCREEN_SETUP:
      screen_setup_create();
      break;
    case SCREEN_MAIN:
      screen_main_create();
      break;
    case SCREEN_MENU:
      screen_menu_create();
      break;
    case SCREEN_RUN_MODES:
      screen_run_modes_create();
      break;
    case SCREEN_PULSE:
      screen_pulse_create();
      break;
    case SCREEN_STEP:
      screen_step_create();
      break;
    case SCREEN_JOG:
      screen_jog_create();
      break;
    case SCREEN_TIMER:
      screen_timer_create();
      break;
    case SCREEN_PROGRAMS:
      screen_programs_create();
      break;
    case SCREEN_SETTINGS:
      screen_settings_create();
      break;
    case SCREEN_SYSINFO:
      screen_sysinfo_create();
      break;
    case SCREEN_CALIBRATION:
      screen_calibration_create();
      break;
    case SCREEN_MOTOR_CONFIG:
      screen_motor_config_create();
      break;
    case SCREEN_DISPLAY:
      screen_display_create();
      break;
    case SCREEN_PEDAL_SETTINGS:
      screen_pedal_settings_create();
      break;
    case SCREEN_DIAGNOSTICS:
      screen_diagnostics_create();
      break;
    case SCREEN_ABOUT:
      screen_about_create();
      break;
    case SCREEN_EDIT_PULSE:
      screen_edit_pulse_create();
      break;
    case SCREEN_EDIT_STEP:
      screen_edit_step_create();
      break;
    case SCREEN_PROGRAM_EDIT:
      screen_program_edit_create(pendingEditSlot);
      pendingEditSlot = -2;
      break;
    case SCREEN_EDIT_CONT:
      screen_edit_cont_create();
      break;
    default:
      break;
  }

  screenCreated[id] = true;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void screens_init() {
  if (!g_lvgl_mutex) {
    g_lvgl_mutex = xSemaphoreCreateMutex();
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
  staleBanner = ui_create_post_card(lv_layer_top(), 24, 94, 752, 54);
  lv_obj_set_style_bg_color(staleBanner, COL_RED, 0);
  ui_create_text(staleBanner, 16, 15, 720, "STATUS UNAVAILABLE / STOP remains available", FONT_NORMAL, lv_color_hex(0xFFFFFF));
  lv_obj_set_hidden(staleBanner, true);
  estop_overlay_create();
  create_screen(SCREEN_MAIN);

  LOG_I("Screens init complete");
}

void screens_reinit() {
  ScreenId prev = currentScreen;

  screen_setup_invalidate_widgets();
  screen_main_invalidate_widgets();
  screen_pulse_invalidate_widgets();
  screen_timer_invalidate_widgets();
  screen_jog_invalidate_widgets();
  screen_programs_invalidate_widgets();
  screen_program_edit_invalidate_widgets();
  screen_step_invalidate_widgets();
  screen_display_invalidate_widgets();
  screen_pedal_settings_invalidate_widgets();
  screen_diagnostics_invalidate_widgets();
  screen_sysinfo_invalidate_widgets();
  screen_motor_config_invalidate_widgets();
  screen_calibration_invalidate_widgets();
  screen_edit_cont_invalidate_widgets();
  screen_edit_pulse_invalidate_widgets();
  screen_edit_step_invalidate_widgets();

  if (staleBanner) { lv_obj_delete(staleBanner); staleBanner = nullptr; }
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

void screens_show_startup() {
  bool configured;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); configured = g_settings.setup_completed;
  xSemaphoreGive(g_settings_mutex);
  if (configured) screens_show(SCREEN_MAIN);
  else { screen_setup_begin(); screens_show(SCREEN_SETUP); }
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN NAVIGATION
// ───────────────────────────────────────────────────────────────────────────────

void screens_show(ScreenId id) {
  if (id < 0 || id >= SCREEN_COUNT) return;

  ScreenId prev = currentScreen;
  screen_setup_leave(id);
  const bool leavingSliderPriorityScreen =
      (prev == SCREEN_STEP || prev == SCREEN_CALIBRATION) && (id != SCREEN_STEP && id != SCREEN_CALIBRATION);
  if (leavingSliderPriorityScreen) {
    speed_set_slider_priority(false);
  }
  if (id == SCREEN_STEP || id == SCREEN_CALIBRATION) {
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
    LOG_I("Screen %d stack free: %u bytes", id, uxTaskGetStackHighWaterMark(lvglHandle));
  }
#endif

  LOG_D("Screen show: %d", id);
}

void screens_request_show(ScreenId id) { pendingScreen = id; }

void screens_request_theme_reinit() { themeReinitPending = true; }

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

ScreenId screens_get_current() { return currentScreen; }

bool screens_is_active(ScreenId id) { return (currentScreen == id); }

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE DISPATCHER
// ───────────────────────────────────────────────────────────────────────────────
void screens_update_current() {
  ui_control_refresh();
  stale_controls(screenRoots[currentScreen], false);
  screen_setup_update();
  switch (currentScreen) {
    case SCREEN_SETUP: break; // Observed above, including while the fault overlay is visible.
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
    case SCREEN_PEDAL_SETTINGS:
      screen_pedal_settings_update();
      break;
    case SCREEN_DIAGNOSTICS:
      screen_diagnostics_update();
      break;
    case SCREEN_ABOUT:
      screen_about_update();
      break;
    case SCREEN_CONFIRM:
      screen_confirm_update();
      break;
    case SCREEN_NONE:
    case SCREEN_MENU:
    case SCREEN_RUN_MODES:
    case SCREEN_SETTINGS:
    case SCREEN_BOOT:
    case SCREEN_COUNT:
      break;
  }
  const bool stale = !ui_control_fresh() && currentScreen != SCREEN_BOOT && currentScreen != SCREEN_NONE;
  stale_controls(screenRoots[currentScreen], stale);
  if (staleBanner) {
    if (stale && !safety_is_estop_locked()) lv_obj_set_hidden(staleBanner, false);
    else lv_obj_set_hidden(staleBanner, true);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SHARED UI WIDGETS (lvglTask only)
// ───────────────────────────────────────────────────────────────────────────────
void ui_add_post_header_accent(lv_obj_t* parent) {
  lv_obj_t* ln = lv_obj_create(parent);
  lv_coord_t w = SCREEN_W - 2 * HEADER_ACCENT_PAD_X;
  lv_obj_set_size(ln, w, 1);
  lv_obj_set_pos(ln, HEADER_ACCENT_PAD_X, HEADER_ACCENT_LINE_Y);
  lv_obj_set_style_bg_color(ln, COL_ACCENT, 0);
  lv_obj_set_style_bg_opa(ln, LV_OPA_40, 0);
  lv_obj_set_style_border_width(ln, 0, 0);
  lv_obj_set_style_radius(ln, 0, 0);
  lv_obj_set_style_pad_all(ln, 0, 0);
  lv_obj_set_scrollable(ln, false);
  lv_obj_set_clickable(ln, false);
}

lv_obj_t* ui_create_header(lv_obj_t* parent, const char* title, const char* right_caption,
                           lv_obj_t** opt_right_lbl) {
  lv_obj_t* header = lv_obj_create(parent);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_scrollable(header, false);
  lv_obj_t* brand = lv_label_create(header);
  lv_label_set_text(brand, "TIG / ROTATOR");
  lv_obj_set_style_text_font(brand, FONT_SMALL, 0);
  lv_obj_set_style_text_color(brand, COL_TEXT_DIM, 0);
  lv_obj_set_pos(brand, 24, 10);
  lv_obj_t* heading = lv_label_create(header);
  lv_label_set_text(heading, title);
  lv_obj_set_style_text_font(heading, FONT_XL, 0);
  lv_obj_set_style_text_color(heading, lv_color_hex(0xF5F5F0), 0);
  lv_obj_set_width(heading, 530);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(heading, 24, 32);
  lv_obj_t* status = lv_label_create(header);
  lv_label_set_text(status, right_caption ? right_caption : "");
  lv_obj_set_style_text_font(status, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(status, COL_ACCENT, 0);
  lv_obj_set_width(status, 200);
  lv_label_set_long_mode(status, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(status, 576, 28);
  if (opt_right_lbl) *opt_right_lbl = status;
  // V5 header separates the screen through its background, without a hairline.
  return header;
}

lv_obj_t* ui_create_settings_header(lv_obj_t* parent, const char* title, const char* right_caption,
                                    lv_color_t right_text_color) {
  lv_obj_t* status = nullptr;
  lv_obj_t* header = ui_create_header(parent, title, right_caption, &status);
  lv_obj_set_style_text_color(status, right_text_color, 0);
  return header;
}

lv_obj_t* ui_create_separator(lv_obj_t* parent, lv_coord_t y) {
  return ui_create_separator_line(parent, 0, y, SCREEN_W, COL_BORDER);
}

lv_obj_t* ui_create_separator_line(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                   lv_color_t color) {
  lv_obj_t* line = lv_obj_create(parent);
  lv_obj_set_size(line, w, 1);
  lv_obj_set_pos(line, x, y);
  lv_obj_set_style_bg_color(line, color, 0);
  lv_obj_set_style_pad_all(line, 0, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_radius(line, 0, 0);
  lv_obj_set_scrollable(line, false);
  return line;
}

void ui_style_post_card(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, COL_BG_CARD, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, COL_BORDER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, RADIUS_CARD, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_scrollable(obj, false);
  lv_obj_set_clickable(obj, false);
}

void ui_style_post_row(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, COL_BG_ROW, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_scrollable(obj, false);
  lv_obj_set_clickable(obj, false);
}

void ui_style_post_warn(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, COL_BG_WARN_PANEL, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, COL_BORDER_WARN, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_scrollable(obj, false);
}

void ui_style_post_ok(lv_obj_t* obj) {
  lv_obj_set_style_bg_color(obj, COL_BG_OK, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(obj, COL_BORDER_OK, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_radius(obj, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_scrollable(obj, false);
}

lv_obj_t* ui_create_post_card(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  ui_style_post_card(card);
  return card;
}

lv_obj_t* ui_create_post_row(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, w, h);
  lv_obj_set_pos(row, x, y);
  ui_style_post_row(row);
  return row;
}

void ui_btn_style_post(lv_obj_t* btn, UiBtnStyle style) {
  const bool accent = (style == UI_BTN_ACCENT);
  const bool danger = (style == UI_BTN_DANGER);
  lv_color_t bg = danger ? lv_color_hex(0xB52C35) : (accent ? COL_ACCENT : COL_BTN_BG);
  lv_color_t bor = danger ? COL_BORDER_DNG : (accent ? COL_ACCENT : COL_BORDER);
  lv_coord_t bw = (accent || danger) ? 2 : 1;
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, bw, 0);
  lv_obj_set_style_border_color(btn, bor, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_STATE_DISABLED);
  lv_obj_set_style_recolor_opa(btn, LV_OPA_TRANSP, LV_STATE_DISABLED);
}

lv_color_t ui_btn_label_color_post(UiBtnStyle style) {
  if (style == UI_BTN_DANGER) return lv_color_hex(0xFFFFFF);
  if (style == UI_BTN_ACCENT) return lv_color_hex(0x13171A);
  return COL_TEXT;
}

void ui_nav_card_btn_style(lv_obj_t* btn, bool accent) {
  lv_obj_set_style_bg_color(btn, accent ? COL_BG_ACTIVE : COL_BG_CARD, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, accent ? 2 : 1, 0);
  lv_obj_set_style_border_color(btn, accent ? COL_ACCENT : COL_BORDER, 0);
  lv_obj_set_style_radius(btn, RADIUS_CARD, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
}

lv_obj_t* ui_create_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        const char* text, const lv_font_t* label_font, UiBtnStyle style, lv_event_cb_t cb,
                        void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  ui_btn_style_post(btn, style);
  if (cb) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
  }
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, label_font, 0);
  lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(style), 0);
  lv_obj_center(lbl);
  return btn;
}

lv_obj_t* ui_create_pm_btn(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, const char* text,
                           const lv_font_t* label_font, UiBtnStyle style, lv_event_cb_t cb, void* user_data) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, BTN_W_PM, BTN_H_PM);
  lv_obj_set_pos(btn, x, y);
  ui_btn_style_post(btn, style);
  // Tap: SHORT_CLICKED. Hold: LONG_PRESSED then LONG_PRESSED_REPEAT (indev long_press / repeat times).
  // Avoid LV_EVENT_CLICKED here: it also fires on release after a long hold and would double-step.
  if (cb) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_SHORT_CLICKED, user_data);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED, user_data);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED_REPEAT, user_data);
  }
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, label_font, 0);
  lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(style), 0);
  lv_obj_center(lbl);
  return btn;
}

// Shared slider style — dark track with accent indicator/knob. Individual callers may still
// override knob size or per-part padding afterwards if needed.
void ui_style_slider(lv_obj_t* slider) {
  lv_obj_set_style_bg_color(slider, COL_SLIDER_TRACK2, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(slider, COL_BORDER, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_all(slider, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, COL_ACCENT, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_border_color(slider, COL_TEXT_DIM, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 2, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, 10, LV_PART_KNOB);
}

void ui_create_action_bar(lv_obj_t* parent, lv_coord_t pad_x, lv_coord_t footer_y, lv_coord_t footer_h,
                          lv_coord_t gap, lv_coord_t left_w, lv_coord_t right_w, const char* left_text,
                          lv_event_cb_t left_cb, const char* right_text, UiBtnStyle right_style,
                          lv_event_cb_t right_cb) {
  ui_create_btn(parent, pad_x, footer_y, left_w, footer_h, left_text, FONT_SUBTITLE, UI_BTN_NORMAL, left_cb,
                nullptr);
  ui_create_btn(parent, pad_x + left_w + gap, footer_y, right_w, footer_h, right_text, FONT_SUBTITLE,
                right_style, right_cb, nullptr);
}

void ui_create_action_bar_three(lv_obj_t* parent, lv_coord_t pad_x, lv_coord_t y, lv_coord_t h,
                                lv_coord_t gap, lv_coord_t btn_w, const char* left_text,
                                lv_event_cb_t left_cb, UiBtnStyle left_style, const char* mid_text,
                                lv_event_cb_t mid_cb, UiBtnStyle mid_style, const char* right_text,
                                lv_event_cb_t right_cb, UiBtnStyle right_style, lv_obj_t** out_left_btn,
                                lv_obj_t** out_mid_btn, lv_obj_t** out_right_btn) {
  lv_coord_t x0 = pad_x;
  lv_coord_t x1 = pad_x + btn_w + gap;
  lv_coord_t x2 = pad_x + (btn_w + gap) * 2;
  lv_obj_t* b0 =
      ui_create_btn(parent, x0, y, btn_w, h, left_text, FONT_SUBTITLE, left_style, left_cb, nullptr);
  lv_obj_t* b1 = ui_create_btn(parent, x1, y, btn_w, h, mid_text, FONT_SUBTITLE, mid_style, mid_cb, nullptr);
  lv_obj_t* b2 =
      ui_create_btn(parent, x2, y, btn_w, h, right_text, FONT_SUBTITLE, right_style, right_cb, nullptr);
  if (out_left_btn) *out_left_btn = b0;
  if (out_mid_btn) *out_mid_btn = b1;
  if (out_right_btn) *out_right_btn = b2;
}

// ───────────────────────────────────────────────────────────────────────────────
// BACK BUTTON HELPER
// ───────────────────────────────────────────────────────────────────────────────
void screens_set_back_button(lv_obj_t* btn, ScreenId dest) {
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        ScreenId dest = (ScreenId)(size_t)lv_event_get_user_data(e);
        screens_show(dest);
      },
      LV_EVENT_CLICKED, (void*)(size_t)dest);
}

void screens_set_edit_slot(int slot) { pendingEditSlot = slot; }
