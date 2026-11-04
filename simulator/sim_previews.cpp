// Regressions that also export screen captures: saver, motor, program, calibration, setup.
#include "sim_test_support.h"

bool sim_enter_calibration_measurement(const char* value) {
  if (!sim_click_label("---")) return false;
  auto root = lv_layer_top();
  auto field = sim_find_type(root, &lv_textarea_class);
  auto keyboard = sim_find_type(root, &lv_keyboard_class);
  if (!sim_expect(field && keyboard, "calibration measurement editor missing")) return false;
  lv_textarea_set_text(field, value); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr);
  sim_pump(80); return true;
}
uint8_t simulator_display_brightness();
int run_screen_saver_test(const char* directory) {
  unsigned failures = 0;
  auto check = [&](bool condition, const char* message) {
    if (!sim_expect(condition, message)) ++failures;
  };
  auto capture = [&](const char* name, bool modal) {
    auto root = modal ? lv_layer_top() : lv_screen_active();
    lv_obj_update_layout(root);
    failures += audit_labels(root, name);
    if (!directory) return;
    std::filesystem::create_directories(directory);
    auto shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888);
    if (!shot) { ++failures; return; }
    check(sim_write_bmp_argb8888((std::filesystem::path(directory)/name).string().c_str(), shot),
          "screen saver screenshot failed");
    lv_draw_buf_destroy(shot);
  };
  simulator_set_scenario("none"); safety_reset_estop(); estop_overlay_hide();
  control_stop(); screens_show(SCREEN_MAIN); sim_pump(120);
  const auto previousTimeout = g_settings.dim_timeout;
  const auto previousBrightness = g_settings.brightness;
  g_settings.dim_timeout = 30; g_settings.brightness = 180;
  dim_reset_activity();
  auto now = lv_tick_get();
  screen_saver_update(now + 29990);
  check(!screen_saver_visible(), "screen saver activated early");
  screen_saver_update(now + 30001);
  check(screen_saver_visible() && screens_get_current() == SCREEN_MAIN,
        "idle timeout failed or navigation changed");
  check(simulator_display_brightness() == 38, "screen saver did not dim backlight");
  capture("01_screen_saver.bmp", true);
  screen_saver_update(now + 34002);
  capture("02_screen_saver_motion.bmp", true);

  // Feed a real LVGL pointer through the production filter over START.
  // Direct LV_EVENT_CLICKED would bypass hit testing and cannot prove wake safety.
  struct TestPointer { ScreenSaverInput input; lv_point_t point; bool down = false; } pointer{};
  auto start = sim_find_action(screenRoots[SCREEN_MAIN], UI_ACTION_START);
  check(start != nullptr, "START missing in saver test");
  if (!start) return 24;
  lv_area_t bounds{}; lv_obj_get_coords(start, &bounds);
  pointer.point = {(bounds.x1 + bounds.x2) / 2, (bounds.y1 + bounds.y2) / 2};
  auto device = lv_indev_create();
  lv_indev_set_type(device, LV_INDEV_TYPE_POINTER);
  lv_indev_set_mode(device, LV_INDEV_MODE_EVENT);
  lv_indev_set_user_data(device, &pointer);
  lv_indev_set_read_cb(device, [](lv_indev_t* indev, lv_indev_data_t* data) {
    auto& source = *static_cast<TestPointer*>(lv_indev_get_user_data(indev));
    data->point = source.point;
    data->state = source.down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    screen_saver_filter_input(data, source.input);
  });
  pointer.down = true; lv_indev_read(device); sim_pump(60);
  check(!screen_saver_visible() && simulator_display_brightness() == 180,
        "wake did not restore screen/brightness");
  for (int i = 0; i < 4; ++i) { lv_indev_read(device); sim_pump(60); }
  check(control_get_state() == STATE_IDLE && pointer.input.suppressUntilRelease,
        "held wake touch reached START");
  screen_saver_update(lv_tick_get() + 300001);
  check(!screen_saver_visible(), "screen saver activated during a held touch");
  dim_reset_activity();
  pointer.down = false; lv_indev_read(device); sim_pump(60);
  check(control_get_state() == STATE_IDLE && !pointer.input.suppressUntilRelease,
        "wake release reached START or retained latch");
  capture("03_awake.bmp", false);
  pointer.down = true; lv_indev_read(device); sim_pump(40);
  pointer.down = false; lv_indev_read(device); sim_pump(80);
  sim_pump(260);  // ENA settle: START lands in ENABLING, then runs
  check(control_get_state() == STATE_RUNNING, "second deliberate touch did not reach START");
  screen_saver_request_preview(); dim_update();
  check(!screen_saver_visible(), "screen saver hid running controls");
  control_stop(); sim_pump(120);
  lv_indev_delete(device);

  screen_saver_request_preview(); dim_update();
  check(screen_saver_visible(), "preview unavailable before physical start");
  control_start_continuous(); sim_pump(80);
  check(!screen_saver_visible() && simulator_display_brightness() == 180,
        "external motion start retained screen saver/dimmed controls");
  control_stop(); sim_pump(120);

  // Physical-input wake requests, including OFF, restore the operator display.
  screen_saver_request_preview(); dim_update();
  check(screen_saver_visible(), "idle preview did not open");
  g_settings.dim_timeout = 0; g_wakePending.store(true); dim_update();
  check(!screen_saver_visible() && !g_wakePending.load(), "OFF retained a physical wake request");
  screen_saver_update(lv_tick_get() + 300001);
  check(!screen_saver_visible(), "OFF allowed automatic screen saver");
  dim_reset_activity();
  screen_saver_request_preview(); dim_update();
  check(screen_saver_visible(), "OFF prevented explicit preview");
  simulator_set_estop_input(true); estop_overlay_show(); dim_update();
  check(!screen_saver_visible() && estop_overlay_visible(), "screen saver obscured E-STOP");
  check(simulator_display_brightness() == 180, "E-STOP retained dimmed backlight");
  capture("04_estop_priority.bmp", true);
  simulator_set_estop_input(false); safety_reset_estop(); estop_overlay_hide(); sim_pump(100);
  for (auto screen : {SCREEN_PROGRAM_EDIT, SCREEN_MOTOR_CONFIG, SCREEN_CALIBRATION, SCREEN_SETUP}) {
    screens_show(screen); sim_pump(60);
    screen_saver_request_preview(); dim_update();
    check(!screen_saver_visible(), "screen saver obscured editing/commissioning");
  }
  screens_show(SCREEN_MAIN); sim_pump(60);
  simulator_set_scenario("stalled-control"); sim_pump(160);
  screen_saver_request_preview(); dim_update();
  check(!screen_saver_visible(), "screen saver obscured stale status");
  simulator_set_scenario("none"); safety_reset_estop(); sim_pump(80);
  screen_saver_request_preview(); dim_update();
  simulator_set_scenario("nvs-failure"); storage_request_settings_save(); sim_pump(600);
  check(!screen_saver_visible(), "screen saver obscured a pending/failed save");
  simulator_set_scenario("none"); sim_pump(1500);
  screens_show(SCREEN_DISPLAY); sim_pump(40);
  capture("05_display_settings.bmp", false);
  check(sim_click_label("PREVIEW"), "Display PREVIEW missing");
  check(screen_saver_visible(), "Display PREVIEW did not open screen saver");
  screens_reinit(); sim_pump(80);
  check(!screen_saver_visible() && screens_get_current() == SCREEN_DISPLAY,
        "theme rebuild retained screen saver or lost page");
  for (int i = 0; i < 3; ++i) {
    screen_saver_request_preview(); dim_update();
    check(screen_saver_visible(), "preview recreation failed");
    screen_saver_destroy();
  }
  g_settings.dim_timeout = previousTimeout; g_settings.brightness = previousBrightness;
  screens_show(SCREEN_MAIN); dim_reset_activity();
  std::printf("SCREEN SAVER TEST: %u failures\n", failures);
  return failures ? 24 : 0;
}

