// Screen-owned motor settings draft. SAVE submits to the control owner.
#include "../screens.h"
#include "../theme.h"
#include "../input_panel.h"
#include "../value_binding.h"
#include "../../config.h"
#include "../../motor/microstep.h"
#include "../../control/motion_policy.h"
#include "../../storage/settings_policy.h"
#include "../../control/control.h"
#include "../../safety/safety.h"
#include "../../utils/numeric_input.h"
#include <cmath>
#include <cstdio>

static const MicrostepSetting microOptions[] = {MICROSTEP_4, MICROSTEP_8, MICROSTEP_16, MICROSTEP_32};
static const char* microLabels[] = {"800", "1600", "3200", "6400"};
static constexpr int kAccelMin = 1000, kAccelMax = 30000;
static constexpr int kMaxRpmMilliMin = 1, kMaxRpmMilliMax = 3000;
static int selectedMicro = 0, motorAccelUi = 10000, motorMaxRpmMilliUi = 3000;
static bool invertDir = false, dirSwitchEnabled = false, saveRequested = false;
enum EntryField : intptr_t { ENTRY_MAX_RPM, ENTRY_ACCEL, ENTRY_ROLLER, ENTRY_GEAR };
static EntryField entryField = ENTRY_MAX_RPM;
static bool entryClosePending = false;
static float motorRollerUi = D_RULLE * 1000.0f, motorGearUi = GEAR_RATIO;
static lv_obj_t *rollerButton = nullptr, *gearButton = nullptr;
static lv_obj_t *microBtns[4]{}, *accelSlider = nullptr, *maxRpmSlider = nullptr;
static lv_obj_t *accelButton = nullptr, *maxRpmButton = nullptr;
static lv_obj_t *invertToggle = nullptr, *dirToggle = nullptr, *saveButton = nullptr;
static UiInputPanel entry;
static UiTextBinding<24> accelValue, maxRpmValue, invertValue, dirValue, rollerValue, gearValue;
static UiTextBinding<48> stopAngleValue;
static UiTextBinding<48> motorStatus;
static UiTextBinding<96> saveFeedback;

