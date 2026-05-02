// PC Simulator - LVGL SDL host for the rotator HMI

#include "lvgl.h"
#include "src/draw/snapshot/lv_snapshot.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mousewheel.h"

#include "../src/control/control.h"
#include "../src/motor/speed.h"
#include "../src/ui/screens.h"
#include "../src/ui/theme.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <vector>

void simulator_init_state();
void simulator_tick();

static void sim_pump(uint32_t ms) {
  uint32_t start = lv_tick_get();
  do {
    screens_process_pending();
    simulator_tick();
    screens_update_current();
    uint32_t waitMs = lv_timer_handler();
    if (waitMs == LV_NO_TIMER_READY || waitMs > 10) {
      waitMs = 5;
    }
    lv_delay_ms(waitMs);
  } while ((lv_tick_get() - start) < ms);
}

static const char* sim_screen_name(ScreenId id) {
  switch (id) {
    case SCREEN_NONE: return "NONE";
    case SCREEN_MAIN: return "MAIN";
    case SCREEN_MENU: return "MENU";
    case SCREEN_PULSE: return "PULSE";
    case SCREEN_STEP: return "STEP";
    case SCREEN_JOG: return "JOG";
    case SCREEN_TIMER: return "TIMER";
    case SCREEN_PROGRAMS: return "PROGRAMS";
    case SCREEN_PROGRAM_EDIT: return "PROGRAM_EDIT";
    case SCREEN_SETTINGS: return "SETTINGS";
    case SCREEN_CONFIRM: return "CONFIRM";
    case SCREEN_BOOT: return "BOOT";
    case SCREEN_EDIT_PULSE: return "EDIT_PULSE";
    case SCREEN_EDIT_STEP: return "EDIT_STEP";
    case SCREEN_EDIT_CONT: return "EDIT_CONT";
    case SCREEN_SYSINFO: return "SYSINFO";
    case SCREEN_CALIBRATION: return "CALIBRATION";
    case SCREEN_MOTOR_CONFIG: return "MOTOR_CONFIG";
    case SCREEN_DISPLAY: return "DISPLAY";
    case SCREEN_PEDAL_SETTINGS: return "PEDAL_SETTINGS";
    case SCREEN_DIAGNOSTICS: return "DIAGNOSTICS";
    case SCREEN_ABOUT: return "ABOUT";
    case SCREEN_RUN_MODES: return "RUN_MODES";
    case SCREEN_COUNT: return "COUNT";
  }
  return "UNKNOWN";
}

static bool sim_fail(const char* msg) {
  std::fprintf(stderr, "SELFTEST FAIL: %s\n", msg);
  return false;
}

static bool sim_expect(bool condition, const char* msg) {
  return condition ? true : sim_fail(msg);
}

static lv_obj_t* sim_clickable_ancestor(lv_obj_t* obj) {
  obj = lv_obj_get_parent(obj);
  while (obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE)) {
      return obj;
    }
    obj = lv_obj_get_parent(obj);
  }
  return nullptr;
}

static lv_obj_t* sim_button_ancestor(lv_obj_t* obj) {
  obj = lv_obj_get_parent(obj);
  while (obj) {
    if (lv_obj_check_type(obj, &lv_button_class)) {
      return obj;
    }
    obj = lv_obj_get_parent(obj);
  }
  return nullptr;
}

static lv_obj_t* sim_find_label_target(lv_obj_t* obj, const char* text, bool contains, bool buttonOnly) {
  if (!obj) return nullptr;

  if (lv_obj_check_type(obj, &lv_label_class)) {
    const char* labelText = lv_label_get_text(obj);
    bool match = false;
    if (labelText) {
      match = contains ? (std::strstr(labelText, text) != nullptr)
                       : (std::strcmp(labelText, text) == 0);
    }
    if (match) {
      lv_obj_t* target = buttonOnly ? sim_button_ancestor(obj) : sim_clickable_ancestor(obj);
      if (target) return target;
    }
  }

  uint32_t count = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < count; i++) {
    lv_obj_t* found = sim_find_label_target(lv_obj_get_child(obj, i), text, contains, buttonOnly);
    if (found) return found;
  }
  return nullptr;
}

static lv_obj_t* sim_find_active_label_target(const char* text, bool contains = false) {
  ScreenId current = screens_get_current();
  if (current <= SCREEN_NONE || current >= SCREEN_COUNT) return nullptr;
  lv_obj_t* button = sim_find_label_target(screenRoots[current], text, contains, true);
  if (button) return button;
  return sim_find_label_target(screenRoots[current], text, contains, false);
}

