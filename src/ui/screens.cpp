// TIG Rotator Controller - Screen Management Implementation
// Handles navigation between registered LVGL screen roots

#include <Arduino.h>
#include "screens.h"
#include "theme.h"
#include "text_metrics.h"
#include "screen_saver.h"
#include "../config.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include <cmath>
#include <cstdio>
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
  ui_trim_text_leading(label);
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

static uint32_t motionSinceMs = 0;
static bool motionSeen = false;
static void track_motion() {
  const SystemState state = ui_control_state();
  const bool moving = state != STATE_IDLE && state != STATE_ESTOP;
  if (moving && !motionSeen) motionSinceMs = millis();
  motionSeen = moving;
}
bool ui_direction_switch_differs() {
  const SystemState state = ui_control_state();
  return g_dir_switch_cache.load() && state != STATE_IDLE && state != STATE_ESTOP &&
         !speed_program_direction_override_active() && speed_get_direction() != ui_control_view().direction;
}
uint32_t ui_motion_elapsed_ms() { return motionSeen ? millis() - motionSinceMs : 0; }

static const char* motion_status_name(SystemState state) {
  switch (state) {
    case STATE_RUNNING: return "Rotating";
    case STATE_PULSE: return "Pulsing";
    case STATE_STEP: return "Stepping";
    case STATE_JOG: return "Jogging";
    case STATE_STOPPING: return "Stopping";
    default: return "Starting";
  }
}

static constexpr int kStatusDot = 12;
static constexpr uint8_t kLiveHeaderFlag = 1;  // Header shows the live machine status.

