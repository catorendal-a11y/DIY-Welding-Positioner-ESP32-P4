// The --self-test regression run.
#include "sim_test_support.h"
#include <FastAccelStepper.h>
#include "../src/control/motion_policy.h"

static bool sim_test_all_screens_create_update() {
  const ScreenId ids[] = {
      SCREEN_BOOT,        SCREEN_MAIN,       SCREEN_MENU,           SCREEN_RUN_MODES,   SCREEN_PULSE,
      SCREEN_STEP,        SCREEN_JOG,        SCREEN_TIMER,          SCREEN_PROGRAMS,    SCREEN_PROGRAM_EDIT,
      SCREEN_EDIT_CONT,   SCREEN_EDIT_PULSE, SCREEN_EDIT_STEP,      SCREEN_SETTINGS,    SCREEN_MOTOR_CONFIG,
      SCREEN_CALIBRATION, SCREEN_DISPLAY,    SCREEN_PEDAL_SETTINGS, SCREEN_DIAGNOSTICS, SCREEN_SYSINFO,
      SCREEN_ABOUT,       SCREEN_CONFIRM,
  };

  for (ScreenId id : ids) {
    if (!sim_show_and_check(id)) return false;
  }

  screens_show(SCREEN_MAIN);
  screens_request_theme_reinit();
  sim_pump(120);
  return sim_expect(screens_get_current() == SCREEN_MAIN, "theme reinit did not restore current screen");
}

static bool sim_test_main_controls() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;
  float rpmBefore = speed_get_target_rpm();
  if (!sim_click_label("+")) return false;
  if (!sim_expect(speed_get_target_rpm() > rpmBefore, "RPM plus did not increase speed")) return false;
  if (!sim_click_label("-")) return false;
  if (!sim_click_action(UI_ACTION_START)) return false;
  sim_pump(260);  // DM542T ENA settle window (STATE_ENABLING)
  if (!sim_expect(control_get_state() == STATE_RUNNING, "START did not run")) return false;
  if (!sim_click_action(UI_ACTION_STOP)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "STOP did not stop")) return false;
  g_dir_switch_cache.store(false);
  sim_pump(250);
  Direction before = speed_get_direction();
  if (!sim_click_action(UI_ACTION_DIRECTION)) return false;
  return sim_expect(speed_get_direction() != before, "direction did not change");
}

static bool sim_test_navigation_flows() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;
  if (!sim_click_label("Menu")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MENU, "MENU button did not open menu")) return false;

  if (!sim_click_label("Run modes")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_RUN_MODES, "RUN MODES did not open run mode picker"))
    return false;

  if (!sim_click_label("Pulse")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PULSE, "run mode PULSE did not open pulse screen"))
    return false;
  if (!sim_click_action(UI_ACTION_START)) return false;
  sim_pump(260);  // ENA settle window
  if (!sim_expect(control_get_state() == STATE_PULSE, "pulse START did not enter PULSE")) return false;
  if (!sim_click_action(UI_ACTION_STOP)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "pulse STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("Step")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_STEP, "run mode STEP did not open step screen"))
    return false;
  if (!sim_click_action(UI_ACTION_START)) return false;
  sim_pump(260);  // ENA settle window
  if (!sim_expect(control_get_state() == STATE_STEP, "step button did not enter STEP")) return false;
  if (!sim_click_action(UI_ACTION_STOP)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "step STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("Jog")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_JOG, "run mode JOG did not open jog screen")) return false;
  if (!sim_hold_label("Hold CW", 260)) return false;
  if (!sim_expect(control_get_state() == STATE_JOG, "jog screen CW press did not enter JOG")) return false;
  if (!sim_send_label_event("Hold CW", LV_EVENT_RELEASED)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "jog screen CW release did not stop")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("Countdown")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_TIMER, "run mode timer did not open timer screen"))
    return false;
  g_settings.countdown_seconds = 1;
  if (!sim_click_label(UI_LABEL_START)) return false;
  sim_pump(3400);
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "timer countdown did not return to main"))
    return false;
  if (!sim_expect(control_get_state() == STATE_RUNNING, "timer countdown did not start RUNNING"))
    return false;
  if (!sim_click_action(UI_ACTION_STOP)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "timer STOP did not return IDLE")) return false;

  return true;
}

// The footer holds one action: Start while idle, Stop as soon as anything moves.
// Stop must use the configured ramp even after a soft start.
static bool sim_test_stop_after_soft_start() {
  screens_show(SCREEN_MAIN); sim_pump(40);
  FastAccelStepper::simAccelerationAtStop() = 0;
  if (!sim_expect(control_start_continuous(true, 0), "soft start rejected")) return false;
  sim_pump(300);
  if (!sim_expect(control_get_state() == STATE_RUNNING, "soft start did not run")) return false;
  control_stop(); sim_pump(300);
  const auto configured = static_cast<int32_t>(motion_scaled_acceleration(g_settings.acceleration, g_settings.microstep));
  return sim_expect(FastAccelStepper::simAccelerationAtStop() == configured, "Stop used the soft-start ramp");
}