static bool sim_send_label_event(const char* text, lv_event_code_t event, bool contains = false) {
  lv_obj_t* target = sim_find_active_label_target(text, contains);
  if (!target) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "label target not found: %s on %s", text, sim_screen_name(screens_get_current()));
    return sim_fail(buf);
  }
  lv_obj_send_event(target, event, nullptr);
  sim_pump(60);
  return true;
}

static bool sim_click_label(const char* text, bool contains = false) {
  return sim_send_label_event(text, LV_EVENT_CLICKED, contains);
}

static bool sim_show_and_check(ScreenId id) {
  if (id == SCREEN_PROGRAM_EDIT) {
    screens_set_edit_slot(0);
  }
  screens_show(id);
  sim_pump(80);
  if (screens_get_current() != id) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "screen switch failed: wanted %s got %s",
                  sim_screen_name(id), sim_screen_name(screens_get_current()));
    return sim_fail(buf);
  }
  if (!screenRoots[id]) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "screen root missing: %s", sim_screen_name(id));
    return sim_fail(buf);
  }
  if (lv_obj_get_child_count(screenRoots[id]) == 0) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "screen has no widgets: %s", sim_screen_name(id));
    return sim_fail(buf);
  }
  return true;
}

static bool sim_click_back_to(ScreenId expected) {
  if (sim_find_active_label_target("<  BACK")) {
    if (!sim_click_label("<  BACK")) return false;
  } else if (sim_find_active_label_target("< BACK")) {
    if (!sim_click_label("< BACK")) return false;
  } else if (sim_find_active_label_target("BACK")) {
    if (!sim_click_label("BACK")) return false;
  } else if (sim_find_active_label_target("CANCEL")) {
    if (!sim_click_label("CANCEL")) return false;
  } else {
    return sim_fail("no back/cancel button found");
  }
  return sim_expect(screens_get_current() == expected, "back navigation landed on wrong screen");
}

static bool sim_test_all_screens_create_update() {
  const ScreenId ids[] = {
    SCREEN_BOOT, SCREEN_MAIN, SCREEN_MENU, SCREEN_RUN_MODES, SCREEN_PULSE,
    SCREEN_STEP, SCREEN_JOG, SCREEN_TIMER, SCREEN_PROGRAMS, SCREEN_PROGRAM_EDIT,
    SCREEN_EDIT_CONT, SCREEN_EDIT_PULSE, SCREEN_EDIT_STEP, SCREEN_SETTINGS,
    SCREEN_MOTOR_CONFIG, SCREEN_CALIBRATION, SCREEN_DISPLAY, SCREEN_PEDAL_SETTINGS,
    SCREEN_DIAGNOSTICS, SCREEN_SYSINFO, SCREEN_ABOUT, SCREEN_CONFIRM,
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

  if (!sim_click_label("> START")) return false;
  if (!sim_expect(control_get_state() == STATE_RUNNING, "main START did not enter RUNNING")) return false;
  if (!sim_click_label("X STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "main STOP did not return IDLE")) return false;

  if (!sim_send_label_event("JOG", LV_EVENT_PRESSED)) return false;
  if (!sim_expect(control_get_state() == STATE_JOG, "main JOG press did not enter JOG")) return false;
  if (!sim_send_label_event("JOG", LV_EVENT_RELEASED)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "main JOG release did not stop")) return false;

  Direction before = speed_get_direction();
  if (!sim_click_label("CW")) return false;
  if (!sim_expect(speed_get_direction() != before, "direction toggle did not change direction")) return false;

  if (!sim_click_label("PULSE")) return false;
  if (!sim_expect(control_get_state() == STATE_PULSE, "main PULSE did not enter PULSE")) return false;
  if (!sim_click_label("X STOP")) return false;
  return sim_expect(control_get_state() == STATE_IDLE, "STOP after PULSE did not return IDLE");
}

static bool sim_test_navigation_flows() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;
  if (!sim_click_label("MENU")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MENU, "MENU button did not open menu")) return false;

  if (!sim_click_label("RUN MODES")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_RUN_MODES, "RUN MODES did not open run mode picker")) return false;

  if (!sim_click_label("PULSE")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PULSE, "run mode PULSE did not open pulse screen")) return false;
  if (!sim_click_label("> START")) return false;
  if (!sim_expect(control_get_state() == STATE_PULSE, "pulse START did not enter PULSE")) return false;
  if (!sim_click_label("[] STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "pulse STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("STEP")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_STEP, "run mode STEP did not open step screen")) return false;
  if (!sim_click_label("> STEP")) return false;
  if (!sim_expect(control_get_state() == STATE_STEP, "step button did not enter STEP")) return false;
  if (!sim_click_label("X STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "step STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("JOG")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_JOG, "run mode JOG did not open jog screen")) return false;
  if (!sim_send_label_event("HOLD CW", LV_EVENT_PRESSED)) return false;
  if (!sim_expect(control_get_state() == STATE_JOG, "jog screen CW press did not enter JOG")) return false;
  if (!sim_send_label_event("HOLD CW", LV_EVENT_RELEASED)) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "jog screen CW release did not stop")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("3-2-1")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_TIMER, "run mode timer did not open timer screen")) return false;
  g_settings.countdown_seconds = 1;
  if (!sim_click_label("> START")) return false;
  sim_pump(1400);
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "timer countdown did not return to main")) return false;
  if (!sim_expect(control_get_state() == STATE_RUNNING, "timer countdown did not start RUNNING")) return false;
  if (!sim_click_label("X STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "timer STOP did not return IDLE")) return false;

  return true;
}

static bool sim_test_settings_and_programs() {
  if (!sim_show_and_check(SCREEN_MENU)) return false;
  if (!sim_click_label("SETUP")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_SETTINGS, "SETUP did not open settings")) return false;

  struct NavCase {
    const char* label;
    ScreenId dest;
  };
  const NavCase settingsCases[] = {
    {"Motor Configuration", SCREEN_MOTOR_CONFIG},
    {"Calibration", SCREEN_CALIBRATION},
    {"Display Settings", SCREEN_DISPLAY},
    {"Pedal Settings", SCREEN_PEDAL_SETTINGS},
    {"Diagnostics", SCREEN_DIAGNOSTICS},
    {"System Info", SCREEN_SYSINFO},
    {"About", SCREEN_ABOUT},
  };

  for (const NavCase& c : settingsCases) {
    if (!sim_show_and_check(SCREEN_SETTINGS)) return false;
    if (!sim_click_label(c.label)) return false;
    if (!sim_expect(screens_get_current() == c.dest, "settings row opened wrong screen")) return false;
    if (!sim_click_back_to(SCREEN_SETTINGS)) return false;
  }

  if (!sim_show_and_check(SCREEN_PROGRAMS)) return false;
  if (!sim_click_label("+ NEW")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAM_EDIT, "+ NEW did not open program edit")) return false;
  if (!sim_click_label("CONT -", true)) return false;
  if (!sim_expect(screens_get_current() == SCREEN_EDIT_CONT, "mode settings did not open continuous edit")) return false;
  if (!sim_click_back_to(SCREEN_PROGRAM_EDIT)) return false;
  if (!sim_click_label("CANCEL")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAMS, "program edit cancel did not return programs")) return false;

  return true;
}

