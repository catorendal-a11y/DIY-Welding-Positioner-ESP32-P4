// The --screenshots dump and the --audit-layout run.
#include "sim_test_support.h"

int run_screenshot_dump(const char* dir) {
  if (!dir || !dir[0]) return 2;
  std::filesystem::create_directories(dir);

  const ScreenId ids[] = {
      SCREEN_BOOT,        SCREEN_MAIN,       SCREEN_MENU,           SCREEN_RUN_MODES,   SCREEN_PULSE,
      SCREEN_STEP,        SCREEN_JOG,        SCREEN_TIMER,          SCREEN_PROGRAMS,    SCREEN_PROGRAM_EDIT,
      SCREEN_EDIT_CONT,   SCREEN_EDIT_PULSE, SCREEN_EDIT_STEP,      SCREEN_SETTINGS,    SCREEN_MOTOR_CONFIG,
      SCREEN_CALIBRATION, SCREEN_DISPLAY,    SCREEN_PEDAL_SETTINGS, SCREEN_DIAGNOSTICS, SCREEN_SYSINFO,
      SCREEN_ABOUT,       SCREEN_CONFIRM,
  };

  for (ScreenId id : ids) {
    if (!sim_show_and_check(id)) return 3;
    lv_refr_now(nullptr);
    lv_draw_buf_t* shot = lv_snapshot_take(screenRoots[id], LV_COLOR_FORMAT_ARGB8888);
    if (!shot) {
      std::fprintf(stderr, "SCREENSHOT FAIL: %s\n", sim_screen_name(id));
      return 4;
    }
    char path[512];
    std::snprintf(path, sizeof(path), "%s/%02d_%s.bmp", dir, (int)id, sim_screen_name(id));
    bool ok = sim_write_bmp_argb8888(path, shot);
    lv_draw_buf_destroy(shot);
    if (!ok) {
      std::fprintf(stderr, "SCREENSHOT WRITE FAIL: %s\n", path);
      return 5;
    }
    std::printf("SCREENSHOT: %s\n", path);
  }

  screens_show(SCREEN_MAIN);
  for (bool active : {true, false}) {
    simulator_set_estop_input(active);
    estop_overlay_show();
    sim_pump(300);
    lv_refr_now(nullptr);
    lv_draw_buf_t* shot = lv_snapshot_take(lv_layer_top(), LV_COLOR_FORMAT_ARGB8888);
    if (!shot) return 4;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/ESTOP_%s.bmp", dir, active ? "ACTIVE" : "RESET");
    bool ok = sim_write_bmp_argb8888(path, shot);
    lv_draw_buf_destroy(shot);
    if (!ok) return 5;
    estop_overlay_hide();
  }
  simulator_set_estop_input(false);
  return 0;
}

int run_layout_audit() {
  unsigned failures = 0;
  speed_slider_set(MIN_RPM);
  for (int id = SCREEN_MAIN; id < SCREEN_COUNT; ++id) {
    screens_show(static_cast<ScreenId>(id)); sim_pump(250);
    lv_obj_update_layout(screenRoots[id]);
    failures += audit_labels(screenRoots[id], sim_screen_name(static_cast<ScreenId>(id)));
    if (id == SCREEN_PROGRAM_EDIT) {
      for (const char* text : {"CANCEL", "SAVE"}) {
        auto button = sim_find_label_target(screenRoots[id], text, false, true);
        if (!button || lv_obj_get_y(button) < 400) { std::printf("LAYOUT EDITOR FOOTER: %s\n", text); ++failures; }
      }
      for (const char* text : {"CONTINUOUS", "PULSE", "STEP"}) {
        auto button = sim_find_label_target(screenRoots[id], text, false, true);
        if (!button || lv_obj_get_y(button) < 160) { std::printf("LAYOUT EDITOR MODE: %s\n", text); ++failures; }
      }
    }
  }
  for (const char* scenario : {"estop", "driver-alarm", "stale-adc", "i2c-failure", "hmi-stale", "speed-pot", "storage", "unexpected-reset"}) {
    simulator_set_scenario(scenario); estop_overlay_show(); sim_pump(100);
    failures += audit_labels(lv_layer_top(), scenario);
    estop_overlay_hide(); simulator_set_scenario("none"); control_transition_to(STATE_IDLE);
  }
  std::printf("LAYOUT AUDIT: %u failures\n", failures);
  return failures ? 1 : 0;
}
