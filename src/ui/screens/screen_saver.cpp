#include "../screen_saver.h"
#include "../screens.h"
#include "../theme.h"
#include "../display.h"
#include "../../safety/safety.h"
#include <cmath>

namespace {
lv_obj_t* overlay = nullptr;
lv_obj_t* composition = nullptr;
lv_obj_t* orbit = nullptr;
uint32_t lastActivity = 0, lastFrame = 0;
unsigned heldInputs = 0, frame = 0;
bool visible = false, previewRequested = false, preview = false;
const auto background = lv_color_hex(0x11191B);
const auto accent = lv_color_hex(0xFF693B);
const auto text = lv_color_hex(0xE9EEEB);
const auto muted = lv_color_hex(0x9CAAA8);

lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
  auto obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(obj, false); lv_obj_set_clickable(obj, false);
  return obj;
}
lv_obj_t* circle(lv_obj_t* parent, int x, int y, int size, lv_color_t color, int stroke) {
  auto obj = box(parent, x, y, size, size, color);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  if (stroke) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(obj, color, 0);
    lv_obj_set_style_border_width(obj, stroke, 0);
  }
  return obj;
}
void create() {
  overlay = box(lv_layer_top(), 0, 0, SCREEN_W, SCREEN_H, background);
  // A complete input shield; input filtering consumes the wake press as well.
  lv_obj_set_clickable(overlay, true);
  ui_create_text(overlay, 40, 34, 360, "TIG / ROTATOR", FONT_XL, text);
  circle(overlay, 623, 39, 8, accent, 0);
  auto status = ui_create_text(overlay, 647, 35, 113, "DISPLAY IDLE", FONT_TINY, muted);
  lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_RIGHT, 0);
  box(overlay, 40, 79, 720, 1, lv_color_hex(0x303B3C));

  composition = box(overlay, 42, 117, 716, 250, background);
  circle(composition, 20, 15, 220, lv_color_hex(0x303B3C), 2);
  circle(composition, 44, 39, 172, lv_color_hex(0x586764), 1);
  auto arc = lv_arc_create(composition);
  lv_obj_remove_style_all(arc);
  lv_obj_set_pos(arc, 20, 15); lv_obj_set_size(arc, 220, 220);
  lv_arc_set_bg_angles(arc, 0, 360); lv_arc_set_rotation(arc, 245);
  lv_arc_set_range(arc, 0, 100); lv_arc_set_value(arc, 23);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, accent, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
  lv_obj_set_clickable(arc, false);
  box(composition, 129, 63, 2, 124, lv_color_hex(0x303B3C));
  box(composition, 68, 124, 124, 2, lv_color_hex(0x303B3C));
  circle(composition, 95, 90, 70, accent, 0);
  circle(composition, 114, 109, 32, background, 0);
  orbit = circle(composition, 122, 30, 16, text, 0);
  ui_create_text(composition, 308, 29, 360, "01 / STANDBY", FONT_SUBTITLE, accent);
  ui_create_text(composition, 306, 72, 380, "Ready when", FONT_HUGE, text);
  ui_create_text(composition, 306, 121, 380, "you are.", FONT_HUGE, text);
  ui_create_text(composition, 308, 194, 360, "Welding positioner  /  Motor stopped", FONT_SUBTITLE, muted);

  box(overlay, 40, 399, 720, 1, lv_color_hex(0x303B3C));
  auto wake = ui_create_text(overlay, 40, 429, 720, "TOUCH ANYWHERE TO WAKE", FONT_LARGE, text);
  lv_obj_set_style_text_align(wake, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_hidden(overlay, true);
}
bool allowed() {
  if (!ui_control_fresh() || ui_control_state() != STATE_IDLE ||
      control_get_state() != STATE_IDLE || ui_control_view().motor_running ||
      safety_inhibit_motion() || control_setup_active() ||
      g_flashWriting.load(std::memory_order_acquire) ||
      storage_status() != STORAGE_SAVED) return false;
  // Editors, confirmations and commissioning remain visible even when paused.
  switch (screens_get_current()) {
    case SCREEN_MAIN: case SCREEN_MENU: case SCREEN_RUN_MODES:
    case SCREEN_SETTINGS: case SCREEN_DISPLAY: case SCREEN_ABOUT:
    case SCREEN_SYSINFO: case SCREEN_DIAGNOSTICS: break;
    default: return false;
  }
  auto top = lv_layer_top();
  for (uint32_t i = 0; i < lv_obj_get_child_count(top); ++i) {
    auto child = lv_obj_get_child(top, i);
    if (child != overlay && !lv_obj_is_hidden(child)) return false;
  }
  return true;
}
void restore_brightness() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const uint8_t brightness = g_settings.brightness;
  xSemaphoreGive(g_settings_mutex);
  display_set_brightness(brightness);
}
void hide() {
  if (visible) {
    lv_obj_set_hidden(overlay, true);
    visible = false;
    restore_brightness();
  }
  preview = false;
}
}

bool screen_saver_visible() { return visible; }
bool screen_saver_available() { return allowed(); }
void dim_reset_activity() {
  lastActivity = lv_tick_get();
  previewRequested = false;
  hide();
}
void screen_saver_destroy() {
  dim_reset_activity();
  if (overlay) lv_obj_delete(overlay);
  overlay = composition = orbit = nullptr;
}
void screen_saver_request_preview() { previewRequested = true; }

void screen_saver_filter_input(lv_indev_data_t* data, ScreenSaverInput& input) {
  if (!data) return;
  const bool pressed = data->state == LV_INDEV_STATE_PRESSED;
  if (pressed != input.pressed) {
    if (pressed) ++heldInputs;
    else if (heldInputs) --heldInputs;
    input.pressed = pressed;
  }
  if (pressed || data->enc_diff) {
    const bool waking = visible;
    if (waking && pressed) input.suppressUntilRelease = true;
    dim_reset_activity();
    if (waking) data->enc_diff = 0;
  }
  if (input.suppressUntilRelease) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->enc_diff = 0;
    if (!pressed) input.suppressUntilRelease = false;
  }
}

void screen_saver_update(uint32_t now) {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  const uint16_t seconds = g_settings.dim_timeout;
  xSemaphoreGive(g_settings_mutex);
  // Drain wake requests even when the timeout is OFF.
  if (g_wakePending.exchange(false, std::memory_order_acq_rel)) {
    dim_reset_activity(); return;
  }
  if (!allowed() || heldInputs || (!seconds && !preview && !previewRequested)) {
    dim_reset_activity(); lastActivity = now; return;
  }
  if (!visible && (previewRequested || now - lastActivity >= uint32_t(seconds) * 1000)) {
    preview = previewRequested; previewRequested = false;
    if (!overlay) create();
    lv_obj_move_foreground(overlay); lv_obj_set_hidden(overlay, false);
    visible = true; frame = 0; lastFrame = now;
    lv_obj_set_pos(composition, 42, 117); lv_obj_set_pos(orbit, 122, 30);
    // Preview retains brightness so the design can be inspected.
    if (!preview) display_set_brightness(38);
  }
  if (visible && now - lastFrame >= 4000) {
    lastFrame = now; frame = (frame + 1) % 72;
    const float angle = float(frame) * 0.08726646f;
    lv_obj_set_pos(orbit, 122 + int(87 * std::sin(angle)), 117 - int(87 * std::cos(angle)));
    lv_obj_set_pos(composition, 42 + int(8 * std::sin(angle)), 117 + int(6 * std::cos(angle)));
  }
}
void dim_update() { screen_saver_update(lv_tick_get()); }