static bool sim_test_single_action_footer() {
  for (ScreenId screen : {SCREEN_MAIN, SCREEN_PULSE, SCREEN_STEP}) {
    screens_show(screen); sim_pump(60);
    auto start = sim_find_action(screenRoots[screen], UI_ACTION_START);
    auto stop = sim_find_action(screenRoots[screen], UI_ACTION_STOP);
    if (!sim_expect(start && stop, "footer action missing")) return false;
    if (!sim_expect(!lv_obj_is_hidden(start) && lv_obj_is_hidden(stop), "idle footer shows Stop")) return false;
    if (!sim_click_action(UI_ACTION_START)) return false;
    if (!sim_expect(lv_obj_is_hidden(start) && !lv_obj_is_hidden(stop), "Stop missing during enable settle")) return false;
    if (screen != SCREEN_MAIN) {
      auto plus = sim_find_label_target(screenRoots[screen], "+", false, true);
      if (!sim_expect(plus && lv_obj_is_disabled(plus), "settings stayed editable during motion")) return false;
    }
    sim_pump(260);
    if (!sim_expect(control_get_state() != STATE_IDLE && !lv_obj_is_hidden(stop), "Stop missing during motion")) return false;
    if (!sim_click_action(UI_ACTION_STOP)) return false;
    sim_pump(300);
    if (!sim_expect(control_get_state() == STATE_IDLE && !lv_obj_is_hidden(start), "Stop did not return to Start")) return false;
  }
  screens_show(SCREEN_MAIN); sim_pump(40);
  return true;
}

// Programs opened from the main screen must return there, and from the menu to the menu.
static bool sim_test_programs_return() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;
  if (!sim_click_label("Programs")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS, "Programs did not open from main")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;
  if (!sim_click_label("Menu") || !sim_click_label("Programs")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS, "Programs did not open from menu")) return false;
  return sim_click_back_to(SCREEN_MENU);
}

static bool sim_test_settings_and_programs() {
  const ScreenId calibrationReturn = screens_get_current();
  screens_show(SCREEN_CALIBRATION); speed_set_slider_priority(true); screens_show(calibrationReturn);
  if (!sim_expect(!speed_using_slider(), "leaving calibration left the panel pot ignored")) return false;
  if (!sim_show_and_check(SCREEN_MENU)) return false;
  if (!sim_click_label("Setup")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_SETTINGS, "SETUP did not open settings")) return false;

  struct NavCase {
    const char* label;
    ScreenId dest;
  };
  const NavCase settingsCases[] = {
      {"Motor configuration", SCREEN_MOTOR_CONFIG},
      {"Calibration", SCREEN_CALIBRATION},
      {"Display settings", SCREEN_DISPLAY},
      {"Pedal settings", SCREEN_PEDAL_SETTINGS},
      {"Diagnostics", SCREEN_DIAGNOSTICS},
      {"System info", SCREEN_SYSINFO},
      {"About", SCREEN_ABOUT},
  };

  for (const NavCase& c : settingsCases) {
    if (!sim_show_and_check(SCREEN_SETTINGS)) return false;
    if (!sim_click_label(c.label)) return false;
    if (!sim_expect(screens_get_current() == c.dest, "settings row opened wrong screen")) return false;
    if (!sim_click_back_to(SCREEN_SETTINGS)) return false;
  }

  if (!sim_show_and_check(SCREEN_PROGRAMS)) return false;
  if (!sim_click_label("+ New")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAM_EDIT, "+ NEW did not open program edit"))
    return false;
  if (!sim_click_label("Continuous") || !sim_click_label("Mode settings " LV_SYMBOL_RIGHT)) return false;
  if (!sim_expect(screens_get_current() == SCREEN_EDIT_CONT, "mode settings did not open continuous edit"))
    return false;
  float previousRpm = screen_program_edit_get_preset()->rpm;
  if (!sim_click_label("+")) return false;
  sim_pump(450);
  if (!sim_click_label("Save")) return false;
  if (!sim_expect(screen_program_edit_get_preset()->rpm > previousRpm,
                  "CONT edits were overwritten by periodic update"))
    return false;
  screens_show(SCREEN_EDIT_PULSE);
  const uint32_t previousOn = screen_program_edit_get_preset()->pulse_on_ms;
  if (!sim_click_label("+")) return false;
  sim_pump(450);
  if (!sim_click_label("Save")) return false;
  if (!sim_expect(screen_program_edit_get_preset()->pulse_on_ms > previousOn,
                  "PULSE edits were overwritten by periodic update"))
    return false;
  screens_show(SCREEN_EDIT_CONT);
  Preset* draft = screen_program_edit_get_preset();
  const uint8_t draftId = draft->id;
  std::snprintf(draft->name, sizeof(draft->name), "Retained draft");
  draft->pulse_on_ms = 1234;
  if (!sim_click_back_to(SCREEN_PROGRAM_EDIT)) return false;
  draft = screen_program_edit_get_preset();
  if (!sim_expect(std::strcmp(draft->name, "Retained draft") == 0 && draft->pulse_on_ms == 1234 &&
                      draft->id == draftId,
                  "program draft lost on return"))
    return false;
  if (!sim_click_label("Cancel")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS, "program edit cancel did not return programs"))
    return false;

  return true;
}

static bool s_confirmHit = false;
static bool s_cancelHit = false;