int run_motor_config_test(const char* directory) {
  unsigned failures = 0;
  auto capture = [&](const char* name, bool modal = false) {
    auto root = modal ? lv_layer_top() : screenRoots[SCREEN_MOTOR_CONFIG];
    lv_obj_update_layout(root); failures += audit_labels(root, name);
    if (!directory) return;
    std::filesystem::create_directories(directory);
    auto shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888);
    if (!shot) { ++failures; return; }
    sim_write_bmp_argb8888((std::filesystem::path(directory)/name).string().c_str(), shot);
    lv_draw_buf_destroy(shot);
  };
  auto enter = [&](const char* value) {
    auto field = sim_find_type(lv_layer_top(), &lv_textarea_class);
    auto kb = sim_find_type(lv_layer_top(), &lv_keyboard_class);
    if (!field || !kb) return false;
    lv_textarea_set_text(field, value); lv_obj_send_event(kb, LV_EVENT_READY, nullptr);
    sim_pump(60); return true;
  };
  auto openValue = [&](const char* title) {
    auto heading = sim_label_object(screenRoots[SCREEN_MOTOR_CONFIG], title);
    auto card = heading ? lv_obj_get_parent(heading) : nullptr;
    if (!card) return false;
    for (uint32_t i = 0; i < lv_obj_get_child_count(card); ++i) {
      auto child = lv_obj_get_child(card, i);
      if (lv_obj_check_type(child, &lv_button_class)) {
        lv_obj_send_event(child, LV_EVENT_CLICKED, nullptr); sim_pump(40);
        return sim_find_type(lv_layer_top(), &lv_textarea_class) != nullptr;
      }
    }
    return false;
  };
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  const SystemSettings original = g_settings;
  screens_show(SCREEN_MOTOR_CONFIG); sim_pump(40);
  capture("01_motor_config.bmp");
  if (!sim_expect(openValue("MAX SPEED / RPM"), "motor limit missing exact input")) return 1;
  capture("02_speed_input.bmp", true);
  if (!enter("4") || !sim_expect(sim_find_type(lv_layer_top(), &lv_textarea_class), "invalid limit closed editor")) return 2;
  capture("03_invalid_speed.bmp", true);
  for (const char* invalid : {"", "0", "0.0751", "1.2.3"})
    if (!enter(invalid) || !sim_expect(sim_find_type(lv_layer_top(), &lv_textarea_class), "invalid or over-precise motor limit accepted")) return 21;
  if (!enter("0,075") || !sim_expect(!sim_find_type(lv_layer_top(), &lv_textarea_class), "decimal comma limit was not accepted")) return 3;
  if (!sim_expect(fabs(g_settings.max_rpm-original.max_rpm) < 0.00001f, "exact input applied before SAVE")) return 4;
  if (!openValue("MAX SPEED / RPM")) return 5;
  auto cancel = sim_find_label_target(lv_layer_top(), "CANCEL", false, true);
  if (!cancel) return 5;
  lv_obj_send_event(cancel, LV_EVENT_CLICKED, nullptr); sim_pump(60);
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_MOTOR_CONFIG], "0.075"), "Cancel changed speed draft")) return 6;
  auto speedHeading = sim_label_object(screenRoots[SCREEN_MOTOR_CONFIG], "MAX SPEED / RPM");
  auto speedCard = lv_obj_get_parent(speedHeading);
  auto plus = sim_find_label_target(speedCard, "+", false, true);
  auto minus = sim_find_label_target(speedCard, "-", false, true);
  if (!plus || !minus) return 22;
  for (auto event : {LV_EVENT_SHORT_CLICKED, LV_EVENT_LONG_PRESSED, LV_EVENT_LONG_PRESSED_REPEAT})
    lv_obj_send_event(plus, event, nullptr);
  lv_obj_send_event(plus, LV_EVENT_CLICKED, nullptr); sim_pump(40);
  if (!sim_expect(sim_label_object(speedCard, "0.078"), "hold-repeat lost precision or double-stepped on release")) return 23;
  for (unsigned i=0; i<3; ++i) lv_obj_send_event(minus, LV_EVENT_SHORT_CLICKED, nullptr);
  sim_pump(40);
  if (!openValue("ACCEL AT 1/16") || !enter("15000.5")) return 7;
  if (!sim_expect(sim_find_type(lv_layer_top(), &lv_textarea_class), "fractional acceleration was silently rounded")) return 8;
  capture("04_invalid_acceleration.bmp", true);
  if (!enter("15001")) return 9;
  capture("05_motor_draft.bmp");
  simulator_set_scenario("nvs-failure");
  if (!sim_click_label("SAVE & APPLY")) return 10;
  sim_pump(600);
  if (!sim_expect(fabs(g_settings.max_rpm-0.075f) < 0.00001f && g_settings.acceleration == 15001,
                  "motor configuration lost exact values")) return 11;
  capture("06_save_failure.bmp");
  auto save = sim_find_active_label_target("SAVE & APPLY");
  if (!sim_expect(save && lv_obj_is_disabled(save) && !openValue("MAX SPEED / RPM"),
                  "pending save allowed edits or duplicate apply")) return 12;
  simulator_set_scenario("none"); sim_pump(1500);
  if (!sim_expect(!lv_obj_is_disabled(save), "save completion did not unlock motor settings")) return 13;
  if (!openValue("MAX SPEED / RPM")) return 14;
  screens_show(SCREEN_SETTINGS); sim_pump(50);
  if (!sim_expect(!sim_find_type(lv_layer_top(), &lv_textarea_class), "motor keyboard leaked on navigation")) return 15;
  screens_show(SCREEN_MOTOR_CONFIG); sim_pump(40);
  if (!openValue("MAX SPEED / RPM")) return 16;
  screens_reinit(); sim_pump(50);
  if (!sim_expect(!sim_find_type(lv_layer_top(), &lv_textarea_class), "motor keyboard leaked on theme rebuild")) return 17;
  simulator_set_scenario("stalled-control"); sim_pump(160);
  save = sim_find_active_label_target("SAVE & APPLY");
  if (!sim_expect(save && lv_obj_is_disabled(save) && sim_label_object(screenRoots[SCREEN_MOTOR_CONFIG], "STATUS STALE"),
                  "stale motor settings still looked ready to apply")) return 18;
  capture("07_stale_status.bmp");
  simulator_set_scenario("none"); sim_pump(80);
  if (!control_apply_motor_settings(original)) return 19;
  sim_pump(600); screens_show(SCREEN_MAIN); sim_pump(50);
  return failures ? 20 : 0;
}

