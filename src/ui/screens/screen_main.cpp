// V5 operating panel. Values are calculated from pulses, not encoder feedback.
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../value_binding.h"
#include "../tabular_font.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../motor/motor.h"
#include "../../control/control.h"
#include "../../safety/safety.h"
#include "../../storage/storage.h"
#include <cmath>
#include <cstdio>
LV_FONT_DECLARE(rotator_digits_104);

static constexpr int kScaleX = 22, kScaleW = 416, kScaleY = 193, kScaleTicks = 30, kPointerW = 20, kPointerH = 14;
static lv_obj_t *rpmLabel, *speedCaption, *sourceLabel, *surfaceLabel;
static lv_obj_t *diameterLabel, *limitLabel, *detailCaption, *detailValue, *detailMessage, *speedPointer, *modeTitle;
static lv_obj_t *startBtn, *stopBtn, *menuBtn, *programsBtn, *minusBtn, *plusBtn, *cwBtn, *ccwBtn, *cwIcon, *ccwIcon;
static lv_obj_t* minimumLabel = nullptr;
struct MainBindings {
  UiTextBinding<16> rpm;
  UiTextBinding<40> caption, minimum, maximum, surface, diameter, title, source, startText, detailCaption, detailValue;
  UiTextBinding<64> detailMessage;
  UiIntBinding speed;
  UiBoolBinding startHidden, stopHidden, startDisabled, menuDisabled, programsDisabled, minusDisabled, plusDisabled,
      detailRowHidden, detailValueHidden, detailMessageHidden;
  void reset() {
    rpm.reset(); detailMessage.reset(); speed.reset();
    for (auto* text : {&caption, &minimum, &maximum, &surface, &diameter, &title, &source, &startText, &detailCaption, &detailValue})
      text->reset();
    for (auto* flag : {&startHidden, &stopHidden, &startDisabled, &menuDisabled, &programsDisabled, &minusDisabled, &plusDisabled,
                       &detailRowHidden, &detailValueHidden, &detailMessageHidden})
      flag->reset();
  }
};
static MainBindings bindings;
static void set_pointer(lv_obj_t* pointer, int32_t value) {
  lv_obj_set_x(pointer, kScaleX + value * kScaleW / 1000 - kPointerW / 2);
}
static void draw_pointer(lv_event_t* e) {
  lv_obj_t* pointer = lv_event_get_target_obj(e);
  lv_area_t area;
  lv_obj_get_coords(pointer, &area);
  lv_draw_triangle_dsc_t triangle;
  lv_draw_triangle_dsc_init(&triangle);
  triangle.color = COL_ACCENT;
  triangle.p[0] = {(lv_value_precise_t)area.x1, (lv_value_precise_t)area.y2};
  triangle.p[1] = {(lv_value_precise_t)((area.x1 + area.x2) / 2), (lv_value_precise_t)area.y1};
  triangle.p[2] = {(lv_value_precise_t)area.x2, (lv_value_precise_t)area.y2};
  lv_draw_triangle(lv_event_get_layer(e), &triangle);
}
static bool can_edit() { return ui_control_fresh() && ui_control_state() == STATE_IDLE && !safety_inhibit_motion(); }
static void start_cb(lv_event_t*) {
  if (can_edit()) control_start_continuous();
}
static void stop_cb(lv_event_t*) { control_stop(); }
static void menu_cb(lv_event_t*) {
  if (control_get_state() == STATE_IDLE) screens_show(SCREEN_MENU);
}
static void programs_cb(lv_event_t*) {
  if (control_get_state() != STATE_IDLE) return;
  screen_programs_set_back(SCREEN_MAIN);
  screens_show(SCREEN_PROGRAMS);
}
static void adjust_cb(lv_event_t* e) {
  if (can_edit()) speed_slider_set(speed_get_target_rpm() + (int)(intptr_t)lv_event_get_user_data(e) * ui_rpm_increment(speed_get_target_rpm()));
}
static void direction_cb(lv_event_t* e) {
  if (!can_edit() || g_dir_switch_cache.load()) return;
  Direction dir = (Direction)(uintptr_t)lv_event_get_user_data(e);
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  bool invert = g_settings.invert_direction;
  xSemaphoreGive(g_settings_mutex);
  if (invert) dir = dir == DIR_CW ? DIR_CCW : DIR_CW;
  speed_set_direction(dir);
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.manual_direction = (uint8_t)dir;
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
}
static void enabled(lv_obj_t* o, bool yes) {
  if (yes)
    lv_obj_set_disabled(o, false);
  else
    lv_obj_set_disabled(o, true);
}
static void create_hero(lv_obj_t* s) {
  lv_obj_t* hero = ui_create_post_card(s, 24, 96, 460, 278);
  speedCaption = ui_create_text(hero, 22, 18, 410, "Target speed", FONT_SUBTITLE, COL_TEXT_DIM);
  static UiTabularFont rpmFont(&rotator_digits_104);
  rpmLabel = ui_create_text(hero, 18, 56, 340, "0.50", rpmFont.get(), COL_TEXT);
  lv_obj_set_height(rpmLabel, rpmFont.get()->line_height);
  lv_obj_set_style_transform_pivot_x(rpmLabel, 0, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, 0, 0);
  lv_obj_t* unit = ui_create_text(hero, 250, 16, 90, "RPM", FONT_LARGE, COL_TEXT_DIM);
  lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_RIGHT, 0);
  // Beside the value they change, and away from Start: a gloved miss must not start rotation.
  plusBtn = ui_create_btn(hero, 356, 14, 88, 80, "+", FONT_HUGE, UI_BTN_NORMAL, adjust_cb, (void*)(intptr_t)1);
  minusBtn = ui_create_btn(hero, 356, 102, 88, 80, "-", FONT_HUGE, UI_BTN_NORMAL, adjust_cb, (void*)(intptr_t)-1);
  for (int i = 0; i <= kScaleTicks; i++) {
    lv_obj_t* tick = lv_obj_create(hero);
    lv_obj_remove_style_all(tick);
    lv_obj_set_pos(tick, kScaleX + i * kScaleW / kScaleTicks, i % 5 == 0 ? kScaleY : kScaleY + 6);
    lv_obj_set_size(tick, 2, i % 5 == 0 ? 14 : 8);
    lv_obj_set_style_bg_color(tick, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
  }
  speedPointer = lv_obj_create(hero);
  lv_obj_remove_style_all(speedPointer);
  lv_obj_set_size(speedPointer, kPointerW, kPointerH);
  lv_obj_set_pos(speedPointer, kScaleX - kPointerW / 2, kScaleY + 19);
  lv_obj_set_clickable(speedPointer, false);
  lv_obj_add_event_cb(speedPointer, draw_pointer, LV_EVENT_DRAW_MAIN, nullptr);
  minimumLabel = ui_create_text(hero, 22, 244, 140, "", FONT_NORMAL, COL_TEXT_DIM);
  limitLabel = ui_create_text(hero, 205, 244, 233, "", FONT_NORMAL, COL_TEXT_DIM);
  lv_obj_set_style_text_align(limitLabel, LV_TEXT_ALIGN_RIGHT, 0);
}
static void create_readouts(lv_obj_t* s) {
  ui_create_post_card(s, 504, 96, 272, 74);
  cwBtn = ui_create_btn(s, 508, 100, 130, 66, "CW", FONT_XL, UI_BTN_NORMAL, direction_cb, (void*)(uintptr_t)DIR_CW);
  ccwBtn = ui_create_btn(s, 642, 100, 130, 66, "CCW", FONT_XL, UI_BTN_NORMAL, direction_cb, (void*)(uintptr_t)DIR_CCW);
  cwIcon = ui_create_rotation_icon(cwBtn, 30, ui_btn_label_color_post(UI_BTN_ACCENT), DIR_CW);
  ccwIcon = ui_create_rotation_icon(ccwBtn, 30, ui_btn_label_color_post(UI_BTN_ACCENT), DIR_CCW);
  lv_obj_align(cwIcon, LV_ALIGN_CENTER, 28, 0);
  lv_obj_align(ccwIcon, LV_ALIGN_CENTER, 36, 0);
  ui_create_text(s, 504, 186, 150, "Surface speed", FONT_SUBTITLE, COL_TEXT_DIM);
  diameterLabel = ui_create_text(s, 654, 186, 122, "", FONT_SUBTITLE, COL_TEXT_DIM);
  lv_obj_set_style_text_align(diameterLabel, LV_TEXT_ALIGN_RIGHT, 0);
  surfaceLabel = ui_create_text(s, 504, 210, 200, "", FONT_HUGE, COL_TEXT);
  lv_obj_set_width(surfaceLabel, LV_SIZE_CONTENT);
  lv_obj_t* unit = ui_create_text(s, 0, 0, 120, "mm/min (calc.)", FONT_NORMAL, COL_TEXT_DIM);
  lv_obj_set_width(unit, LV_SIZE_CONTENT);
  lv_obj_t* row = lv_obj_create(s);
  lv_obj_remove_style_all(row);
  lv_obj_set_pos(row, 504, 208);
  lv_obj_set_size(row, 272, 48);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_set_parent(surfaceLabel, row);
  lv_obj_set_parent(unit, row);
  ui_create_separator_line(s, 504, 262, 272, COL_BORDER);
  ui_create_text(s, 504, 276, 80, "Source", FONT_SUBTITLE, COL_TEXT_DIM);
  sourceLabel = ui_create_text(s, 586, 276, 190, "", FONT_SUBTITLE, COL_TEXT);
  lv_obj_set_style_text_align(sourceLabel, LV_TEXT_ALIGN_RIGHT, 0);
  ui_create_separator_line(s, 504, 308, 272, COL_BORDER);
  detailCaption = ui_create_text(s, 504, 322, 110, "Pedal", FONT_SUBTITLE, COL_TEXT_DIM);
  detailValue = ui_create_text(s, 616, 320, 160, "", FONT_LARGE, COL_TEXT);
  lv_obj_set_style_text_align(detailValue, LV_TEXT_ALIGN_RIGHT, 0);
  detailMessage = ui_create_text(s, 504, 318, 272, "", FONT_SUBTITLE, COL_TEXT);
  lv_label_set_long_mode(detailMessage, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_hidden(detailMessage, true);
}
void screen_main_create() {
  bindings.reset();
  lv_obj_t* s = screenRoots[SCREEN_MAIN];
  lv_obj_clean(s);
  lv_obj_t* h = ui_create_header(s, "Continuous rotation", "", nullptr);
  modeTitle = lv_obj_get_child(h, 0);
  create_hero(s);
  create_readouts(s);
  menuBtn = ui_create_btn(s, 24, 408, 184, 56, "Menu", FONT_BTN, UI_BTN_NORMAL, menu_cb, nullptr);
  programsBtn = ui_create_btn(s, 216, 408, 184, 56, "Programs", FONT_BTN, UI_BTN_NORMAL, programs_cb, nullptr);
  startBtn = ui_create_btn(s, 416, 408, 360, 56, UI_LABEL_START, FONT_LARGE, UI_BTN_ACCENT, start_cb, nullptr);
  stopBtn = ui_create_btn(s, 416, 408, 360, 56, UI_LABEL_STOP, FONT_LARGE, UI_BTN_DANGER, stop_cb, nullptr);
  lv_obj_set_user_data(startBtn, (void*)(uintptr_t)UI_ACTION_START);
  lv_obj_set_user_data(stopBtn, (void*)(uintptr_t)UI_ACTION_STOP);
  lv_obj_set_hidden(stopBtn, true);
  bindings.rpm.bind(rpmLabel); bindings.caption.bind(speedCaption);
  bindings.minimum.bind(minimumLabel); bindings.maximum.bind(limitLabel);
  bindings.surface.bind(surfaceLabel); bindings.diameter.bind(diameterLabel);
  bindings.title.bind(modeTitle); bindings.source.bind(sourceLabel);
  bindings.detailCaption.bind(detailCaption); bindings.detailValue.bind(detailValue);
  bindings.detailMessage.bind(detailMessage);
  bindings.startText.bind(lv_obj_get_child(startBtn, 0));
  bindings.speed.bind(speedPointer, set_pointer);
  bindings.startHidden.bind(startBtn, lv_obj_set_hidden);
  bindings.stopHidden.bind(stopBtn, lv_obj_set_hidden, true);
  bindings.startDisabled.bind(startBtn, lv_obj_set_disabled, true);
  bindings.menuDisabled.bind(menuBtn, lv_obj_set_disabled);
  bindings.programsDisabled.bind(programsBtn, lv_obj_set_disabled);
  bindings.minusDisabled.bind(minusBtn, lv_obj_set_disabled);
  bindings.plusDisabled.bind(plusBtn, lv_obj_set_disabled);
  bindings.detailRowHidden.bind(detailCaption, lv_obj_set_hidden);
  bindings.detailValueHidden.bind(detailValue, lv_obj_set_hidden);
  bindings.detailMessageHidden.bind(detailMessage, lv_obj_set_hidden, true);
}
static void update_direction(const ControlSnapshot& view, bool moving, bool blocked) {
  const bool cw = view.direction == DIR_CW;
  for (lv_obj_t* b : {cwBtn, ccwBtn}) {
    const bool chosen = (b == cwBtn) == cw;
    lv_obj_t* label = lv_obj_get_child(b, 0);
    lv_color_t fill = chosen ? COL_ACCENT : COL_BG_CARD;
    lv_obj_set_style_bg_color(b, fill, 0);
    lv_obj_set_style_bg_color(b, fill, LV_STATE_DISABLED);
    lv_obj_set_style_opa(b, LV_OPA_COVER, LV_STATE_DISABLED);  // A locked direction must stay readable.
    lv_obj_set_style_border_width(b, 0, 0);
    enabled(b, !moving && !blocked && !g_dir_switch_cache.load());
    lv_obj_set_style_text_color(label, chosen ? ui_btn_label_color_post(UI_BTN_ACCENT) : COL_TEXT_DIM, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, chosen ? -18 : 0, 0);
    lv_obj_set_user_data(b, chosen ? nullptr : (void*)(uintptr_t)UI_ACTION_DIRECTION);
  }
  lv_obj_set_hidden(cwIcon, !cw);
  lv_obj_set_hidden(ccwIcon, cw);
}
static void update_detail(bool moving, bool blocked, bool rangeBlocked) {
  const StorageStatus storage = storage_status();
  const char* message = storage == STORAGE_ERROR ? "Save failed, retry pending"
                        : storage != STORAGE_SAVED ? "Saving changes..."
                        : rangeBlocked ? "Speed outside motor range"
                        : blocked ? "Clear fault before restart"
                        : moving && ui_direction_switch_differs() ? "Direction switch changed; applies at next start" : nullptr;
  bindings.detailRowHidden.set(message != nullptr);
  bindings.detailValueHidden.set(message != nullptr);
  bindings.detailMessageHidden.set(message == nullptr);
  if (message) {
    bindings.detailMessage.set(message);
    return;
  }
  bindings.detailCaption.set(moving ? "Elapsed" : "Pedal");
  char text[16];
  const uint32_t seconds = ui_motion_elapsed_ms() / 1000;
  snprintf(text, sizeof(text), "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  bindings.detailValue.set(moving ? text : speed_get_pedal_enabled() ? "Enabled" : "Off");
}
void screen_main_update() {
  ui_mark_motion_callback(screenRoots[SCREEN_MAIN], start_cb);
  if (!screens_is_active(SCREEN_MAIN) || !rpmLabel) return;
  const auto& view = ui_control_view();
  const SystemState st = ui_control_state();
  const bool moving = st != STATE_IDLE && st != STATE_ESTOP;
  const bool rangeBlocked = !motor_milli_hz_for_rpm_calibrated(view.target_rpm);
  const bool blocked = rangeBlocked || !ui_control_fresh() || safety_inhibit_motion() || safety_is_estop_locked() || st == STATE_ESTOP;
  float rpm = st == STATE_ESTOP ? 0.0f : moving ? view.estimated_rpm : view.target_rpm;
  float diameter = speed_get_workpiece_diameter_mm();
  if (diameter <= 0) diameter = D_EMNE * 1000.0f;
  char rpmText[16];
  ui_format_rpm(rpmText, sizeof(rpmText), rpm);
  bindings.rpm.set(rpmText);
  bindings.caption.set(moving ? "Estimated speed" : "Target speed");
  char text[40];
  const float maximum = speed_get_rpm_max();
  snprintf(text, sizeof(text), "Min %.3f", (double)speed_get_rpm_min()); bindings.minimum.set(text);
  snprintf(text, sizeof(text), "Max %.3f RPM", (double)maximum); bindings.maximum.set(text);
  const float ratio = maximum > 0 && std::isfinite(rpm) ? constrain(rpm / maximum, 0.0f, 1.0f) : 0;
  bindings.speed.set((int32_t)(1000.0f * ratio));
  snprintf(text, sizeof(text), "%.0f", (double)(rpm * diameter * 3.14159265f)); bindings.surface.set(text);
  snprintf(text, sizeof(text), "Dia %.0f mm", (double)diameter); bindings.diameter.set(text);
  update_direction(view, moving, blocked);
  bindings.title.set(st == STATE_PULSE      ? "Pulse rotation"
                               : st == STATE_STEP     ? "Angle move"
                               : st == STATE_JOG      ? "Jog"
                               : st == STATE_STOPPING ? "Stopping"
                                                      : "Continuous rotation");
  bindings.menuDisabled.set(moving);
  bindings.programsDisabled.set(moving);
  bindings.minusDisabled.set(moving || blocked);
  bindings.plusDisabled.set(moving || blocked);
  SpeedInputSource source = view.source;
  bindings.source.set(st == STATE_JOG                ? "Jog setting"
                                 : st == STATE_STEP             ? "Step setting"
                                 : source == SPEED_SOURCE_PEDAL ? "Pedal analog"
                                 : source == SPEED_SOURCE_UI    ? "Screen / program"
                                                                : "Panel dial");
  update_detail(moving, blocked, rangeBlocked);
  const bool showStop = moving || !ui_control_fresh();
  bindings.startHidden.set(showStop); bindings.stopHidden.set(!showStop);
  bindings.startDisabled.set(blocked);
  bindings.startText.set(blocked ? "Start blocked" : UI_LABEL_START);
}
void screen_main_invalidate_widgets() {
  bindings.reset();
  minimumLabel = nullptr;
  rpmLabel = speedCaption = sourceLabel = surfaceLabel = diameterLabel = limitLabel =
      detailCaption = detailValue = detailMessage = speedPointer = modeTitle = nullptr;
  startBtn = stopBtn = menuBtn = programsBtn = minusBtn = plusBtn = cwBtn = ccwBtn = cwIcon = ccwIcon = nullptr;
}