static void sim_confirm_cb() { s_confirmHit = true; }

static void sim_cancel_cb() { s_cancelHit = true; }

static bool sim_test_confirm_and_overlay() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;

  s_cancelHit = false;
  screen_confirm_create("TEST CANCEL", "Cancel path.", sim_confirm_cb, sim_cancel_cb, SCREEN_NONE);
  sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_CONFIRM, "confirm dialog did not open")) return false;
  if (!sim_click_label("Cancel")) return false;
  sim_pump(80);
  if (!sim_expect(s_cancelHit, "confirm cancel callback did not run")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "confirm cancel did not return")) return false;

  s_confirmHit = false;
  screen_confirm_create("TEST CONFIRM", "Confirm path.", sim_confirm_cb, sim_cancel_cb, SCREEN_MAIN);
  sim_pump(80);
  if (!sim_click_label("Confirm")) return false;
  sim_pump(80);
  if (!sim_expect(s_confirmHit, "confirm callback did not run")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "confirm success did not return to main"))
    return false;

  simulator_set_estop_input(true);
  g_uiResetPending.store(false);
  estop_overlay_show();
  sim_pump(300);
  lv_obj_t* blockedReset = sim_find_label_target(lv_layer_top(), "RESET BLOCKED", false, true);
  if (!sim_expect(blockedReset && lv_obj_is_disabled(blockedReset),
                  "fault reset must be disabled"))
    return false;
  lv_obj_send_event(blockedReset, LV_EVENT_CLICKED, nullptr);
  if (!sim_expect(!g_uiResetPending.load(), "unsafe reset was requested")) return false;
  simulator_set_estop_input(false);
  sim_pump(300);
  lv_obj_t* readyReset = sim_find_label_target(lv_layer_top(), "RESET TO IDLE", false, true);
  if (!sim_expect(readyReset && !lv_obj_is_disabled(readyReset), "safe reset unavailable"))
    return false;
  if (!sim_expect(estop_overlay_visible(), "ESTOP overlay did not show")) return false;
  estop_overlay_hide();
  sim_pump(40);
  return sim_expect(!estop_overlay_visible(), "ESTOP overlay did not hide");
}