int run_program_edit_test(const char* directory) {
  unsigned failures = 0;
  auto findLabel = [](auto&& self, lv_obj_t* root, const char* value) -> lv_obj_t* {
    if (lv_obj_check_type(root,&lv_label_class) && strcmp(lv_label_get_text(root),value) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
      if (auto found = self(self,lv_obj_get_child(root,i),value)) return found;
    return nullptr;
  };
  auto capture = [&](const char* name, bool modal = false) {
    auto root = modal ? lv_layer_top() : screenRoots[screens_get_current()];
    lv_obj_update_layout(root); failures += audit_labels(root,name);
    if (!directory) return;
    std::filesystem::create_directories(directory);
    auto shot = lv_snapshot_take(root,LV_COLOR_FORMAT_ARGB8888);
    if (shot) { sim_write_bmp_argb8888((std::filesystem::path(directory)/name).string().c_str(),shot); lv_draw_buf_destroy(shot); }
    else ++failures;
  };
  auto enter = [&](const char* value, bool confirm = true) {
    auto field = sim_find_type(lv_layer_top(),&lv_textarea_class);
    auto kb = sim_find_type(lv_layer_top(),&lv_keyboard_class);
    if (!field || !kb) return false;
    lv_textarea_set_text(field,value); lv_obj_send_event(kb,confirm ? LV_EVENT_READY : LV_EVENT_CANCEL,nullptr);
    sim_pump(80); return true;
  };
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  speed_slider_set(0.08f); screens_show(SCREEN_PROGRAMS); sim_pump(80);
  const auto before = g_presets;
  if (!sim_click_label("+ NEW")) return 1;
  capture("01_new_program.bmp");
  auto draft = screen_program_edit_get_preset();
  if (!sim_click_label("PULSE") || !sim_click_label("PULSE")) return 2;
  if (!sim_expect(draft->mode == STATE_PULSE && (draft->mode_mask & PRESET_MASK_PULSE),"repeated mode selection removed the run mode")) return 3;
  if (!sim_click_label("STEP ON") || !sim_expect(!(draft->mode_mask & PRESET_MASK_STEP),"available-mode removal failed")) return 4;
  if (!sim_click_label("STEP") || !sim_expect(draft->mode_mask & PRESET_MASK_STEP,"selected run mode was not made available")) return 5;
  capture("02_step_program.bmp");
  if (!sim_click_label("CONTINUOUS") || !sim_click_label("New Program")) return 6;
  capture("03_name_editor.bmp",true);
  // Character-count limits must not allow a truncated UTF-8 name in the 32-byte field.
  const char* tooLong = "\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8\xc3\xb8";
  if (!enter(tooLong) || !sim_expect(sim_find_type(lv_layer_top(),&lv_textarea_class) && strcmp(draft->name,"New Program") == 0,"oversized UTF-8 name was accepted")) return 7;
  if (!enter("  ROOT PASS  ") || !sim_expect(strcmp(draft->name,"ROOT PASS") == 0,"program name trim/confirm failed")) return 8;
  if (!sim_click_label("ROOT PASS") || !enter("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW")) return 23;
  capture("07_long_name.bmp");
  if (!sim_click_label("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW") || !enter("ROOT PASS")) return 24;
  if (!sim_click_label("ROOT PASS") || !enter("Discard me",false) || !sim_expect(strcmp(draft->name,"ROOT PASS") == 0,"keyboard cancel changed the draft")) return 9;
  char rpmText[16]; ui_format_rpm(rpmText,sizeof(rpmText),draft->rpm);
  if (!sim_click_label(rpmText) || !enter("4")) return 10;
  capture("04_rpm_validation.bmp",true);
  if (!sim_expect(sim_find_type(lv_layer_top(),&lv_textarea_class) && fabs(draft->rpm-0.08f) < 0.00001f,"invalid RPM closed the editor or changed the draft")) return 11;
  auto cancelInput = sim_find_label_target(lv_layer_top(),"CANCEL",false,true);
  if (!cancelInput) return 30;
  lv_obj_send_event(cancelInput,LV_EVENT_CLICKED,nullptr); sim_pump(80);
  if (!sim_expect(!sim_find_type(lv_layer_top(),&lv_textarea_class) && fabs(draft->rpm-0.08f) < 0.00001f,"RPM Cancel changed the draft")) return 31;
  if (!sim_click_label(rpmText)) return 32;
  if (!enter("0,075") || !sim_expect(fabs(draft->rpm-0.075f) < 0.00001f,"exact decimal RPM was lost")) return 12;
  if (!sim_click_label("+") || !sim_expect(fabs(draft->rpm-0.076f) < 0.00001f,"low-speed adjustment was too coarse")) return 13;
  for (const char* mode : {"CONTINUOUS", "PULSE", "STEP"}) {
    if (!sim_click_label(mode) || !sim_click_label("MODE SETTINGS >")) return 25;
    const char* title = strcmp(mode,"STEP") == 0 ? "SPEED / RPM" : "TARGET SPEED / RPM";
    auto titleLabel = findLabel(findLabel,lv_screen_active(),title);
    auto card = titleLabel ? lv_obj_get_parent(titleLabel) : nullptr;
    if (!sim_expect(card && findLabel(findLabel,card,"0.076"),"sub-editor rounded low RPM")) return 26;
    auto plus = sim_find_label_target(card,"+",false,true);
    if (!plus) return 27;
    lv_obj_send_event(plus,LV_EVENT_CLICKED,nullptr); sim_pump(60);
    if (!sim_expect(findLabel(findLabel,card,"0.077"),"sub-editor low-speed increment was too coarse")) return 28;
    if (!sim_click_label("CANCEL") || !sim_expect(fabs(draft->rpm-0.076f) < 0.00001f,"sub-editor Cancel changed the draft")) return 29;
  }
  if (!sim_click_label("PULSE")) return 14;
  capture("05_pulse_program.bmp");
  if (!sim_click_label("MODE SETTINGS >") || !sim_click_label("SAVE")) return 15;
  if (!sim_expect(strcmp(draft->name,"ROOT PASS") == 0 && fabs(draft->rpm-0.076f) < 0.00001f,"sub-editor return lost program draft")) return 16;
  simulator_set_scenario("nvs-failure");
  if (!sim_click_label("SAVE")) return 17;
  sim_pump(600);
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAM_EDIT &&
       sim_find_label_target(screenRoots[SCREEN_PROGRAM_EDIT],"SAVE FAILED / RETRYING",false,false),
       "program save failure was hidden")) return 33;
  auto pendingPlus = sim_find_label_target(screenRoots[SCREEN_PROGRAM_EDIT],"+",false,true);
  auto pendingMode = sim_find_label_target(screenRoots[SCREEN_PROGRAM_EDIT],"STEP",false,true);
  if (!sim_expect(pendingPlus && pendingMode && lv_obj_has_state(pendingPlus,LV_STATE_DISABLED) &&
       lv_obj_has_state(pendingMode,LV_STATE_DISABLED),"saving draft remained editable")) return 35;
  lv_obj_send_event(pendingPlus,LV_EVENT_CLICKED,nullptr);
  lv_obj_send_event(pendingMode,LV_EVENT_CLICKED,nullptr);
  if (!sim_expect(draft->mode == STATE_PULSE && fabs(draft->rpm-0.076f) < 0.00001f,
       "late edit changed the in-flight saved draft")) return 36;
  simulator_set_scenario("none"); sim_pump(1500);
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS,"program save did not wait for durable receipt")) return 34;
  if (!sim_expect(g_presets.size() == before.size()+1 && strcmp(g_presets.back().name,"ROOT PASS") == 0 &&
                  g_presets.back().mode == STATE_PULSE && fabs(g_presets.back().rpm-0.076f) < 0.00001f,"saved program does not match draft")) return 18;
  capture("06_saved_programs.bmp");
  if (!sim_click_label("+ NEW") || !sim_click_label("CANCEL") || !sim_expect(g_presets.size() == before.size()+1,"Cancel created a program")) return 19;
  if (!sim_click_label("+ NEW") || !sim_click_label("New Program")) return 20;
  screens_show(SCREEN_SETTINGS); sim_pump(80);
  if (!sim_expect(sim_find_type(lv_layer_top(),&lv_keyboard_class) == nullptr,"program keyboard leaked across navigation")) return 21;
  xSemaphoreTake(g_presets_mutex,portMAX_DELAY); g_presets = before; xSemaphoreGive(g_presets_mutex);
  screens_show(SCREEN_MAIN); sim_pump(50); return failures ? 22 : 0;
}