static bool can_edit() {
  return !saveRequested && ui_control_fresh() && ui_control_state() == STATE_IDLE && !safety_inhibit_motion();
}
static void changed() { saveFeedback.set("Unsaved changes"); }
static void sync_stop_angle() {
  // Stop distance does not depend on microstepping: both rate and setting are per 1/16.
  const float rpm = motorMaxRpmMilliUi / 1000.0f;
  const float hzAt16 = rpmToStepHz(rpm) * 16.0f / (microstep_get_steps_per_rev() / 200.0f);
  char text[48];
  snprintf(text, sizeof(text), "Stop %.0f\xC2\xB0 at max", (double)motion_stop_angle_deg(rpm, hzAt16, motorAccelUi));
  stopAngleValue.set(text);
}
static void sync_mechanics() {
  char text[24];
  snprintf(text, sizeof(text), "Roller %.1f mm", (double)motorRollerUi); rollerValue.set(text);
  snprintf(text, sizeof(text), "Gear %.1f:1", (double)motorGearUi); gearValue.set(text);
}
static void sync_acceleration(int value) {
  motorAccelUi = constrain(value, kAccelMin, kAccelMax);
  char text[24]; snprintf(text, sizeof(text), "%d", motorAccelUi); accelValue.set(text);
  if (accelSlider && lv_slider_get_value(accelSlider) != motorAccelUi)
    lv_slider_set_value(accelSlider, motorAccelUi, LV_ANIM_OFF);
  sync_stop_angle();
}
static void sync_rpm(int value) {
  motorMaxRpmMilliUi = constrain(value, kMaxRpmMilliMin, kMaxRpmMilliMax);
  char text[24]; snprintf(text, sizeof(text), "%.3f", motorMaxRpmMilliUi / 1000.0); maxRpmValue.set(text);
  if (maxRpmSlider && lv_slider_get_value(maxRpmSlider) != motorMaxRpmMilliUi)
    lv_slider_set_value(maxRpmSlider, motorMaxRpmMilliUi, LV_ANIM_OFF);
  sync_stop_angle();
}
static void sync_toggles() {
  lv_obj_set_checked(invertToggle, invertDir);
  lv_obj_set_checked(dirToggle, dirSwitchEnabled);
  invertValue.set(invertDir ? "On" : "Off"); dirValue.set(dirSwitchEnabled ? "On" : "Off");
}
static void back_cb(lv_event_t*) { screen_setup_return(); }
static void micro_cb(lv_event_t* event) {
  if (!can_edit()) return;
  selectedMicro = (intptr_t)lv_event_get_user_data(event);
  for (int i=0; i<4; ++i) {
    const auto style = i == selectedMicro ? UI_BTN_ACCENT : UI_BTN_NORMAL;
    ui_btn_style_post(microBtns[i], style);
    lv_obj_set_checked(microBtns[i], i == selectedMicro);
    lv_obj_set_style_text_color(lv_obj_get_child(microBtns[i], 0), ui_btn_label_color_post(style), 0);
    lv_obj_set_style_bg_color(microBtns[i], COL_ACCENT, LV_STATE_CHECKED);
    lv_obj_set_style_recolor_opa(microBtns[i], LV_OPA_TRANSP, LV_STATE_CHECKED);
  }
  changed();
}
static void adjust_cb(lv_event_t* event) {
  if (!can_edit()) return;
  const int amount = (intptr_t)lv_event_get_user_data(event);
  if (std::abs(amount) == 1) sync_rpm(motorMaxRpmMilliUi + amount);
  else sync_acceleration(motorAccelUi + amount);
  changed();
}
static void slider_cb(lv_event_t* event) {
  if (!can_edit()) return;
  auto slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (slider == accelSlider) sync_acceleration(lv_slider_get_value(slider));
  else sync_rpm(lv_slider_get_value(slider));
  changed();
}
static void toggle_cb(lv_event_t* event) {
  if (!can_edit()) return;
  if ((intptr_t)lv_event_get_user_data(event)) invertDir = !invertDir;
  else dirSwitchEnabled = !dirSwitchEnabled;
  sync_toggles(); changed();
}
static void input_cb(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_CANCEL) { entryClosePending = true; return; }
  if (lv_event_get_code(event) != LV_EVENT_READY || !entry.active()) return;
  if (!can_edit()) { entry.error("Stop the motor and restore fresh status before confirming."); return; }
  float value = 0;
  if (!parse_float_entry(entry.text(), value)) { entry.error("Enter one complete number. Use a dot or comma for decimals."); return; }
  if (entryField == ENTRY_ROLLER || entryField == ENTRY_GEAR) {
    const float roller = entryField == ENTRY_ROLLER ? value : motorRollerUi;
    const float gear = entryField == ENTRY_GEAR ? value : motorGearUi;
    if (!settings_mechanics_valid(roller, gear)) {
      entry.error(entryField == ENTRY_ROLLER ? "Enter 10 to 1000 mm." : "Enter a ratio from 1 to 1000."); return;
    }
    motorRollerUi = roller; motorGearUi = gear; sync_mechanics();
  } else if (entryField == ENTRY_ACCEL) {
    if (value < kAccelMin || value > kAccelMax || std::floor(value) != value) {
      entry.error("Enter a whole number from 1000 to 30000 steps/s2."); return;
    }
    sync_acceleration(static_cast<int>(value));
  } else {
    const double milli = static_cast<double>(value) * 1000;
    const double rounded = std::round(milli);
    if (value < MIN_RPM || value > MAX_RPM || std::fabs(milli-rounded) > 0.0002) {
      entry.error("Enter 0.001 to 3.000 RPM, with at most three decimal places."); return;
    }
    sync_rpm(static_cast<int>(rounded));
  }
  changed(); entryClosePending = true;
}
static void open_input_cb(lv_event_t* event) {
  if (!can_edit() || entry.active()) return;
  entryField = static_cast<EntryField>((intptr_t)lv_event_get_user_data(event));
  char value[24];
  const char* title = "Maximum workpiece speed";
  const char* hint = "0.001 to 3.000 RPM. Save & apply commits the draft.";
  if (entryField == ENTRY_ACCEL) {
    snprintf(value, sizeof(value), "%d", motorAccelUi);
    title = "Motor acceleration"; hint = "Whole steps/s2 at 1/16: 1000 to 30000. Save & apply commits the draft.";
  } else if (entryField == ENTRY_ROLLER) {
    snprintf(value, sizeof(value), "%.1f", (double)motorRollerUi);
    title = "Drive roller diameter"; hint = "10 to 1000 mm. Save & apply commits the draft.";
  } else if (entryField == ENTRY_GEAR) {
    snprintf(value, sizeof(value), "%.1f", (double)motorGearUi);
    title = "Gear ratio"; hint = "Motor turns per roller turn, 1 to 1000. Save & apply commits the draft.";
  } else {
    snprintf(value, sizeof(value), "%.3f", motorMaxRpmMilliUi/1000.0);
  }
  entry.open(title, hint, value, "0123456789.,", 12, LV_KEYBOARD_MODE_NUMBER, input_cb);
}
static void save_cb(lv_event_t*) {
  if (!can_edit() || entry.active()) return;
  SystemSettings request;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); request = g_settings;
  xSemaphoreGive(g_settings_mutex);
  request.microstep = microOptions[selectedMicro]; request.acceleration = motorAccelUi;
  request.max_rpm = motorMaxRpmMilliUi/1000.0f;
  request.roller_mm = motorRollerUi; request.gear_ratio = motorGearUi;
  request.invert_direction = invertDir; request.dir_switch_enabled = dirSwitchEnabled;
  if (!control_apply_motor_settings(request)) { saveFeedback.set("Apply blocked / try when idle"); return; }
  saveRequested = true; saveFeedback.set("Apply queued"); screen_motor_config_update();
}
static void reset_bindings() {
  accelValue.reset(); maxRpmValue.reset(); invertValue.reset(); dirValue.reset();
  motorStatus.reset(); saveFeedback.reset();
  rollerValue.reset(); gearValue.reset(); stopAngleValue.reset();
}
void screen_motor_config_leave() { entry.close(); entryClosePending = false; }
void screen_motor_config_invalidate_widgets() {
  screen_motor_config_leave(); reset_bindings();
  for (auto& button : microBtns) button = nullptr;
  accelSlider = maxRpmSlider = accelButton = maxRpmButton = invertToggle = dirToggle = saveButton = nullptr;
  rollerButton = gearButton = nullptr;
}
void screen_motor_config_create() {
  screen_motor_config_invalidate_widgets(); saveRequested = false;
  auto root = screenRoots[SCREEN_MOTOR_CONFIG]; lv_obj_clean(root);
  SystemSettings settings;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); settings = g_settings;
  xSemaphoreGive(g_settings_mutex);
  motorAccelUi = constrain(static_cast<int>(settings.acceleration), kAccelMin, kAccelMax);
  motorMaxRpmMilliUi = constrain(static_cast<int>(std::round(settings.max_rpm * 1000)), kMaxRpmMilliMin, kMaxRpmMilliMax);
  invertDir = settings.invert_direction; dirSwitchEnabled = settings.dir_switch_enabled;
  motorRollerUi = settings.roller_mm; motorGearUi = settings.gear_ratio;
  selectedMicro = 0;
  for (int i=0; i<4; ++i) if (microOptions[i] == settings.microstep) selectedMicro = i;
  ui_create_header(root, "Motor configuration", "", nullptr);
  auto micro = ui_create_post_card(root, 20, 94, 760, 64);
  ui_create_text(micro, 14, 10, 260, "Microstep (steps per rev)", FONT_SMALL, COL_TEXT_DIM);
  ui_create_text(micro, 14, 34, 260, "Match the driver DIP switches", FONT_NORMAL, COL_TEXT);
  for (int i=0; i<4; ++i) {
    microBtns[i] = ui_create_btn(micro, 300+i*110, 10, 100, 44, microLabels[i], FONT_BTN,
                                 i == selectedMicro ? UI_BTN_ACCENT : UI_BTN_NORMAL, micro_cb, (void*)(intptr_t)i);
    lv_obj_set_checked(microBtns[i], i == selectedMicro);
    lv_obj_set_style_bg_color(microBtns[i], COL_ACCENT, LV_STATE_CHECKED);
    lv_obj_set_style_recolor_opa(microBtns[i], LV_OPA_TRANSP, LV_STATE_CHECKED);
  }
  auto speed = ui_create_post_card(root, 20, 170, 368, 130);
  ui_create_text(speed, 14, 12, 340, "Max speed (RPM)", FONT_SMALL, COL_TEXT_DIM);
  maxRpmButton = ui_create_btn(speed, 14, 34, 178, 48, "", FONT_XL, UI_BTN_NORMAL, open_input_cb, (void*)ENTRY_MAX_RPM);
  maxRpmValue.bind(lv_obj_get_child(maxRpmButton, 0));
  lv_obj_set_size(ui_create_pm_btn(speed, 204, 34, "-", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)-1), 66, 48);
  lv_obj_set_size(ui_create_pm_btn(speed, 278, 34, "+", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)1), 76, 48);
  maxRpmSlider = lv_slider_create(speed); lv_slider_set_range(maxRpmSlider, kMaxRpmMilliMin, kMaxRpmMilliMax);
  lv_obj_set_pos(maxRpmSlider, 20, 101); lv_obj_set_size(maxRpmSlider, 328, 12);
  ui_style_slider(maxRpmSlider); lv_obj_add_event_cb(maxRpmSlider, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  auto acceleration = ui_create_post_card(root, 404, 170, 376, 130);
  ui_create_text(acceleration, 14, 12, 150, "Accel at 1/16", FONT_SMALL, COL_TEXT_DIM);
  auto stopAngle = ui_create_text(acceleration, 170, 12, 192, "", FONT_SMALL, COL_TEXT_DIM);
  lv_obj_set_style_text_align(stopAngle, LV_TEXT_ALIGN_RIGHT, 0);
  stopAngleValue.bind(stopAngle);
  accelButton = ui_create_btn(acceleration, 14, 34, 178, 48, "", FONT_XL, UI_BTN_NORMAL, open_input_cb, (void*)ENTRY_ACCEL);
  accelValue.bind(lv_obj_get_child(accelButton, 0));
  lv_obj_set_size(ui_create_pm_btn(acceleration, 204, 34, "-", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)-500), 66, 48);
  lv_obj_set_size(ui_create_pm_btn(acceleration, 278, 34, "+", FONT_XL, UI_BTN_NORMAL, adjust_cb, (void*)500), 84, 48);
  accelSlider = lv_slider_create(acceleration); lv_slider_set_range(accelSlider, kAccelMin, kAccelMax);
  lv_obj_set_pos(accelSlider, 20, 101); lv_obj_set_size(accelSlider, 336, 12);
  ui_style_slider(accelSlider); lv_obj_add_event_cb(accelSlider, slider_cb, LV_EVENT_VALUE_CHANGED, nullptr);
  auto invert = ui_create_post_card(root, 20, 312, 368, 64);
  ui_create_text(invert, 14, 12, 238, "Invert direction", FONT_NORMAL, COL_TEXT);
  ui_create_text(invert, 14, 36, 238, "Reverse the motor output", FONT_SMALL, COL_TEXT_DIM);
  invertToggle = ui_create_btn(invert, 270, 8, 84, 48, "", FONT_BTN, UI_BTN_NORMAL, toggle_cb, (void*)1);
  invertValue.bind(lv_obj_get_child(invertToggle, 0));
  auto direction = ui_create_post_card(root, 404, 312, 376, 64);
  ui_create_text(direction, 14, 12, 246, "Physical direction switch", FONT_NORMAL, COL_TEXT);
  ui_create_text(direction, 14, 36, 246, "Use external CW / CCW", FONT_SMALL, COL_TEXT_DIM);
  dirToggle = ui_create_btn(direction, 278, 8, 84, 48, "", FONT_BTN, UI_BTN_NORMAL, toggle_cb, nullptr);
  dirValue.bind(lv_obj_get_child(dirToggle, 0));
  for (auto toggle : {invertToggle, dirToggle}) {
    lv_obj_set_style_border_color(toggle, COL_ACCENT, LV_STATE_CHECKED);
    lv_obj_set_style_border_width(toggle, 2, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(toggle, COL_BG_ACTIVE, LV_STATE_CHECKED);
    lv_obj_set_style_recolor_opa(toggle, LV_OPA_TRANSP, LV_STATE_CHECKED);
  }
  motorStatus.bind(ui_create_text(root, 24, 385, 220, "", FONT_SMALL, COL_TEXT_DIM));
  auto feedback = ui_create_text(root, 254, 385, 522, "", FONT_SMALL, COL_TEXT_DIM);
  lv_obj_set_style_text_align(feedback, LV_TEXT_ALIGN_RIGHT, 0); saveFeedback.bind(feedback);
  ui_create_btn(root, 20, 408, 130, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_cb, nullptr);
  rollerButton = ui_create_btn(root, 162, 408, 190, 56, "", FONT_BTN, UI_BTN_NORMAL, open_input_cb, (void*)ENTRY_ROLLER);
  rollerValue.bind(lv_obj_get_child(rollerButton, 0));
  gearButton = ui_create_btn(root, 364, 408, 170, 56, "", FONT_BTN, UI_BTN_NORMAL, open_input_cb, (void*)ENTRY_GEAR);
  gearValue.bind(lv_obj_get_child(gearButton, 0));
  saveButton = ui_create_btn(root, 546, 408, 234, 56, "Save & apply", FONT_BTN, UI_BTN_ACCENT, save_cb, nullptr);
  sync_rpm(motorMaxRpmMilliUi); sync_acceleration(motorAccelUi); sync_toggles(); sync_mechanics();
  saveFeedback.set("Tap a value for exact entry"); screen_motor_config_update();
}
static void lock_edits(lv_obj_t* root, bool lock) {
  if (!root) return;
  if (lv_obj_check_type(root, &lv_button_class) || lv_obj_check_type(root, &lv_slider_class)) {
    // Navigation remains available during failed storage writes.
    if (lv_obj_get_parent(root) != screenRoots[SCREEN_MOTOR_CONFIG] || root == saveButton ||
        root == rollerButton || root == gearButton)
      lv_obj_set_disabled(root, lock);
  }
  for (uint32_t i=0; i<lv_obj_get_child_count(root); ++i) lock_edits(lv_obj_get_child(root, i), lock);
}
void screen_motor_config_update() {
  if (entryClosePending) screen_motor_config_leave();
  if (!saveButton) return;
  if (saveRequested) {
    const auto applied = control_config_status();
    if (applied == CONFIG_CANCELLED) { saveRequested = false; saveFeedback.set("Apply cancelled / edit and retry"); }
    else if (applied == CONFIG_PENDING) saveFeedback.set("Apply queued");
    else {
      const auto status = storage_settings_save_status(control_config_save_ticket());
      if (status == STORAGE_SAVED) {
        saveRequested = false; screen_setup_config_saved(); saveFeedback.set("Saved");
      } else saveFeedback.set(status == STORAGE_ERROR ? "Save failed, retrying" : "Saving...");
    }
  }
  if (!ui_control_fresh()) motorStatus.set("Status stale");
  else motorStatus.set("");
  lock_edits(screenRoots[SCREEN_MOTOR_CONFIG], !can_edit());
}