static unsigned bindingCalls = 0;
static void sim_bound_integer(lv_obj_t* label, int32_t value) {
  ++bindingCalls;
  lv_label_set_text_fmt(label, "%ld", (long)value);
}
static bool sim_test_status_bindings() {
  auto* scratch = lv_obj_create(nullptr);
  auto* first = lv_label_create(scratch);
  auto* second = lv_label_create(scratch);
  auto* button = lv_button_create(scratch);
  UiIntBinding integer;
  UiBoolBinding disabled;
  UiTextBinding<32> text;
  const uint32_t originalEvents = lv_obj_get_event_count(first);
  for (unsigned pass = 0; pass < 8; ++pass) {
    if (!sim_expect(integer.bind(first, sim_bound_integer), "integer observer allocation failed")) return false;
    integer.set(42);
    const unsigned notified = bindingCalls;
    integer.set(42);
    if (!sim_expect(bindingCalls == notified, "unchanged value notified its widget again")) return false;
    integer.reset();
    if (!sim_expect(lv_obj_get_event_count(first) == originalEvents, "binding reset retained widget callbacks")) return false;
  }
  if (!sim_expect(integer.bind(first, sim_bound_integer), "integer rebind failed")) return false;
  integer.set(17);
  if (!sim_expect(integer.bind(second, sim_bound_integer), "replacement binding failed")) return false;
  integer.set(23);
  if (!sim_expect(strcmp(lv_label_get_text(first), "17") == 0 && strcmp(lv_label_get_text(second), "23") == 0,
                  "old widget remained subscribed after rebind")) return false;
  if (!sim_expect(disabled.bind(button, lv_obj_set_disabled), "boolean binding failed")) return false;
  disabled.set(true);
  if (!sim_expect(lv_obj_is_disabled(button), "boolean binding did not disable button")) return false;
  disabled.set(false);
  if (!sim_expect(!lv_obj_is_disabled(button), "boolean binding did not restore button")) return false;
  if (!sim_expect(text.bind(first), "text binding failed")) return false;
  char temporary[] = "gyjp / 0.001";
  text.set(temporary); temporary[0] = 'X';
  if (!sim_expect(strcmp(lv_label_get_text(first), "gyjp / 0.001") == 0, "binding retained borrowed text")) return false;
  ui_trim_text_leading(first); lv_obj_update_layout(scratch);
  const auto* font = lv_obj_get_style_text_font(first, LV_PART_MAIN);
  const auto trim = lv_obj_get_style_text_leading_trim(first, LV_PART_MAIN);
  if (!sim_expect(lv_font_get_bottom_trim(font, trim) == 0, "text trim removed descender space")) return false;
  lv_obj_set_height(first, 1); lv_obj_update_layout(scratch);
  if (!sim_expect(audit_labels(first, "intentional clipped trim test") > 0, "trim-aware audit missed clipped text")) return false;
  auto numeric = lv_label_create(scratch);
  lv_obj_set_style_text_font(numeric, &rotator_digits_104, 0);
  lv_label_set_text(numeric, "0.075"); ui_trim_text_leading(numeric);
  if (!sim_expect(lv_obj_get_style_text_leading_trim(numeric, LV_PART_MAIN) == LV_TEXT_LEADING_TRIM_NONE,
                  "legacy numeric font without cap height was trimmed")) return false;
  for (const auto* source : {&rotator_digits_104, &lv_font_montserrat_28}) {
    UiTabularFont tabular(source);
    uint16_t advance = 0;
    for (uint32_t digit = '0'; digit <= '9'; ++digit) {
      for (uint32_t next : {0u, (uint32_t)'1', (uint32_t)'8', (uint32_t)'.'}) {
        lv_font_glyph_dsc_t glyph{};
        if (!sim_expect(lv_font_get_glyph_dsc(tabular.get(), &glyph, digit, next),
                        "tabular font lost a digit")) return false;
        if (!advance) advance = glyph.adv_w;
        if (!sim_expect(glyph.adv_w == advance, "digit width depends on value or kerning")) return false;
        if (!sim_expect(glyph.ofs_x >= 0 && glyph.ofs_x + glyph.box_w <= advance,
                        "tabular digit extends outside its cell")) return false;
      }
    }
    lv_obj_set_style_text_font(numeric, tabular.get(), 0);
    lv_obj_set_size(numeric, 340, tabular.get()->line_height);
    for (const auto* value : {"0.001", "1.111", "2.888", "3.000", "9.9", "10.0"}) {
      lv_label_set_text(numeric, value); lv_obj_update_layout(scratch);
      if (!sim_expect(lv_obj_get_width(numeric) == 340 &&
                      lv_obj_get_height(numeric) == tabular.get()->line_height,
                      "numeric readout changed its fixed bounds")) return false;
      if (!sim_expect(audit_labels(numeric, "tabular readout") == 0,
                      "tabular readout clips a supported RPM")) return false;
    }
    // Do not leave a widget pointing at this scoped font.
    lv_obj_set_style_text_font(numeric, &rotator_digits_104, 0);
  }
  lv_obj_set_style_text_font(numeric, ui_readout_font(), 0);
  lv_obj_set_size(numeric, 220, 2 * ui_readout_font()->line_height + 4);
  for (const auto* value : {"0\nDEG", "360 / 360\nDEG", "3600 / 3600\nDEG"}) {
    lv_label_set_text(numeric, value); lv_obj_update_layout(scratch);
    if (!sim_expect(audit_labels(numeric, "fixed angle readout") == 0,
                    "fixed angle readout clips step progress")) return false;
  }
  lv_obj_delete(scratch);
  integer.set(99); text.set("deleted"); disabled.set(true);
  integer.reset(); text.reset(); disabled.reset();

  UiInputPanel input;
  const auto openPanel = [&]() {
    input.open("Test input", "Whole number", "12", "0123456789", 12,
               LV_KEYBOARD_MODE_NUMBER, [](lv_event_t*) {});
  };
  openPanel();
  auto field = sim_find_type(lv_layer_top(), &lv_textarea_class);
  if (!sim_expect(field && lv_obj_get_style_text_leading_trim(field, LV_PART_MAIN) == LV_TEXT_LEADING_TRIM_NONE,
                  "shared text trim changed textarea metrics")) return false;
  simulator_set_estop_input(true); estop_overlay_show(); sim_pump(40);
  auto reset = sim_find_label_target(lv_layer_top(), "RESET BLOCKED", false, true);
  auto overlay = reset ? lv_obj_get_parent(reset) : nullptr;
  if (!sim_expect(overlay && lv_obj_get_child(lv_layer_top(), lv_obj_get_child_count(lv_layer_top())-1) == overlay,
                  "fault overlay was hidden behind input panel")) return false;
  simulator_set_estop_input(false); safety_reset_estop(); estop_overlay_hide(); sim_pump(80);
  lv_obj_delete(lv_obj_get_parent(field)); input.error("deleted");
  if (!sim_expect(!input.active(), "input panel retained deleted widgets")) return false;
  openPanel(); input.close(); openPanel(); sim_pump(30);
  if (!sim_expect(input.active(), "queued panel deletion cleared replacement")) return false;
  input.close(); sim_pump(30);
  if (!sim_expect(!sim_find_type(lv_layer_top(), &lv_textarea_class), "shared input panel leaked after close")) return false;

  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  const float previousRpm = speed_get_target_rpm();
  for (unsigned pass = 0; pass < 3; ++pass) {
    screens_show(SCREEN_MAIN); screens_reinit();
    speed_slider_set(0.076f + pass * 0.001f); sim_pump(80);
    char expected[16]; ui_format_rpm(expected, sizeof(expected), ui_control_view().target_rpm);
    if (!sim_expect(sim_label_object(screenRoots[SCREEN_MAIN], expected), "main RPM binding lost after theme rebuild")) return false;
    screens_show(SCREEN_DIAGNOSTICS); sim_pump(40);
    snprintf(expected, sizeof(expected), "%.3f", (double)ui_control_view().target_rpm);
    if (!sim_expect(sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], expected), "diagnostic target disagreed with snapshot")) return false;
    screens_reinit(); sim_pump(60);
    if (!sim_expect(sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], expected), "diagnostic binding lost after theme rebuild")) return false;
  }
  simulator_set_scenario("stalled-control"); sim_pump(160);
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "Status stale") &&
                  sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "---"), "diagnostics presented stale motion values as live")) return false;
  simulator_set_scenario("none"); sim_pump(80);
  if (!sim_expect(!sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "Status stale"), "diagnostics failed to recover fresh status")) return false;
  speed_slider_set(previousRpm); screens_show(SCREEN_MAIN); sim_pump(40);
  return true;
}
static bool sim_test_countdown_cancellation_and_direction() {
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  g_settings.countdown_seconds=1;
  for (unsigned elapsed : {50u,900u}) {
    screens_show(SCREEN_TIMER); sim_pump(40);
    if (!sim_click_label(UI_LABEL_START)) return false;
    sim_pump(elapsed);
    // Fault/reset happen without a UI tick: current safety state alone cannot detect this history.
    simulator_set_estop_input(true); control_run_cycle();
    simulator_set_estop_input(false); safety_reset_estop(); control_run_cycle();
    sim_pump(1200);
    if (!sim_expect(control_get_state()==STATE_IDLE && !motor_is_running(),"old countdown restarted after fault/reset")) return false;
  }
  screens_show(SCREEN_TIMER); sim_pump(40);
  if (!sim_click_label(UI_LABEL_START)) return false;
  control_stop(); sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_IDLE,"idle STOP did not cancel countdown")) return false;
  if (!sim_click_label(UI_LABEL_START)) return false;
  screens_show(SCREEN_MAIN); sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_IDLE,"navigation did not cancel countdown")) return false;
  screens_show(SCREEN_TIMER); sim_pump(40);
  if (!sim_click_label(UI_LABEL_START)) return false;
  sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_RUNNING,"fresh countdown START failed")) return false;
  control_stop(); sim_pump(300);
  g_settings.invert_direction=true; speed_set_direction(DIR_CW);
  screens_show(SCREEN_PROGRAMS); sim_pump(40);
  if (!sim_click_label("+ New")) return false;
  const auto draft=screen_program_edit_get_preset();
  if (!sim_expect(draft->direction==DIR_CW && speed_resolve_direction((Direction)draft->direction)==DIR_CCW,
                  "new program inherited an already inverted direction")) return false;
  if (!sim_click_label("Cancel")) return false;
  g_settings.invert_direction=false;
  speed_set_workpiece_diameter_mm(300); speed_slider_set(0.02f);
  screens_show(SCREEN_STEP); sim_pump(40);
  auto speedCard=lv_obj_get_parent(sim_label_object(screenRoots[SCREEN_STEP],"Speed"));
  auto targetCard=lv_obj_get_parent(sim_label_object(screenRoots[SCREEN_STEP],"Step angle"));
  lv_obj_send_event(sim_find_label_target(speedCard,"-",false,true),LV_EVENT_SHORT_CLICKED,nullptr); sim_pump(60);
  if (!sim_expect(fabs(speed_get_target_rpm()-0.019f)<0.00001f,"Step fine adjustment failed")) return false;
  if (!sim_click_label("90\xC2\xB0")) return false;
  lv_obj_send_event(sim_find_label_target(targetCard,"+",false,true),LV_EVENT_SHORT_CLICKED,nullptr); sim_pump(60);
  if (!sim_expect(sim_find_label_target(screenRoots[SCREEN_STEP],LV_SYMBOL_PLAY "  Step 95\xC2\xB0",false,true) != nullptr,"Step angle + did not change the move")) return false;
  lv_obj_send_event(sim_find_label_target(targetCard,"-",false,true),LV_EVENT_SHORT_CLICKED,nullptr); sim_pump(60);
  if (!sim_expect(audit_labels(screenRoots[SCREEN_STEP],"Step fine adjustment")==0,"Step labels overflowed")) return false;
  // Boundary geometry must produce an explanation, never a narrowed/reversed move.
  g_settings.microstep=32; g_settings.calibration_factor=1.5f;
  speed_set_workpiece_diameter_mm(20000);
  if (!sim_click_label("Step angle")) return false;
  auto field=sim_find_type(screenRoots[SCREEN_STEP],&lv_textarea_class);
  auto keyboard=sim_find_type(screenRoots[SCREEN_STEP],&lv_keyboard_class);
  if (!sim_expect(field && keyboard,"custom Step input missing")) return false;
  lv_textarea_set_text(field,"3600"); lv_obj_send_event(keyboard,LV_EVENT_READY,nullptr); sim_pump(80);
  auto blockedMove=sim_find_active_label_target("Out of range");
  if (!sim_expect(blockedMove && lv_obj_is_disabled(blockedMove),"overflowing move was not visibly blocked")) return false;
  if (!sim_expect(audit_labels(screenRoots[SCREEN_STEP],"Step boundary values")==0,"boundary Step labels overflowed")) return false;
  g_settings.microstep=16; g_settings.calibration_factor=1; speed_set_workpiece_diameter_mm(300);
  if (!sim_click_label("90\xC2\xB0")) return false;
  screens_show(SCREEN_DISPLAY); sim_pump(40);
  lv_obj_t* dim=nullptr;
  for (const char* value : {"Off","30s","1m","2m","5m"})
    if (!dim) dim=sim_find_active_label_target(value);
  if (!sim_expect(dim!=nullptr,"Display dim setting missing")) return false;
  lv_obj_send_event(dim,LV_EVENT_CLICKED,nullptr); sim_pump(40);
  const std::string expected=lv_label_get_text(lv_obj_get_child(dim,0));
  screens_reinit(); sim_pump(60);
  if (!sim_expect(sim_find_label_target(screenRoots[SCREEN_DISPLAY],expected.c_str(),false,true)!=nullptr,
                  "theme reconstruction discarded the dim draft")) return false;
  screens_show(SCREEN_MAIN); sim_pump(40);
  return true;
}
static bool sim_test_v23_fault_scenarios() {
  const struct { const char* scenario; const char* title; bool resettable; } faults[] = {
    {"hmi-stale", "HMI STALE", false},
    {"speed-pot", "SPEED POT", false},
    {"storage", "STORAGE", false},
    {"unexpected-reset", "UNEXPECTED RESET", true},
  };
  for (const auto& fault : faults) {
    screens_show(SCREEN_MAIN); sim_pump(40);
    if (!sim_expect(simulator_set_scenario(fault.scenario), "fault scenario unknown")) return false;
    g_uiResetPending.store(false);
    estop_overlay_show(); sim_pump(300);
    if (!sim_expect(control_get_state() == STATE_ESTOP, "fault scenario did not latch")) return false;
    if (!sim_expect(sim_label_object(lv_layer_top(), fault.title) != nullptr, "fault overlay heading wrong")) return false;
    if (!sim_expect(!control_start_continuous(), "fault scenario admitted motion")) return false;
    auto erase = sim_find_label_target(lv_layer_top(), "Hold 3 s to erase data", false, true);
    if (!sim_expect(erase && lv_obj_is_hidden(erase) == (std::strcmp(fault.scenario, "storage") != 0), "erase offered for the wrong fault")) return false;
    if (!fault.resettable) {
      auto blocked = sim_find_label_target(lv_layer_top(), "RESET BLOCKED", false, true);
      if (!sim_expect(blocked && lv_obj_is_disabled(blocked), "fault reset was not blocked")) return false;
      lv_obj_send_event(blocked, LV_EVENT_CLICKED, nullptr);
      if (!sim_expect(!g_uiResetPending.load(), "blocked fault requested a reset")) return false;
      simulator_set_scenario("none"); sim_pump(300);
    }
    auto ready = sim_find_label_target(lv_layer_top(), "RESET TO IDLE", false, true);
    if (!sim_expect(ready && !lv_obj_is_disabled(ready), "cleared fault cannot reset")) return false;
    safety_reset_estop(); estop_overlay_hide(); simulator_set_scenario("none"); control_transition_to(STATE_IDLE);
    sim_pump(40);
    if (!sim_expect(!motor_is_running(), "reset started motion")) return false;
  }
  return true;
}