int run_calibration_test(const char* directory) {
  unsigned failures = 0;
  auto capture = [&](const char* name) {
    lv_obj_update_layout(screenRoots[SCREEN_CALIBRATION]);
    failures += audit_labels(screenRoots[SCREEN_CALIBRATION], name);
    if (!directory) return;
    std::filesystem::create_directories(directory);
    auto shot = lv_snapshot_take(screenRoots[SCREEN_CALIBRATION], LV_COLOR_FORMAT_ARGB8888);
    if (shot) { sim_write_bmp_argb8888((std::filesystem::path(directory)/name).string().c_str(),shot); lv_draw_buf_destroy(shot); }
  };
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  screens_show(SCREEN_MAIN); sim_pump(50); g_settings.calibration_factor = 1;
  simulator_fast_motion(true); screens_show(SCREEN_CALIBRATION); sim_pump(50);
  if (!sim_hold_label("JOG +", 260)) return 18;
  if (!sim_expect(control_get_state() == STATE_JOG, "calibration jog failed across enable settle")) return 18;
  if (!sim_send_label_event("JOG +", LV_EVENT_RELEASED)) return 18;
  if (!sim_expect(control_get_state() == STATE_IDLE, "calibration jog release failed to stop")) return 18;
  capture("01_align.bmp");
  if (!sim_click_label("MOVE 360")) return 1;
  capture("02_moving.bmp");
  if (!sim_click_label("STOP")) return 2;
  sim_pump(300);
  auto save = sim_find_active_label_target("SAVE CALIBRATION");
  if (!sim_expect(save && lv_obj_is_disabled(save), "interrupted calibration allowed Save")) return 3;
  if (!sim_click_label("RESTART") || !sim_click_label("MOVE 360")) return 4;
  sim_pump(900); capture("03_measure.bmp");
  if (!sim_enter_calibration_measurement("345") || !sim_click_label("APPLY MEASUREMENT")) return 5;
  if (!sim_expect(std::fabs(g_settings.calibration_factor-1) < 0.00001f && calibration_get_factor() > 1,
                  "unverified calibration leaked into stored settings")) return 6;
  capture("04_verify_ready.bmp");
  if (!sim_click_label("VERIFY 360")) return 7;
  sim_pump(900);
  if (!sim_enter_calibration_measurement("359")) return 8;
  capture("05_verify_failed.bmp");
  if (!sim_expect(lv_obj_is_disabled(save), "failed verification allowed Save")) return 9;
  if (!sim_click_label("VERIFY 360")) return 10;
  sim_pump(900);
  if (!sim_enter_calibration_measurement("360,25")) return 11;
  if (!sim_expect(!lv_obj_is_disabled(save) &&
                  lv_color_to_u32(lv_obj_get_style_bg_color(save, LV_PART_MAIN)) == lv_color_to_u32(COL_ACCENT),
                  "verified calibration Save state and appearance disagree")) return 17;
  capture("06_verified.bmp");
  simulator_set_scenario("nvs-failure");
  if (!sim_click_label("SAVE CALIBRATION")) return 12;
  sim_pump(600); capture("07_save_retry.bmp");
  auto back = sim_find_active_label_target("<  BACK");
  if (!sim_expect(back && lv_obj_is_disabled(back), "calibration allowed exit before save receipt")) return 15;
  if (!sim_click_label("STOP")) return 16;
  simulator_set_scenario("none"); sim_pump(1500); capture("08_saved.bmp");
  if (!sim_expect(sim_find_label_target(screenRoots[SCREEN_CALIBRATION],"Calibration saved",false,false) != nullptr,
                  "calibration save confirmation missing")) return 13;
  screens_show(SCREEN_MAIN); sim_pump(50);
  g_settings.calibration_factor = 1; calibration_discard_draft(); calibration_process_pending();
  simulator_fast_motion(false);
  return failures ? 14 : 0;
}