static void set_motion_status(lv_obj_t* label, bool blocked) {
  if (!label) return;
  const SystemState state = ui_control_state();
  const bool moving = state != STATE_IDLE && state != STATE_ESTOP;
  const char* status = !ui_control_fresh() ? "Status unavailable" : blocked ? "Motion locked" : moving ? motion_status_name(state) : "Ready";
  const lv_color_t color = blocked || !ui_control_fresh() ? COL_RED : moving ? COL_ACCENT : COL_GREEN;
  if (strcmp(lv_label_get_text(label), status) != 0) lv_label_set_text(label, status);
  lv_obj_set_style_text_color(label, color, 0);
  // The dot sits left of the right-aligned text. Text fonts only have a tiny bullet glyph.
  auto* dot = static_cast<lv_obj_t*>(lv_obj_get_user_data(label));
  if (!dot) {
    dot = lv_obj_create(lv_obj_get_parent(label));
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, kStatusDot, kStatusDot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_clickable(dot, false);
    lv_obj_set_user_data(label, dot);
  }
  lv_point_t size;
  lv_text_get_size(&size, status, lv_obj_get_style_text_font(label, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
  lv_obj_set_pos(dot, SCREEN_W - 24 - size.x - 10 - kStatusDot, (HEADER_H - kStatusDot) / 2);
  lv_obj_set_style_bg_color(dot, color, 0);
}

static void update_header_status(lv_obj_t* root) {
  if (!root) return;
  const SystemState state = ui_control_state();
  const bool blocked = !ui_control_fresh() || safety_inhibit_motion() || safety_is_estop_locked() || state == STATE_ESTOP;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
    lv_obj_t* child = lv_obj_get_child(root, i);
    if (lv_obj_get_user_flag(child, kLiveHeaderFlag)) set_motion_status(lv_obj_get_child(child, 1), blocked);
  }
}

static constexpr uint8_t kRotationCcwFlag = 1;  // Flag 0 marks motion buttons.
static void draw_rotation_icon(lv_event_t* e) {
  lv_obj_t* icon = lv_event_get_target_obj(e);
  lv_area_t area;
  lv_obj_get_coords(icon, &area);
  const bool cw = !lv_obj_get_user_flag(icon, kRotationCcwFlag);
  const lv_color_t color = lv_obj_get_style_text_color(icon, LV_PART_MAIN);
  const float radius = lv_area_get_width(&area) / 2.0f - 4.0f;
  const float centerX = (area.x1 + area.x2) / 2.0f, centerY = (area.y1 + area.y2) / 2.0f;
  lv_draw_arc_dsc_t arc;
  lv_draw_arc_dsc_init(&arc);
  arc.color = color;
  arc.width = 3;
  arc.rounded = 1;
  arc.center = {(int32_t)centerX, (int32_t)centerY};
  arc.radius = (uint16_t)radius;
  // LVGL angles grow clockwise from east. The gap and arrow head sit at the top.
  arc.start_angle = cw ? 340 : 240;
  arc.end_angle = cw ? 300 : 200;
  lv_draw_arc(lv_event_get_layer(e), &arc);
  const float head = (cw ? 300.0f : 240.0f) * 3.14159265f / 180.0f;
  const float side = cw ? 1.0f : -1.0f;
  const float x = centerX + radius * cos(head), y = centerY + radius * sin(head);
  const float radialX = cos(head), radialY = sin(head), tangentX = -radialY * side, tangentY = radialX * side;
  lv_draw_triangle_dsc_t triangle;
  lv_draw_triangle_dsc_init(&triangle);
  triangle.color = color;
  triangle.p[0] = {(lv_value_precise_t)(x + tangentX * radius * 0.7f), (lv_value_precise_t)(y + tangentY * radius * 0.7f)};
  triangle.p[1] = {(lv_value_precise_t)(x + radialX * radius * 0.45f), (lv_value_precise_t)(y + radialY * radius * 0.45f)};
  triangle.p[2] = {(lv_value_precise_t)(x - radialX * radius * 0.45f), (lv_value_precise_t)(y - radialY * radius * 0.45f)};
  lv_draw_triangle(lv_event_get_layer(e), &triangle);
}

lv_obj_t* ui_create_rotation_icon(lv_obj_t* parent, int size, lv_color_t color, Direction direction) {
  lv_obj_t* icon = lv_obj_create(parent);
  lv_obj_remove_style_all(icon);
  lv_obj_set_size(icon, size, size);
  lv_obj_set_style_text_color(icon, color, 0);
  lv_obj_set_user_flag(icon, kRotationCcwFlag, direction == DIR_CCW);
  lv_obj_set_clickable(icon, false);
  lv_obj_add_event_cb(icon, draw_rotation_icon, LV_EVENT_DRAW_MAIN, nullptr);
  return icon;
}

void ui_set_rotation_icon(lv_obj_t* icon, Direction direction) {
  if (!icon || lv_obj_get_user_flag(icon, kRotationCcwFlag) == (direction == DIR_CCW)) return;
  lv_obj_set_user_flag(icon, kRotationCcwFlag, direction == DIR_CCW);
  lv_obj_invalidate(icon);
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
void ui_mark_motion_action(lv_obj_t* obj) { if (obj) lv_obj_set_user_flag(obj, 0, true); }
void ui_mark_motion_callback(lv_obj_t* root, lv_event_cb_t callback) {
  if (!root) return;
  for (uint32_t i=0; i<lv_obj_get_event_count(root); ++i)
    if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(root,i)) == callback) ui_mark_motion_action(root);
  for (uint32_t i=0; i<lv_obj_get_child_count(root); ++i) ui_mark_motion_callback(lv_obj_get_child(root,i),callback);
}
static bool movement_button(lv_obj_t* obj) { return lv_obj_get_user_flag(obj, 0); }
static void stale_controls(lv_obj_t* obj, bool disable) {
  if (!obj) return;
  if (!disable && lv_obj_is_state_user_2(obj)) {
    lv_obj_set_state_user_2(obj, false); lv_obj_set_disabled(obj, false);
  }
  if (disable && movement_button(obj) && !lv_obj_is_disabled(obj)) {
    lv_obj_set_state_user_2(obj, true); lv_obj_set_disabled(obj, true);
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
         id == SCREEN_EDIT_STEP || id == SCREEN_STEP || id == SCREEN_MOTOR_CONFIG;
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

  ui_trim_text_leading(screenRoots[id]);
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
  ui_create_text(staleBanner, 16, 15, 720, "Status unavailable / Stop remains available", FONT_NORMAL, lv_color_hex(0xFFFFFF));
  lv_obj_set_hidden(staleBanner, true);
  estop_overlay_create();
  create_screen(SCREEN_MAIN);

  LOG_I("Screens init complete");
}

void screens_reinit() {
  screen_saver_destroy();
  ScreenId prev = currentScreen;
  if (prev == SCREEN_CALIBRATION) screen_calibration_leave();

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
  dim_reset_activity();

  ScreenId prev = currentScreen;
  if (prev == SCREEN_TIMER && id != SCREEN_TIMER) screen_timer_leave();
  if (prev == SCREEN_CALIBRATION && id != SCREEN_CALIBRATION) screen_calibration_leave();
  if (prev == SCREEN_PROGRAM_EDIT && id != SCREEN_PROGRAM_EDIT) screen_program_edit_leave();
  if (prev == SCREEN_MOTOR_CONFIG && id != SCREEN_MOTOR_CONFIG) screen_motor_config_leave();
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

  if (id == SCREEN_CALIBRATION && prev != id) screen_calibration_enter();
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
  track_motion();
  update_header_status(screenRoots[currentScreen]);
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
  lv_obj_t* heading = lv_label_create(header);
  lv_label_set_text(heading, title);
  lv_obj_set_style_text_font(heading, FONT_XXL, 0);
  lv_obj_set_style_text_color(heading, lv_color_hex(0xF5F5F0), 0);
  lv_obj_set_width(heading, 530);
  lv_label_set_long_mode(heading, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_pos(heading, 24, 22);
  lv_obj_t* status = lv_label_create(header);
  lv_label_set_text(status, right_caption ? right_caption : "");
  lv_obj_set_style_text_font(status, FONT_BTN, 0);
  lv_obj_set_style_text_color(status, COL_ACCENT, 0);
  lv_obj_set_width(status, 200);
  lv_label_set_long_mode(status, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(status, 576, 27);
  if (opt_right_lbl) *opt_right_lbl = status;
  else lv_obj_set_user_flag(header, kLiveHeaderFlag, true);
  // V5 header separates the screen through its background, without a hairline.
  return header;
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
  // Show command availability immediately; theme fades can leave enabled text
  // on a disabled background after calibration or a storage receipt.
  static const lv_style_prop_t immediateProperties[] = {
      LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_OPA, LV_STYLE_BORDER_COLOR,
      LV_STYLE_BORDER_WIDTH, LV_STYLE_OUTLINE_OPA, LV_STYLE_OUTLINE_WIDTH,
      LV_STYLE_RECOLOR, LV_STYLE_RECOLOR_OPA, 0};
  static lv_style_transition_dsc_t immediate;
  static bool immediateInitialized = false;
  if (!immediateInitialized) {
    lv_style_transition_dsc_init(&immediate, immediateProperties, lv_anim_path_linear, 0, 0, nullptr);
    immediateInitialized = true;
  }
  for (auto state : {LV_STATE_DEFAULT, LV_STATE_PRESSED, LV_STATE_CHECKED, LV_STATE_DISABLED})
    lv_obj_set_style_transition(btn, &immediate, state);
  lv_obj_set_style_opa(btn, LV_OPA_70, LV_STATE_DISABLED);
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
  ui_trim_text_leading(lbl);
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
  ui_trim_text_leading(lbl);
  lv_obj_center(lbl);
  return btn;
}

// Shared slider style — dark track with accent indicator/knob. Individual callers may still
// override knob size or per-part padding afterwards if needed.
void ui_style_slider(lv_obj_t* slider) {
  lv_obj_set_style_opa(slider, LV_OPA_70, LV_STATE_DISABLED);
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

// ───────────────────────────────────────────────────────────────────────────────
// BACK BUTTON HELPER
// ───────────────────────────────────────────────────────────────────────────────
void screens_set_edit_slot(int slot) { pendingEditSlot = slot; }