static bool sim_test_operator_feedback() {
  // A confirmed program start that gets blocked says why, then returns to the list.
  Preset preset{};
  preset.id = 240; std::strncpy(preset.name, "BLOCK TEST", sizeof(preset.name) - 1);
  preset.mode = STATE_RUNNING; preset.mode_mask = preset_mode_to_mask(STATE_RUNNING);
  preset.rpm = 0.5f; preset.direction = DIR_CW;
  g_presets.push_back(preset);
  screens_show(SCREEN_MAIN); sim_pump(40); screens_show(SCREEN_PROGRAMS); sim_pump(80);
  auto name = sim_label_object(screenRoots[SCREEN_PROGRAMS], "BLOCK TEST");
  auto row = name ? sim_find_type(lv_obj_get_parent(name), &lv_button_class) : nullptr;
  if (!sim_expect(row != nullptr, "program row missing")) return false;
  lv_obj_send_event(row, LV_EVENT_CLICKED, nullptr); sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_CONFIRM, "program review did not open")) return false;
  simulator_set_scenario("speed-pot");
  if (!sim_click_label("Confirm")) return false;
  sim_pump(120);
  if (!sim_expect(screens_get_current() == SCREEN_CONFIRM &&
                  sim_label_object(screenRoots[SCREEN_CONFIRM], "PROGRAM BLOCKED"), "blocked start was silent")) return false;
  if (!sim_expect(audit_labels(screenRoots[SCREEN_CONFIRM], "program blocked") == 0, "blocked text overflowed")) return false;
  if (!sim_click_label("Cancel")) return false;
  sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS, "blocked dialog lost its return screen")) return false;
  simulator_set_scenario("none"); safety_reset_estop(); control_transition_to(STATE_IDLE); sim_pump(40);
  g_presets.pop_back();
  screens_show(SCREEN_MAIN); sim_pump(40);

  // REBOOT explains a pending save, and that dialog must not offer REBOOT.
  screens_show(SCREEN_SYSINFO); sim_pump(40);
  storage_request_settings_save();
  if (!sim_click_label("Reboot")) return false;
  sim_pump(80);
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_CONFIRM], "RESTART NOT POSSIBLE") &&
                  sim_label_object(screenRoots[SCREEN_CONFIRM], "Confirm"), "blocked REBOOT was silent")) return false;
  if (!sim_click_label("Cancel")) return false;
  sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_SYSINFO, "REBOOT dialog lost its return screen")) return false;
  // A save that keeps failing must not lock the on-screen restart.
  simulator_set_scenario("nvs-failure"); storage_request_settings_save(); sim_pump(600);
  if (!sim_click_label("Reboot")) return false;
  sim_pump(80);
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_CONFIRM], "REBOOT DEVICE") != nullptr, "failed save blocked REBOOT")) return false;
  if (!sim_click_label("Cancel")) return false;
  sim_pump(80);
  simulator_set_scenario("none"); sim_pump(1500);

  // Jog must not change the manual direction.
  speed_set_direction(DIR_CW);
  screens_show(SCREEN_JOG); sim_pump(40);
  if (!sim_hold_label("Hold CCW", 80)) return false;
  sim_pump(300);
  if (!sim_expect(speed_get_requested_direction() == DIR_CW, "Jog changed the manual direction")) return false;

  // Invalid Step entries keep the keypad open and explain the limit.
  speed_set_workpiece_diameter_mm(300);
  screens_show(SCREEN_STEP); sim_pump(40);
  if (!sim_click_label("Step angle")) return false;
  auto field = sim_find_type(screenRoots[SCREEN_STEP], &lv_textarea_class);
  auto keyboard = sim_find_type(screenRoots[SCREEN_STEP], &lv_keyboard_class);
  if (!sim_expect(field && keyboard, "Step keypad missing")) return false;
  lv_textarea_set_text(field, "0"); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr); sim_pump(80);
  if (!sim_expect(sim_find_type(screenRoots[SCREEN_STEP], &lv_keyboard_class) &&
                  sim_label_object(screenRoots[SCREEN_STEP], "Enter an angle above 0, up to 3600 degrees"),
                  "invalid Step angle was silent")) return false;
  lv_textarea_set_text(field, "90"); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr); sim_pump(80);
  if (!sim_expect(!sim_find_type(screenRoots[SCREEN_STEP], &lv_keyboard_class), "valid Step angle kept the keypad")) return false;
  if (!sim_click_label("Diameter", true)) return false;
  field = sim_find_type(screenRoots[SCREEN_STEP], &lv_textarea_class);
  keyboard = sim_find_type(screenRoots[SCREEN_STEP], &lv_keyboard_class);
  if (!sim_expect(field && keyboard, "diameter keypad missing")) return false;
  lv_textarea_set_text(field, "0"); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr); sim_pump(80);
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_STEP], "Enter a diameter from 1 to 20000 mm") != nullptr,
                  "invalid diameter was silent")) return false;
  lv_obj_send_event(keyboard, LV_EVENT_CANCEL, nullptr); sim_pump(80);
  screens_show(SCREEN_MAIN); sim_pump(40);

  // The screen saver follows the selected accent.
  const auto previousAccent = g_settings.accent_color;
  theme_set_color(1); screen_saver_destroy();
  screen_saver_request_preview(); sim_pump(80);
  auto standby = sim_label_object(lv_layer_top(), "Standby");
  const bool follows = standby && lv_color_eq(lv_obj_get_style_text_color(standby, LV_PART_MAIN), theme_accent_on_dark());
  screen_saver_destroy(); theme_set_color(previousAccent); sim_pump(40);
  return sim_expect(follows, "screen saver ignored the accent");
}