static bool s_confirmHit = false;
static bool s_cancelHit = false;

static void sim_confirm_cb() {
  s_confirmHit = true;
}

static void sim_cancel_cb() {
  s_cancelHit = true;
}

static bool sim_test_confirm_and_overlay() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;

  s_cancelHit = false;
  screen_confirm_create("TEST CANCEL", "Cancel path.", sim_confirm_cb, sim_cancel_cb, SCREEN_NONE);
  sim_pump(80);
  if (!sim_expect(screens_get_current() == SCREEN_CONFIRM, "confirm dialog did not open")) return false;
  if (!sim_click_label("CANCEL")) return false;
  sim_pump(80);
  if (!sim_expect(s_cancelHit, "confirm cancel callback did not run")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "confirm cancel did not return")) return false;

  s_confirmHit = false;
  screen_confirm_create("TEST CONFIRM", "Confirm path.", sim_confirm_cb, sim_cancel_cb, SCREEN_MAIN);
  sim_pump(80);
  if (!sim_click_label("CONFIRM")) return false;
  sim_pump(80);
  if (!sim_expect(s_confirmHit, "confirm callback did not run")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "confirm success did not return to main")) return false;

  estop_overlay_show();
  sim_pump(40);
  if (!sim_expect(estop_overlay_visible(), "ESTOP overlay did not show")) return false;
  estop_overlay_hide();
  sim_pump(40);
  return sim_expect(!estop_overlay_visible(), "ESTOP overlay did not hide");
}

static int run_self_test() {
  std::puts("SIM SELFTEST: start");
  g_settings.countdown_seconds = 1;
  if (!sim_test_all_screens_create_update()) return 2;
  std::puts("SIM SELFTEST: screens ok");
  if (!sim_test_main_controls()) return 3;
  std::puts("SIM SELFTEST: main controls ok");
  if (!sim_test_navigation_flows()) return 4;
  std::puts("SIM SELFTEST: run mode flows ok");
  if (!sim_test_settings_and_programs()) return 5;
  std::puts("SIM SELFTEST: settings/programs ok");
  if (!sim_test_confirm_and_overlay()) return 6;
  std::puts("SIM SELFTEST: confirm/overlay ok");
  std::puts("SIM SELFTEST: PASS");
  return 0;
}

