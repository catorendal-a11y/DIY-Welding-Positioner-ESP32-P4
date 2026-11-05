// Commissioning reuses validated motor configuration and calibration screens.
#include "../screens.h"
#include "../theme.h"
#include "../../control/setup_policy.h"
#include "../../safety/safety.h"
#include "../../config.h"

static SetupProgress progress;
static bool active = false, cwTested = false, ccwTested = false;
static uint32_t finishTicket = 0;
static lv_obj_t *setupDetail = nullptr, *nextBtn = nullptr, *exitBtn = nullptr;
static lv_obj_t *cwBtn = nullptr, *ccwBtn = nullptr, *startBtn = nullptr, *stopBtn = nullptr, *driverBtn = nullptr;
static lv_obj_t *confirmBtn = nullptr, *flipBtn = nullptr;
static void enabled(lv_obj_t* obj, bool yes) {
  if (!obj) return;
  lv_obj_set_style_bg_color(obj, COL_BTN_BG, LV_STATE_DISABLED);
  lv_obj_set_style_border_color(obj, COL_BORDER, LV_STATE_DISABLED);
  auto label = lv_obj_get_child(obj, 0);
  if (label) lv_obj_set_style_text_color(label, !yes ? COL_TEXT_DIM : obj == nextBtn || obj == startBtn ? lv_color_hex(0x11191C) : COL_TEXT, 0);
  if (yes) lv_obj_set_disabled(obj, false); else lv_obj_set_disabled(obj, true);
}
void screen_setup_begin() {
  control_stop(); progress = SetupProgress{}; cwTested = ccwTested = false;
  finishTicket = 0; active = true; control_set_setup_active(true);
}
void screen_setup_leave(ScreenId destination) {
  if (!active || destination == SCREEN_SETUP || destination == SCREEN_MOTOR_CONFIG ||
      destination == SCREEN_CALIBRATION || destination == SCREEN_CONFIRM) return;
  control_stop(); speed_set_slider_priority(false); active = false; control_set_setup_active(false);
}
void screen_setup_return(bool, bool) {
  control_stop(); screens_request_show(active ? SCREEN_SETUP : SCREEN_SETTINGS);
}
void screen_setup_config_saved() {
  if (active && progress.stage == SetupStage::Motor) progress.motor_saved = true;
}
void screen_setup_calibration_saved() {
  if (active && progress.stage == SetupStage::Calibration) progress.calibration_saved = true;
}
static void exit_cb(lv_event_t*) { control_stop(); screens_request_show(SCREEN_SETTINGS); }
static void open_cb(lv_event_t* e) {
  control_stop(); screens_request_show((ScreenId)(intptr_t)lv_event_get_user_data(e));
}
static SystemSettings settings_copy() {
  SystemSettings s; xSemaphoreTake(g_settings_mutex, portMAX_DELAY); s = g_settings;
  xSemaphoreGive(g_settings_mutex); return s;
}
static void driver_cb(lv_event_t*) {
  auto s = settings_copy(); s.stepper_driver = s.stepper_driver == STEPPER_DRIVER_DM542T ? STEPPER_DRIVER_STANDARD : STEPPER_DRIVER_DM542T;
  if (control_apply_motor_settings(s)) progress.motor_saved = false;
}
static void flip_cb(lv_event_t*) {
  if (ui_control_state() != STATE_IDLE || !ui_control_fresh()) return;
  auto s = settings_copy(); s.invert_direction = !s.invert_direction;
  if (control_apply_motor_settings(s)) { cwTested = ccwTested = false; progress.direction_confirmed = false; }
}
static void hold_cb(lv_event_t* e) {
  const auto code = lv_event_get_code(e);
  const bool cw = (intptr_t)lv_event_get_user_data(e) == 0;
  if (code == LV_EVENT_PRESSED) {
    control_set_jog_speed(constrain(0.1f, MIN_RPM, speed_get_rpm_max()));
    if (cw) control_start_jog_cw(); else control_start_jog_ccw();
  } else if (code == LV_EVENT_PRESSING) {
    control_renew_jog();
    if (ui_control_state() == STATE_JOG) { if (cw) cwTested = true; else ccwTested = true; }
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) control_stop_jog();
}
static void confirm_direction_cb(lv_event_t*) {
  if (cwTested && ccwTested && ui_control_state() == STATE_IDLE && ui_control_fresh())
    progress.direction_confirmed = true;
}
static void start_cb(lv_event_t*) {
  if (progress.reset_seen && ui_control_fresh()) {
    speed_set_slider_priority(true); speed_slider_set(constrain(0.1f, MIN_RPM, speed_get_rpm_max()));
    progress.start_requested = control_start_continuous();
  }
}
static void stop_cb(lv_event_t*) { progress.stop_requested = true; control_stop(); }
static void next_cb(lv_event_t*) {
  if (progress.stage == SetupStage::Complete) { screens_request_show(SCREEN_MAIN); return; }
  if (!ui_control_fresh() || ui_control_state() != STATE_IDLE || safety_inhibit_motion()) {
    return;
  }
  if (!progress.advance()) return;
  control_stop();
  if (progress.stage == SetupStage::Saving) {
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY); g_settings.setup_completed = true;
    xSemaphoreGive(g_settings_mutex); finishTicket = storage_request_settings_save();
  }
  screens_request_show(SCREEN_SETUP);
}
void screen_setup_invalidate_widgets() {
  setupDetail = nextBtn = exitBtn = cwBtn = ccwBtn = startBtn = stopBtn = driverBtn = confirmBtn = flipBtn = nullptr;
}
void screen_setup_create() {
  if (!active) screen_setup_begin();
  screen_setup_invalidate_widgets();
  auto s = screenRoots[SCREEN_SETUP]; lv_obj_clean(s);
  ui_create_header(s, "Setup wizard", "", nullptr);
  const char* tabs[] = {"1  Motor", "2  Direction", "3  Calibration", "4  Check"};
  const int stage = (int)progress.stage;
  for (int i = 0; i < 4; ++i) {
    auto card = ui_create_post_card(s, 24 + i * 190, 94, 182, 46);
    const bool chosen = i == (stage > 3 ? 3 : stage);
    if (chosen) lv_obj_set_style_bg_color(card, COL_ACCENT, 0);
    ui_create_text(card, 12, 12, 158, tabs[i], FONT_NORMAL, chosen ? lv_color_hex(0x11191C) : COL_TEXT_DIM);
  }
  setupDetail = ui_create_text(s, 24, 156, 752, "", FONT_SUBTITLE, COL_TEXT);
  lv_label_set_long_mode(setupDetail, LV_LABEL_LONG_MODE_WRAP);
  if (progress.stage == SetupStage::Motor) {
    ui_create_text(s, 24, 240, 752, "Match microstep to the drive switches before movement.", FONT_NORMAL, COL_TEXT_DIM);
    auto gear = ui_create_text(s, 24, 270, 752, "", FONT_NORMAL, COL_TEXT_DIM);
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    const float gearRatio = g_settings.gear_ratio, rollerMm = g_settings.roller_mm;
    xSemaphoreGive(g_settings_mutex);
    lv_label_set_text_fmt(gear, "Gear ratio %.1f / roller %.0f mm / default workpiece %.0f mm",
                          (double)gearRatio, (double)rollerMm, (double)(D_EMNE * 1000));
    driverBtn = ui_create_btn(s, 24, 320, 300, 56, "", FONT_BTN, UI_BTN_NORMAL, driver_cb, nullptr);
    ui_create_btn(s, 344, 320, 432, 56, "Open motor config", FONT_BTN, UI_BTN_ACCENT, open_cb, (void*)(intptr_t)SCREEN_MOTOR_CONFIG);
  } else if (progress.stage == SetupStage::Direction) {
    cwBtn = ui_create_btn(s, 24, 224, 368, 68, "Hold CW", FONT_XL, UI_BTN_NORMAL, nullptr, nullptr);
    ccwBtn = ui_create_btn(s, 408, 224, 368, 68, "Hold CCW", FONT_XL, UI_BTN_NORMAL, nullptr, nullptr);
    for (auto button : {cwBtn, ccwBtn}) {
      lv_obj_t* icon = ui_create_rotation_icon(button, 34, COL_TEXT, button == cwBtn ? DIR_CW : DIR_CCW);
      lv_obj_align(icon, LV_ALIGN_CENTER, 90, 0);
    }
    for (auto code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST}) {
      lv_obj_add_event_cb(cwBtn, hold_cb, code, (void*)(intptr_t)0);
      lv_obj_add_event_cb(ccwBtn, hold_cb, code, (void*)(intptr_t)1);
    }
    flipBtn = ui_create_btn(s, 24, 312, 368, 56, "Flip direction", FONT_BTN, UI_BTN_NORMAL, flip_cb, nullptr);
    confirmBtn = ui_create_btn(s, 408, 312, 368, 56, "Direction correct", FONT_BTN, UI_BTN_NORMAL, confirm_direction_cb, nullptr);
  } else if (progress.stage == SetupStage::Calibration) {
    ui_create_text(s, 24, 245, 752, "Mark the workpiece. Measure rotation manually. Verify before saving.", FONT_NORMAL, COL_TEXT_DIM);
    ui_create_btn(s, 24, 312, 752, 56, "Open calibration", FONT_BTN, UI_BTN_ACCENT, open_cb, (void*)(intptr_t)SCREEN_CALIBRATION);
  } else if (progress.stage == SetupStage::Check) {
    ui_create_text(s, 24, 290, 752, "This records your function check; it does not measure physical stop time.", FONT_NORMAL, COL_TEXT_DIM);
    startBtn = ui_create_btn(s, 24, 328, 752, 56, LV_SYMBOL_PLAY "  Test start", FONT_BTN, UI_BTN_ACCENT, start_cb, nullptr);
    stopBtn = ui_create_btn(s, 24, 328, 752, 56, UI_LABEL_STOP, FONT_BTN, UI_BTN_DANGER, stop_cb, nullptr);
  }
  exitBtn = ui_create_btn(s, 24, 408, 180, 56, "Exit setup", FONT_BTN, UI_BTN_NORMAL, exit_cb, nullptr);
  nextBtn = ui_create_btn(s, 408, 408, 368, 56, stage >= 4 ? "Finish" : "Next", FONT_BTN, UI_BTN_ACCENT, next_cb, nullptr);
  screen_setup_update();
}
void screen_setup_update() {
  ui_mark_motion_callback(screenRoots[SCREEN_SETUP], start_cb);
  ui_mark_motion_callback(screenRoots[SCREEN_SETUP], hold_cb);
  if (!active) return;
  const bool fresh = ui_control_fresh(), idle = ui_control_state() == STATE_IDLE;
  const bool locked = safety_is_estop_locked() || safety_inhibit_motion();
  if (fresh) progress.observe(safety_is_estop_active(), locked, idle, ui_control_state() == STATE_RUNNING);
  if (progress.stage == SetupStage::Saving && storage_settings_save_status(finishTicket) == STORAGE_SAVED) {
    progress.stage = SetupStage::Complete;
  }
  if (!screens_is_active(SCREEN_SETUP) || !setupDetail) return;
  const bool configPending = control_config_status() == CONFIG_PENDING;
  const bool configSaved = control_config_status() != CONFIG_APPLIED ||
      storage_settings_save_status(control_config_save_ticket()) == STORAGE_SAVED;
  const bool ready = fresh && idle && !locked && !configPending && configSaved;
  const bool holdingJog = idle || ui_control_state() == STATE_JOG || ui_control_state() == STATE_ENABLING;
  enabled(cwBtn, fresh && !locked && !configPending && configSaved && holdingJog);
  enabled(ccwBtn, fresh && !locked && !configPending && configSaved && holdingJog);
  enabled(startBtn, ready && progress.reset_seen);
  if (startBtn && stopBtn) {
    const bool showStop = !fresh || (!idle && ui_control_state() != STATE_ESTOP);
    lv_obj_set_hidden(startBtn, showStop); lv_obj_set_hidden(stopBtn, !showStop);
  }
  enabled(flipBtn, ready);
  enabled(confirmBtn, ready && cwTested && ccwTested);
  enabled(driverBtn, ready);
  if (driverBtn) lv_label_set_text(lv_obj_get_child(driverBtn, 0), settings_copy().stepper_driver == STEPPER_DRIVER_DM542T ? "Driver: DM542T" : "Driver: standard");
  enabled(nextBtn, ready && (progress.ready() || progress.stage == SetupStage::Complete));
  enabled(exitBtn, progress.stage != SetupStage::Saving);
  const char* message = "";
  switch (progress.stage) {
    case SetupStage::Motor: message = progress.motor_saved ? "Motor settings saved. Continue with a direction check." : "Select the driver, then open Motor Config and Save & apply."; break;
    case SetupStage::Direction: message = progress.direction_confirmed ? "Direction confirmed. Continue with calibration." : "Hold both buttons and observe the workpiece. Flip if needed, test again, then confirm."; break;
    case SetupStage::Calibration: message = progress.calibration_saved ? "Verified calibration saved. Continue with the function check." : "Open Calibration: align, measure, verify, then save. Return here after storage confirms the result."; break;
    case SetupStage::Check: message = !progress.estop_seen ? "Press the physical E-STOP switch. The fault overlay must appear." : !progress.reset_seen ? "Release the physical switch, check the machine, then RESET TO IDLE." : !progress.stop_seen ? "Press Test start, observe rotation, then Stop. Confirm that the workpiece stops." : "E-STOP, reset and start/stop sequence observed. NEXT confirms your physical function check."; break;
    case SetupStage::Saving: message = storage_settings_save_status(finishTicket) == STORAGE_ERROR ? "Save failed. Retrying automatically; keep power on." : "Saving completed setup. Keep power on."; break;
    case SetupStage::Complete: message = "Setup saved. Select Finish to return to the operating screen."; break;
  }
  lv_label_set_text(setupDetail, fresh ? message : "Status unavailable. Motion cannot start; Stop remains available.");
}