// Regressions for defects found by the function review.
static bool sim_test_review_fixes() {
  // A diameter typed during calibration belongs to the draft: leaving restores the saved one.
  g_settings.workpiece_diameter_mm = 300; speed_set_workpiece_diameter_mm(300);
  screens_show(SCREEN_CALIBRATION); sim_pump(40);
  speed_set_workpiece_diameter_mm(500);
  screens_show(SCREEN_SETTINGS); sim_pump(40);
  if (!sim_expect(speed_get_workpiece_diameter_mm() == 300, "calibration diameter leaked after leaving")) return false;
  // Display: brightness is previewed live, and Back without Save returns to the saved value.
  screens_show(SCREEN_DISPLAY); sim_pump(40);
  const uint8_t brightness = g_settings.brightness;
  auto slider = sim_find_type(screenRoots[SCREEN_DISPLAY], &lv_slider_class);
  if (!sim_expect(slider != nullptr, "brightness slider missing")) return false;
  lv_slider_set_value(slider, 20, LV_ANIM_OFF); lv_obj_send_event(slider, LV_EVENT_VALUE_CHANGED, nullptr); sim_pump(40);
  if (!sim_expect(g_settings.brightness != brightness, "brightness preview did not apply")) return false;
  if (!sim_click_back_to(SCREEN_SETTINGS)) return false;
  if (!sim_expect(g_settings.brightness == brightness, "Back kept an unsaved brightness")) return false;
  // Main +/- uses the fine step below 0.1 RPM, like the other screens.
  screens_show(SCREEN_MAIN); sim_pump(40); speed_slider_set(0.05f); sim_pump(40);
  if (!sim_click_label("+")) return false;
  if (!sim_expect(fabs(speed_get_target_rpm() - 0.051f) < 0.00001f, "Main + skipped the fine step")) return false;
  // A program cannot be saved slower than the motor can run at its own diameter.
  screens_set_edit_slot(-1); screens_show(SCREEN_PROGRAM_EDIT); sim_pump(40);
  Preset* draft = screen_program_edit_get_preset();
  draft->workpiece_diameter_mm = 50; draft->rpm = MIN_RPM;
  screen_program_edit_update_ui();
  if (!sim_expect(screen_program_edit_rpm_min() > MIN_RPM && draft->rpm >= screen_program_edit_rpm_min(),
                  "program speed below the motor range was accepted")) return false;
  if (!sim_expect(control_program_feasible(*draft), "clamped program still cannot start")) return false;
  screens_show(SCREEN_MAIN); sim_pump(40); speed_slider_set(0.5f);
  return true;
}