static bool sim_write_bmp_argb8888(const char* path, const lv_draw_buf_t* buf) {
  if (!path || !buf || !buf->data || buf->header.cf != LV_COLOR_FORMAT_ARGB8888) return false;

  const uint32_t w = buf->header.w;
  const uint32_t h = buf->header.h;
  const uint32_t srcStride = buf->header.stride;
  const uint32_t rowStride = ((w * 3u + 3u) / 4u) * 4u;
  const uint32_t pixelBytes = rowStride * h;
  const uint32_t fileBytes = 54u + pixelBytes;

  FILE* f = std::fopen(path, "wb");
  if (!f) return false;

  uint8_t header[54] = {};
  header[0] = 'B';
  header[1] = 'M';
  header[2] = (uint8_t)(fileBytes);
  header[3] = (uint8_t)(fileBytes >> 8);
  header[4] = (uint8_t)(fileBytes >> 16);
  header[5] = (uint8_t)(fileBytes >> 24);
  header[10] = 54;
  header[14] = 40;
  header[18] = (uint8_t)(w);
  header[19] = (uint8_t)(w >> 8);
  header[20] = (uint8_t)(w >> 16);
  header[21] = (uint8_t)(w >> 24);
  header[22] = (uint8_t)(h);
  header[23] = (uint8_t)(h >> 8);
  header[24] = (uint8_t)(h >> 16);
  header[25] = (uint8_t)(h >> 24);
  header[26] = 1;
  header[28] = 24;
  std::fwrite(header, 1, sizeof(header), f);

  std::vector<uint8_t> row(rowStride, 0);
  const uint8_t* data = (const uint8_t*)buf->data;
  for (int32_t y = (int32_t)h - 1; y >= 0; y--) {
    const uint8_t* src = data + (size_t)y * srcStride;
    for (uint32_t x = 0; x < w; x++) {
      row[x * 3u + 0u] = src[x * 4u + 0u];
      row[x * 3u + 1u] = src[x * 4u + 1u];
      row[x * 3u + 2u] = src[x * 4u + 2u];
    }
    std::fwrite(row.data(), 1, rowStride, f);
  }

  std::fclose(f);
  return true;
}

static int run_screenshot_dump(const char* dir) {
  if (!dir || !dir[0]) return 2;
  _mkdir(dir);

  const ScreenId ids[] = {
    SCREEN_BOOT, SCREEN_MAIN, SCREEN_MENU, SCREEN_RUN_MODES, SCREEN_PULSE,
    SCREEN_STEP, SCREEN_JOG, SCREEN_TIMER, SCREEN_PROGRAMS, SCREEN_PROGRAM_EDIT,
    SCREEN_EDIT_CONT, SCREEN_EDIT_PULSE, SCREEN_EDIT_STEP, SCREEN_SETTINGS,
    SCREEN_MOTOR_CONFIG, SCREEN_CALIBRATION, SCREEN_DISPLAY, SCREEN_PEDAL_SETTINGS,
    SCREEN_DIAGNOSTICS, SCREEN_SYSINFO, SCREEN_ABOUT, SCREEN_CONFIRM,
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

  return 0;
}

int main(int argc, char** argv) {
  bool selfTest = argc > 1 && std::strcmp(argv[1], "--self-test") == 0;
  bool screenshots = argc > 2 && std::strcmp(argv[1], "--screenshots") == 0;

  simulator_init_state();

  lv_init();
  lv_display_t* display = lv_sdl_window_create(SCREEN_W, SCREEN_H);
  if (!display) {
    return 1;
  }

  lv_sdl_window_set_title(display, "DIY Welding Positioner ESP32-P4 - LVGL Simulator");
  lv_sdl_mouse_create();
  lv_sdl_mousewheel_create();
  lv_sdl_keyboard_create();

  theme_init();
  screens_init();
  screens_show(SCREEN_MAIN);

  if (selfTest) {
    return run_self_test();
  }
  if (screenshots) {
    return run_screenshot_dump(argv[2]);
  }

  uint32_t lastScreenUpdate = 0;
  while (lv_display_get_next(nullptr) != nullptr) {
    screens_process_pending();
    uint32_t now = lv_tick_get();
    if (now - lastScreenUpdate >= 200) {
      lastScreenUpdate = now;
      simulator_tick();
      screens_update_current();
    }

    uint32_t waitMs = lv_timer_handler();
    if (waitMs == LV_NO_TIMER_READY || waitMs > 25) {
      waitMs = 5;
    }
    lv_delay_ms(waitMs);
  }

  return 0;
}
