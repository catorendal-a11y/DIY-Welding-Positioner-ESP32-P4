// PC Simulator - LVGL SDL host for the rotator HMI

#include "sim_test_support.h"

struct SimSaverInput {
  lv_indev_t* device = nullptr;
  lv_indev_read_cb_t read = nullptr;
  ScreenSaverInput input;
};
static SimSaverInput simSaverInputs[3];
static void sim_attach_saver_input(lv_indev_t* device) {
  for (auto& slot : simSaverInputs) if (!slot.device) {
    slot.device = device; slot.read = lv_indev_get_read_cb(device);
    lv_indev_set_read_cb(device, [](lv_indev_t* indev, lv_indev_data_t* data) {
      for (auto& source : simSaverInputs) if (source.device == indev) {
        source.read(indev, data);
        screen_saver_filter_input(data, source.input); return;
      }
    });
    return;
  }
}

#include "build_identity.h"
int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--build-info") == 0) {
    std::puts(simulator_build_identity);
    return 0;
  }
  bool selfTest = argc > 1 && std::strcmp(argv[1], "--self-test") == 0;
  bool screenshots = argc > 2 && std::strcmp(argv[1], "--screenshots") == 0;
  bool auditLayout = argc > 1 && std::strcmp(argv[1], "--audit-layout") == 0;

  simulator_init_state();

  lv_init();
  lv_display_t* display = lv_sdl_window_create(SCREEN_W, SCREEN_H);
  if (!display) {
    return 1;
  }

  lv_sdl_window_set_title(display, "DIY Welding Positioner ESP32-P4 - LVGL Simulator");
  // Preserve each SDL driver's user data; wrap its read callback only.
  sim_attach_saver_input(lv_sdl_mouse_create());
  sim_attach_saver_input(lv_sdl_mousewheel_create());
  sim_attach_saver_input(lv_sdl_keyboard_create());

  theme_init();
  screens_init();
  screens_show(SCREEN_MAIN);
  if (argc > 2 && std::strcmp(argv[1], "--scenario") == 0) {
    if (!simulator_set_scenario(argv[2])) {
      std::fprintf(stderr, "Unknown scenario: %s\n", argv[2]); return 2;
    }
    if (control_get_state() == STATE_ESTOP) estop_overlay_show();
  }
  if (auditLayout) return run_layout_audit();

  if (argc > 2 && std::strcmp(argv[1], "--screensaver-preview") == 0) return run_screen_saver_test(argv[2]);
  if (argc > 2 && std::strcmp(argv[1], "--motor-preview") == 0) return run_motor_config_test(argv[2]);
  if (argc > 2 && std::strcmp(argv[1], "--program-preview") == 0) return run_program_edit_test(argv[2]);
  if (argc > 2 && std::strcmp(argv[1], "--calibration-preview") == 0) return run_calibration_test(argv[2]);
  if (argc > 2 && std::strcmp(argv[1], "--commissioning-preview") == 0) return run_commissioning_test(argv[2]);
  if (selfTest) {
    return run_self_test();
  }
  if (screenshots) {
    return run_screenshot_dump(argv[2]);
  }

  uint32_t lastScreenUpdate = 0, lastControlUpdate = 0;
  while (lv_display_get_next(nullptr) != nullptr) {
    screens_process_pending();
    uint32_t now = lv_tick_get();
    if (now - lastControlUpdate >= 5) { lastControlUpdate = now; simulator_tick(); }
    if (now - lastScreenUpdate >= 40) {
      lastScreenUpdate = now;
      screens_update_current();
    }

    dim_update();
    uint32_t waitMs = lv_timer_handler();
    if (waitMs == LV_NO_TIMER_READY || waitMs > 5) {
      waitMs = 5;
    }
    lv_delay_ms(waitMs);
  }

  return 0;
}