int run_self_test() {
  std::puts("SIM SELFTEST: start");
  g_settings.countdown_seconds = 1;
  if (!sim_test_all_screens_create_update()) return 2;
  std::puts("SIM SELFTEST: screens ok");
  if (!sim_test_status_bindings()) return 17;
  std::puts("SIM SELFTEST: status bindings/lifecycle/stale diagnostics ok");
  if (!sim_test_main_controls()) return 3;
  std::puts("SIM SELFTEST: main controls ok");
  if (!sim_test_navigation_flows()) return 4;
  std::puts("SIM SELFTEST: run mode flows ok");
  if (!sim_test_settings_and_programs()) return 5;
  if (!sim_test_single_action_footer()) return 22;
  if (!sim_test_stop_after_soft_start()) return 24;
  if (!sim_test_programs_return()) return 23;
  std::puts("SIM SELFTEST: one-action footer and Programs return ok");
  std::puts("SIM SELFTEST: settings/programs ok");
  if (const int step = run_program_edit_test()) {
    std::printf("SELFTEST FAIL: program editor step %d\n", step);
    return 15;
  }
  std::puts("SIM SELFTEST: new program modes/precision/input/save/cancel ok");
  if (run_motor_config_test() != 0) return 18;
  std::puts("SIM SELFTEST: motor exact input/cancel/save lock/lifecycle ok");
  if (!sim_test_confirm_and_overlay()) return 6;
  std::puts("SIM SELFTEST: confirm/overlay ok");
  if (!sim_test_v23_fault_scenarios()) return 20;
  std::puts("SIM SELFTEST: HMI/pot/storage/reset fault scenarios ok");
  if (!sim_test_operator_feedback()) return 21;
  if (!sim_test_review_fixes()) return 25;
  std::puts("SIM SELFTEST: calibration diameter, display Back, fine step, program speed floor ok");
  std::puts("SIM SELFTEST: blocked program/REBOOT, Step entry, Jog direction, saver accent ok");
  simulator_set_scenario("nvs-failure");
  const uint32_t ticket = storage_request_settings_save();
  sim_pump(600);
  if (!sim_expect(storage_settings_save_status(ticket) == STORAGE_ERROR, "failed save reported success")) return 7;
  simulator_set_scenario("none"); sim_pump(1500);
  if (!sim_expect(storage_settings_save_status(ticket) == STORAGE_SAVED, "save retry did not commit")) return 8;
  control_transition_to(STATE_IDLE);
  sim_pump(30);
  simulator_set_scenario("rejected-motion");
  control_start_continuous(); sim_pump(260);  // ENA settle, then the rejected replay
  if (!sim_expect(control_get_state() == STATE_ESTOP,
                  "rejected motion did not latch fault")) return 9;
  simulator_set_scenario("none"); control_transition_to(STATE_IDLE);
  std::puts("SIM SELFTEST: fault scenarios/save generations ok");
  if (run_calibration_test() != 0) return 14;
  std::puts("SIM SELFTEST: calibration interruption/draft/verify/save ok");
  if (run_commissioning_test() != 0) return 10;
  std::puts("SIM SELFTEST: commissioning UI/save failure/retry ok");
  screens_show(SCREEN_MAIN); sim_pump(80);
  simulator_set_scenario("stalled-control"); sim_pump(160);
  if (!sim_expect(!ui_control_fresh() && !control_start_continuous(), "stale control admitted motion")) return 11;
  auto start = sim_find_label_target(screenRoots[SCREEN_MAIN], "Start blocked", false, true);
  if (!sim_expect(start && lv_obj_is_disabled(start), "stale START not disabled")) return 12;
  control_stop(); sim_pump(60);
  if (!sim_expect(control_get_state() == STATE_ESTOP, "stalled executor did not fault on STOP")) return 13;
  simulator_set_scenario("none"); safety_reset_estop(); sim_pump(80);
  std::puts("SIM SELFTEST: stale control and independent STOP deadline ok");
  if (!sim_test_countdown_cancellation_and_direction()) return 16;
  std::puts("SIM SELFTEST: countdown cancellation/direction/fine Step adjustment ok");
  if (run_screen_saver_test() != 0) return 19;
  std::puts("SIM SELFTEST: screen saver timeout/wake/motion/fault/lifecycle ok");
  std::puts("SIM SELFTEST: PASS");
  return 0;
}
