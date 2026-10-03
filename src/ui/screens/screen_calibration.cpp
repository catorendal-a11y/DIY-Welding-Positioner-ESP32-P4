// Guided manual workpiece calibration. Measurements require a completed move.
#include "../screens.h"
#include "../theme.h"
#include "../../motor/calibration.h"
#include "../../motor/calibration_session.h"
#include "../../motor/motor.h"
#include "../../motor/speed.h"
#include "../../safety/safety.h"
#include "../../storage/storage.h"
#include "../../config.h"
#include <cstdio>
#include <cmath>

static CalibrationSession session;
static lv_obj_t *stageBtns[4]{}, *hint = nullptr, *title = nullptr;
static lv_obj_t *measurement = nullptr, *measurementCaption = nullptr, *applyBtn = nullptr;
static lv_obj_t *runBtn = nullptr, *stopBtn = nullptr, *saveBtn = nullptr, *restartBtn = nullptr, *backBtn = nullptr;
static lv_obj_t *jogMinus = nullptr, *jogPlus = nullptr, *factorValue = nullptr, *context = nullptr;
static lv_obj_t *diameterBtn = nullptr, *progress = nullptr;
static lv_obj_t *entryPanel = nullptr, *entryField = nullptr, *entryKeyboard = nullptr, *entryError = nullptr;
static bool entryClosePending = false, editingDiameter = false;
static uint32_t saveTicket = 0;