int run_commissioning_test(const char* directory) {
  int layoutFailures = 0;
  auto capture = [&](const char* name) {
    lv_refr_now(nullptr); layoutFailures += audit_labels(screenRoots[SCREEN_SETUP], name);
    if (!directory) return;
    std::filesystem::create_directories(directory);
    lv_refr_now(nullptr);
    auto shot = lv_snapshot_take(screenRoots[SCREEN_SETUP], LV_COLOR_FORMAT_ARGB8888);
    if (shot) { sim_write_bmp_argb8888((std::filesystem::path(directory) / name).string().c_str(), shot); lv_draw_buf_destroy(shot); }
  };
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  simulator_fast_motion(true);
  screen_setup_begin();
  if (!sim_show_and_check(SCREEN_SETUP)) return 2;
  capture("01_motor.bmp");
  auto next = sim_find_active_label_target("NEXT");
  if (!sim_expect(next && lv_obj_is_disabled(next), "setup skipped unsaved motor config")) return 3;
  if (!sim_click_label("OPEN MOTOR CONFIG") || !sim_click_label("SAVE & APPLY")) return 4;
  sim_pump(600);
  if (!sim_click_back_to(SCREEN_SETUP) || !sim_click_label("NEXT")) return 5;
  capture("02_direction.bmp");
  for (const char* label : {"HOLD CW", "HOLD CCW"}) {
    // Hold like an operator: keep the button down through the ENA settle so
    // the state reaches JOG and the direction is actually exercised.
    if (!sim_hold_label(label, 260)) return 6;
    if (!sim_send_label_event(label, LV_EVENT_RELEASED)) return 6;
    sim_pump(80);
  }
  if (!sim_click_label("DIRECTION CORRECT") || !sim_click_label("NEXT")) return 7;
  sim_pump(40);  // let SETUP rebuild at the calibration stage
  capture("03_calibration.bmp");
  if (!sim_click_label("OPEN CALIBRATION") || !sim_click_label("MOVE 360")) return 8;
  sim_pump(900);
  if (!sim_enter_calibration_measurement() || !sim_click_label("APPLY MEASUREMENT") || !sim_click_label("VERIFY 360")) return 9;
  sim_pump(900);
  if (!sim_enter_calibration_measurement() || !sim_click_label("SAVE CALIBRATION")) return 10;
  sim_pump(600);
  if (!sim_click_back_to(SCREEN_SETUP) || !sim_click_label("NEXT")) return 11;
  capture("04_function_check.bmp");
  next = sim_find_active_label_target("NEXT");
  if (!sim_expect(next && lv_obj_is_disabled(next), "setup skipped physical function check")) return 12;
  simulator_set_estop_input(true); estop_overlay_show(); sim_pump(100);
  simulator_set_estop_input(false); sim_pump(300);
  auto reset = sim_find_label_target(lv_layer_top(), "RESET TO IDLE", false, true);
  if (!sim_expect(reset && !lv_obj_is_disabled(reset), "wizard reset unavailable")) return 13;
  lv_obj_send_event(reset, LV_EVENT_CLICKED, nullptr); sim_pump(80); estop_overlay_hide();
  if (!sim_expect(control_get_state() == STATE_IDLE, "reset restarted motion")) return 14;
  if (!sim_click_label("TEST START")) return 15;
  sim_pump(260);  // ENA settle: the wizard must observe STATE_RUNNING
  if (!sim_click_label("STOP ROTATION")) return 15;
  simulator_set_scenario("nvs-failure");
  if (!sim_click_label("NEXT")) return 16;
  sim_pump(600);
  auto finish = sim_find_active_label_target("FINISH");
  if (!sim_expect(finish && lv_obj_is_disabled(finish), "setup finished before durable save")) return 17;
  capture("05_save_failed.bmp");
  simulator_set_scenario("none"); sim_pump(1500);
  capture("06_complete.bmp");
  if (!sim_click_label("FINISH") || !sim_expect(screens_get_current() == SCREEN_MAIN && !control_setup_active(), "wizard did not finish")) return 18;
  simulator_fast_motion(false);
  // Cancellation must preserve configured data and cancel a queued start.
  g_settings.setup_completed = false; screens_show_startup(); sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_SETUP, "new installation did not offer setup")) return 19;
  if (!sim_click_label("EXIT SETUP")) return 19;
  if (!sim_expect(!control_setup_active() && control_get_state() == STATE_IDLE, "wizard cancellation retained motion")) return 20;
  g_settings.setup_completed = true; screens_show_startup(); sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "existing installation forced through wizard")) return 22;
  return layoutFailures ? 21 : 0;
}