static void enabled(lv_obj_t* obj, bool yes) {
  lv_obj_set_style_bg_color(obj, COL_BG_INPUT, LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(obj, LV_OPA_50, LV_STATE_DISABLED);
  lv_obj_set_disabled(obj, !yes);
  auto text = lv_obj_get_child(obj, 0);
  if (text) lv_obj_set_style_text_color(text, yes ? ui_btn_label_color_post(obj == runBtn || obj == saveBtn || obj == applyBtn ? UI_BTN_ACCENT : UI_BTN_NORMAL) : COL_TEXT_VDIM, 0);
}
static bool safe_idle() { return ui_control_fresh() && ui_control_state() == STATE_IDLE && !safety_inhibit_motion(); }
static uint8_t microstep() {
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); const auto value = g_settings.microstep;
  xSemaphoreGive(g_settings_mutex); return value;
}
static float workpiece_diameter() {
  const float value = speed_get_workpiece_diameter_mm();
  return value >= 1 ? value : D_EMNE * 1000;
}
static bool context_matches() {
  return speed_get_direction() == (session.direction_cw ? DIR_CW : DIR_CCW) &&
         microstep() == session.microstep && std::fabs(workpiece_diameter() - session.diameter) < 0.01f &&
         std::fabs(calibration_get_factor() - session.factor) < 0.000001f;
}
static void close_entry() {
  if (entryPanel) {
    lv_keyboard_set_textarea(entryKeyboard, nullptr);
    lv_obj_delete_async(entryPanel);
  }
  entryPanel = entryField = entryKeyboard = entryError = nullptr; entryClosePending = false;
}
void screen_calibration_enter() {
  control_set_calibration_active(true);
  calibration_discard_draft();
  session.reset(calibration_get_saved_factor(), workpiece_diameter()); saveTicket = 0;
  screen_calibration_update();
}
void screen_calibration_leave() {
  control_set_calibration_active(false);
  close_entry(); control_stop(); calibration_discard_draft();
  session.abort("Calibration closed. Unsaved correction discarded.");
}
static void back_cb(lv_event_t*) {
  if (session.stage != CalibrationSession::Saving) screen_setup_return(true);
}
static void restart_cb(lv_event_t*) {
  if (!safe_idle() || session.stage == CalibrationSession::Saving) return;
  close_entry(); calibration_discard_draft(); calibration_process_pending();
  session.reset(calibration_get_factor(), workpiece_diameter()); saveTicket = 0;
  screen_calibration_update();
}
static void stop_cb(lv_event_t*) {
  if (session.stage != CalibrationSession::Saving && session.stage != CalibrationSession::Saved)
    session.abort("Move stopped. Re-align the mark and restart.");
  control_stop(); screen_calibration_update();
}
static void move_cb(lv_event_t*) {
  if (!safe_idle() || entryPanel) return;
  if (session.stage != CalibrationSession::Prepare && !context_matches()) {
    session.abort("Machine settings changed. Restart calibration."); screen_calibration_update(); return;
  }
  const float rpm = constrain(0.25f, MIN_RPM, speed_get_rpm_max());
  speed_set_slider_priority(true); speed_slider_set(rpm);
  const long steps = labs(angleToSteps(360));
  const float hz = motor_milli_hz_for_rpm_calibrated(rpm) / 1000.0f;
  const uint32_t budget = uint32_t((steps / (hz > 0 ? hz : 1) + 30) * 1000);
  if (!session.begin(millis(), budget, ui_control_view().completed_steps,
                     speed_get_direction() == DIR_CW, microstep())) return;
  if (!control_start_step(360)) session.abort("Start rejected. Check the safety inputs.");
  screen_calibration_update();
}
static void jog_cb(lv_event_t* e) {
  const auto code = lv_event_get_code(e);
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) { control_stop_jog(); return; }
  if (session.stage != CalibrationSession::Prepare || entryPanel) return;
  if (code == LV_EVENT_PRESSING) control_renew_jog();
  else if (code == LV_EVENT_PRESSED && safe_idle()) {
    control_set_jog_speed(constrain(0.1f, MIN_RPM, speed_get_rpm_max()));
    if ((intptr_t)lv_event_get_user_data(e) > 0) control_start_jog_cw(); else control_start_jog_ccw();
  }
}
static void apply_cb(lv_event_t*) {
  if (!safe_idle() || !context_matches()) return;
  if (session.apply()) calibration_set_factor(session.factor);
  screen_calibration_update();
}
static void save_cb(lv_event_t*) {
  if (!safe_idle() || !session.passed() || !context_matches()) return;
  saveTicket = calibration_save(); session.stage = CalibrationSession::Saving; screen_calibration_update();
}
static lv_obj_t* label(lv_obj_t* parent, int x, int y, int w, const char* text, const lv_font_t* font, lv_color_t color) {
  auto obj = lv_label_create(parent); lv_label_set_text(obj, text); lv_obj_set_pos(obj, x, y); lv_obj_set_width(obj, w);
  lv_obj_set_style_text_font(obj, font, 0); lv_obj_set_style_text_color(obj, color, 0);
  lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_WRAP); return obj;
}
static void entry_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CANCEL) { entryClosePending = true; return; }
  if (lv_event_get_code(e) != LV_EVENT_READY) return;
  float value = 0;
  bool valid = safe_idle() && calibration_parse_angle(lv_textarea_get_text(entryField), value);
  if (editingDiameter) {
    valid = valid && session.stage == CalibrationSession::Prepare && value >= 1 && value <= 2000;
    if (valid) { speed_set_workpiece_diameter_mm(value); session.diameter = value; }
  } else valid = valid && context_matches() && session.measurement(value);
  if (!valid) { lv_label_set_text(entryError, editingDiameter ? "Enter a diameter from 1 to 2000 mm." : "Enter an angle from 0.5 to 720 degrees."); return; }
  entryClosePending = true;
}
static void open_entry(lv_event_t* e) {
  if (!safe_idle() || entryPanel) return;
  editingDiameter = (intptr_t)lv_event_get_user_data(e) == 1;
  if (editingDiameter ? session.stage != CalibrationSession::Prepare :
      (session.stage != CalibrationSession::Measure && session.stage != CalibrationSession::VerifyMeasure)) return;
  entryPanel = lv_obj_create(lv_layer_top()); lv_obj_set_size(entryPanel, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(entryPanel, 0, 0); lv_obj_set_style_pad_all(entryPanel, 0, 0);
  lv_obj_set_style_bg_color(entryPanel, COL_BG, 0); lv_obj_set_style_bg_opa(entryPanel, LV_OPA_COVER, 0);
  lv_obj_set_scrollable(entryPanel, false);
  label(entryPanel, 24, 24, 750, editingDiameter ? "Workpiece diameter" : "Measured workpiece angle", FONT_XL, COL_TEXT);
  label(entryPanel, 24, 70, 750, editingDiameter ? "Enter the outside diameter in mm." : "Enter the total actual rotation, including any overshoot.", FONT_SUBTITLE, COL_TEXT_DIM);
  entryField = lv_textarea_create(entryPanel); lv_obj_set_pos(entryField, 24, 112); lv_obj_set_size(entryField, 752, 54);
  lv_textarea_set_one_line(entryField, true); lv_textarea_set_max_length(entryField, 12);
  lv_textarea_set_accepted_chars(entryField, "0123456789.,");
  lv_obj_set_style_text_font(entryField, FONT_XL, 0);
  if (editingDiameter) { char text[24]; snprintf(text, sizeof(text), "%.1f", (double)session.diameter); lv_textarea_set_text(entryField, text); }
  entryError = label(entryPanel, 24, 180, 752, "", FONT_SUBTITLE, COL_RED);
  lv_label_set_max_lines(entryError, 2);
  entryKeyboard = lv_keyboard_create(entryPanel); lv_keyboard_set_mode(entryKeyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(entryKeyboard, entryField); lv_obj_set_size(entryKeyboard, 800, 236);
  lv_obj_align(entryKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(entryKeyboard, entry_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(entryKeyboard, entry_cb, LV_EVENT_CANCEL, nullptr);
}
void screen_calibration_create() {
  auto root = screenRoots[SCREEN_CALIBRATION]; lv_obj_clean(root);
  lv_obj_set_style_bg_color(root, COL_BG, 0); lv_obj_set_scrollable(root, false);
  ui_create_settings_header(root, "Calibration", "WORKPIECE", COL_ACCENT);
  for (int i=0; i<4; ++i) {
    const char* names[] = {"1 / ALIGN", "2 / MEASURE", "3 / VERIFY", "4 / SAVE"};
    stageBtns[i] = ui_create_btn(root, 20+i*193, 86, 181, 40, names[i], FONT_SUBTITLE, UI_BTN_NORMAL, nullptr, nullptr);
    lv_obj_set_clickable(stageBtns[i], false);
  }
  auto card = ui_create_post_card(root, 20, 140, 552, 188);
  title = label(card, 16, 12, 520, "Align the reference mark", FONT_XL, COL_TEXT);
  hint = label(card, 16, 48, 520, "", FONT_SUBTITLE, COL_TEXT_DIM); lv_label_set_max_lines(hint, 3);
  measurementCaption = label(card, 16, 110, 220, "ACTUAL ANGLE / DEG", FONT_SUBTITLE, COL_TEXT_DIM);
  measurement = ui_create_btn(card, 16, 134, 226, 44, "---", FONT_XL, UI_BTN_NORMAL, open_entry, nullptr);
  applyBtn = ui_create_btn(card, 258, 134, 278, 44, "APPLY MEASUREMENT", FONT_SUBTITLE, UI_BTN_ACCENT, apply_cb, nullptr);
  diameterBtn = ui_create_btn(card, 16, 122, 226, 54, "300 mm", FONT_XL, UI_BTN_NORMAL, open_entry, (void*)1);
  progress = label(card, 16, 122, 520, "", FONT_XL, COL_ACCENT);
  auto summary = ui_create_post_card(root, 588, 140, 192, 188);
  label(summary, 14, 12, 162, "CORRECTION", FONT_SUBTITLE, COL_TEXT_DIM);
  factorValue = label(summary, 14, 42, 164, "1.0000", FONT_XL, COL_ACCENT);
  context = label(summary, 14, 82, 164, "", FONT_SMALL, COL_TEXT_DIM); lv_label_set_max_lines(context, 4);
  runBtn = ui_create_btn(root, 20, 342, 256, 50, "MOVE 360", FONT_SUBTITLE, UI_BTN_ACCENT, move_cb, nullptr);
  stopBtn = ui_create_btn(root, 288, 342, 192, 50, "STOP", FONT_SUBTITLE, UI_BTN_DANGER, stop_cb, nullptr);
  jogMinus = ui_create_btn(root, 492, 342, 138, 50, "JOG -", FONT_SUBTITLE, UI_BTN_NORMAL, jog_cb, (void*)-1);
  jogPlus = ui_create_btn(root, 642, 342, 138, 50, "JOG +", FONT_SUBTITLE, UI_BTN_NORMAL, jog_cb, (void*)1);
  // Bind hold direction explicitly; ui_create_btn user data belongs to its event.
  lv_obj_remove_event_cb(jogMinus, jog_cb); lv_obj_remove_event_cb(jogPlus, jog_cb);
  lv_obj_add_event_cb(jogMinus, jog_cb, LV_EVENT_ALL, (void*)-1); lv_obj_add_event_cb(jogPlus, jog_cb, LV_EVENT_ALL, (void*)1);
  backBtn = ui_create_btn(root, 20, 408, 152, 54, "<  BACK", FONT_SUBTITLE, UI_BTN_NORMAL, back_cb, nullptr);
  restartBtn = ui_create_btn(root, 188, 408, 192, 54, "RESTART", FONT_SUBTITLE, UI_BTN_NORMAL, restart_cb, nullptr);
  saveBtn = ui_create_btn(root, 396, 408, 384, 54, "SAVE CALIBRATION", FONT_SUBTITLE, UI_BTN_ACCENT, save_cb, nullptr);
  session.reset(calibration_get_factor(), workpiece_diameter()); saveTicket = 0; screen_calibration_update();
}
void screen_calibration_invalidate_widgets() {
  close_entry();
  for (auto& obj : stageBtns) obj = nullptr;
  hint = title = measurement = measurementCaption = applyBtn = runBtn = stopBtn = saveBtn = restartBtn = nullptr;
  jogMinus = jogPlus = factorValue = context = diameterBtn = progress = nullptr;
  backBtn = nullptr;
}
void screen_calibration_update() {
  if (!title) return;
  if (entryClosePending) close_entry();
  const bool wasMoving = session.moving();
  const auto& view = ui_control_view();
  session.observe(millis(), view.state == STATE_IDLE, safety_inhibit_motion() || !ui_control_fresh(),
                  view.completed_steps, speed_get_direction() == DIR_CW, microstep());
  if (wasMoving && !session.moving() && session.error) control_stop();
  if (session.stage == CalibrationSession::Saving) {
    auto status = storage_settings_save_status(saveTicket);
    if (status == STORAGE_SAVED) { session.stage = CalibrationSession::Saved; screen_setup_calibration_saved(); }
  }
  const int stage = session.stage <= CalibrationSession::Moving ? 0 : session.stage == CalibrationSession::Measure ? 1 :
                    session.stage <= CalibrationSession::VerifyMeasure || (session.stage == CalibrationSession::Result && !session.passed()) ? 2 : 3;
  for (int i=0; i<4; ++i) ui_btn_style_post(stageBtns[i], i == stage ? UI_BTN_ACCENT : UI_BTN_NORMAL);
  const char* heading = "Align the reference mark";
  const char* instructions = "Set diameter below. Use JOG to align a clear mark. Then run one full workpiece revolution.";
  if (session.moving()) { heading = session.stage == CalibrationSession::Verifying ? "Verification in progress" : "Rotation in progress"; instructions = "Wait for the complete revolution. STOP cancels this measurement."; }
  else if (session.stage == CalibrationSession::Measure) { heading = "Enter the actual rotation"; instructions = "Measure from the reference mark. Enter the total angle, then apply the correction."; }
  else if (session.stage == CalibrationSession::VerifyReady) { heading = "Verify the correction"; instructions = "Mark the current start position. Run another full revolution in the same direction."; }
  else if (session.stage == CalibrationSession::VerifyMeasure) { heading = "Measure the verification move"; instructions = "Enter the total actual angle again. Save is available within 360 +/- 0.5 degrees."; }
  else if (session.stage == CalibrationSession::Result) { heading = session.passed() ? "Verification passed" : "Verification needs another try"; instructions = session.passed() ? "The correction is verified. SAVE CALIBRATION stores it permanently." : "Outside tolerance. Repeat verification or restart to take a new measurement."; }
  else if (session.stage == CalibrationSession::Saving) { heading = "Saving calibration"; instructions = storage_settings_save_status(saveTicket) == STORAGE_ERROR ? "Write failed. Retrying automatically; keep power on." : "Waiting for storage confirmation. Keep power on."; }
  else if (session.stage == CalibrationSession::Saved) { heading = "Calibration saved"; instructions = "The verified correction is stored. Return to setup or normal operation."; }
  lv_label_set_text(title, heading); lv_label_set_text(hint, session.error ? session.error : instructions);
  lv_obj_set_style_text_color(hint, session.error ? COL_RED : COL_TEXT_DIM, 0);
  const bool measuring = session.stage == CalibrationSession::Measure || session.stage == CalibrationSession::VerifyMeasure;
  lv_obj_set_hidden(measurement, !measuring); lv_obj_set_hidden(measurementCaption, !measuring);
  lv_obj_set_hidden(applyBtn, session.stage != CalibrationSession::Measure);
  lv_obj_set_hidden(diameterBtn, session.stage != CalibrationSession::Prepare);
  lv_obj_set_hidden(progress, !session.moving() && session.stage != CalibrationSession::Result);
  char text[96];
  const float angle = session.stage == CalibrationSession::VerifyMeasure ? session.verified : session.measured;
  if (angle > 0) snprintf(text, sizeof(text), "%.2f", (double)angle); else snprintf(text, sizeof(text), "---");
  lv_label_set_text(lv_obj_get_child(measurement, 0), text);
  snprintf(text, sizeof(text), "%.1f mm", (double)session.diameter); lv_label_set_text(lv_obj_get_child(diameterBtn, 0), text);
  snprintf(text, sizeof(text), "%.4f", (double)session.factor); lv_label_set_text(factorValue, text);
  snprintf(text, sizeof(text), "OD %.1f mm\n%s / %.3f RPM\n~%.1f min / turn\nManual measurement", (double)session.diameter,
           speed_get_direction() == DIR_CW ? "CW" : "CCW", (double)constrain(0.25f, MIN_RPM, speed_get_rpm_max()),
           (double)(1.0f / constrain(0.25f, MIN_RPM, speed_get_rpm_max()))); lv_label_set_text(context, text);
  if (session.moving()) snprintf(text, sizeof(text), "Estimated travel %.0f / 360 deg", (double)view.progress_degrees);
  else snprintf(text, sizeof(text), "Measured %.2f deg  |  Error %+.2f deg", (double)session.verified, (double)(session.verified - 360));
  lv_label_set_text(progress, text);
  lv_obj_set_style_text_color(progress, session.stage == CalibrationSession::Result && !session.passed() ? COL_RED : COL_ACCENT, 0);
  enabled(runBtn, safe_idle() && !entryPanel && (session.stage == CalibrationSession::Prepare || session.stage == CalibrationSession::VerifyReady || session.stage == CalibrationSession::Result));
  lv_label_set_text(lv_obj_get_child(runBtn, 0), session.stage == CalibrationSession::Prepare || session.stage == CalibrationSession::Moving ? "MOVE 360" : "VERIFY 360");
  enabled(measurement, safe_idle()); enabled(diameterBtn, safe_idle());
  enabled(applyBtn, safe_idle() && session.measured > 0);
  enabled(saveBtn, safe_idle() && session.passed() && context_matches());
  enabled(restartBtn, safe_idle() && session.stage != CalibrationSession::Saving);
  enabled(backBtn, session.stage != CalibrationSession::Saving);
  const bool canJog = session.stage == CalibrationSession::Prepare && !entryPanel && ui_control_fresh() &&
                      (view.state == STATE_IDLE || view.state == STATE_JOG) && !safety_inhibit_motion();
  enabled(jogMinus, canJog); enabled(jogPlus, canJog);
}
