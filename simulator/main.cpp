// PC Simulator - LVGL SDL host for the rotator HMI

#include "lvgl.h"
#include "include/lvgl/draw/lv_snapshot.h"
#include "include/lvgl/drivers/sdl/lv_sdl_window.h"
#include "include/lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "include/lvgl/drivers/sdl/lv_sdl_keyboard.h"
#include "include/lvgl/drivers/sdl/lv_sdl_mousewheel.h"

#include "../src/control/control.h"
#include "../src/safety/safety.h"
#include "../src/motor/speed.h"
#include "../src/ui/screens.h"
#include "../src/ui/theme.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

void simulator_init_state();
void simulator_tick();
void simulator_set_estop_input(bool active);
void simulator_fast_motion(bool fast);
bool simulator_set_scenario(const char* scenario);

static void sim_pump(uint32_t ms) {
  uint32_t start = lv_tick_get();
  do {
    screens_process_pending();
    simulator_tick();
    screens_update_current();
    estop_overlay_update();
    uint32_t waitMs = lv_timer_handler();
    if (waitMs == LV_NO_TIMER_READY || waitMs > 10) {
      waitMs = 5;
    }
    lv_delay_ms(waitMs);
  } while ((lv_tick_get() - start) < ms);
  // Rendering or the final delay can cross a control deadline. Observe that
  // elapsed time before the test checks state; do not depend on frame speed.
  simulator_tick();
  screens_update_current();
  estop_overlay_update();
}

static const char* sim_screen_name(ScreenId id) {
  switch (id) {
    case SCREEN_NONE:
      return "NONE";
    case SCREEN_MAIN:
      return "MAIN";
    case SCREEN_MENU:
      return "MENU";
    case SCREEN_PULSE:
      return "PULSE";
    case SCREEN_STEP:
      return "STEP";
    case SCREEN_JOG:
      return "JOG";
    case SCREEN_TIMER:
      return "TIMER";
    case SCREEN_PROGRAMS:
      return "PROGRAMS";
    case SCREEN_PROGRAM_EDIT:
      return "PROGRAM_EDIT";
    case SCREEN_SETTINGS:
      return "SETTINGS";
    case SCREEN_CONFIRM:
      return "CONFIRM";
    case SCREEN_BOOT:
      return "BOOT";
    case SCREEN_EDIT_PULSE:
      return "EDIT_PULSE";
    case SCREEN_EDIT_STEP:
      return "EDIT_STEP";
    case SCREEN_EDIT_CONT:
      return "EDIT_CONT";
    case SCREEN_SYSINFO:
      return "SYSINFO";
    case SCREEN_CALIBRATION:
      return "CALIBRATION";
    case SCREEN_MOTOR_CONFIG:
      return "MOTOR_CONFIG";
    case SCREEN_DISPLAY:
      return "DISPLAY";
    case SCREEN_PEDAL_SETTINGS:
      return "PEDAL_SETTINGS";
    case SCREEN_DIAGNOSTICS:
      return "DIAGNOSTICS";
    case SCREEN_ABOUT:
      return "ABOUT";
    case SCREEN_RUN_MODES:
      return "RUN_MODES";
    case SCREEN_SETUP: return "SETUP";
    case SCREEN_COUNT:
      return "COUNT";
  }
  return "UNKNOWN";
}

static bool sim_fail(const char* msg) {
  std::fprintf(stderr, "SELFTEST FAIL: %s\n", msg);
  return false;
}

static bool sim_expect(bool condition, const char* msg) { return condition ? true : sim_fail(msg); }

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
      match = contains ? (std::strstr(labelText, text) != nullptr) : (std::strcmp(labelText, text) == 0);
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
    std::snprintf(buf, sizeof(buf), "label target not found: %s on %s", text,
                  sim_screen_name(screens_get_current()));
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
    std::snprintf(buf, sizeof(buf), "screen switch failed: wanted %s got %s", sim_screen_name(id),
                  sim_screen_name(screens_get_current()));
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

static lv_obj_t* sim_find_action(lv_obj_t* root, UiActionId id) {
  if (lv_obj_get_user_data(root) == (void*)(uintptr_t)id) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    if (lv_obj_t* result = sim_find_action(lv_obj_get_child(root, i), id)) return result;
  return nullptr;
}
static bool sim_click_action(UiActionId id) {
  lv_obj_t* target = sim_find_action(lv_screen_active(), id);
  if (!target || lv_obj_has_state(target, LV_STATE_DISABLED) || lv_obj_has_flag(target, LV_OBJ_FLAG_HIDDEN))
    return sim_fail("action unavailable");
  lv_obj_send_event(target, LV_EVENT_CLICKED, nullptr);
  sim_pump(60);
  return true;
}

static bool sim_test_main_controls() {
  if (!sim_show_and_check(SCREEN_MAIN)) return false;
  float rpmBefore = speed_get_target_rpm();
  if (!sim_click_label("+")) return false;
  if (!sim_expect(speed_get_target_rpm() > rpmBefore, "RPM plus did not increase speed")) return false;
  if (!sim_click_label("-")) return false;
  if (!sim_click_action(UI_ACTION_START)) return false;
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
  if (!sim_click_label("MENU")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MENU, "MENU button did not open menu")) return false;

  if (!sim_click_label("RUN MODES")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_RUN_MODES, "RUN MODES did not open run mode picker"))
    return false;

  if (!sim_click_label("PULSE")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PULSE, "run mode PULSE did not open pulse screen"))
    return false;
  if (!sim_click_label("> START")) return false;
  if (!sim_expect(control_get_state() == STATE_PULSE, "pulse START did not enter PULSE")) return false;
  if (!sim_click_label("[] STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "pulse STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("STEP")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_STEP, "run mode STEP did not open step screen"))
    return false;
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
  if (!sim_expect(screens_get_current() == SCREEN_TIMER, "run mode timer did not open timer screen"))
    return false;
  g_settings.countdown_seconds = 1;
  if (!sim_click_label("> START")) return false;
  sim_pump(3400);
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "timer countdown did not return to main"))
    return false;
  if (!sim_expect(control_get_state() == STATE_RUNNING, "timer countdown did not start RUNNING"))
    return false;
  if (!sim_click_action(UI_ACTION_STOP)) return false;
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
  if (!sim_expect(screens_get_current() == SCREEN_PROGRAM_EDIT, "+ NEW did not open program edit"))
    return false;
  if (!sim_click_label("CONT -", true)) return false;
  if (!sim_expect(screens_get_current() == SCREEN_EDIT_CONT, "mode settings did not open continuous edit"))
    return false;
  float previousRpm = screen_program_edit_get_preset()->rpm;
  if (!sim_click_label("+")) return false;
  sim_pump(450);
  if (!sim_click_label("SAVE")) return false;
  if (!sim_expect(screen_program_edit_get_preset()->rpm > previousRpm,
                  "CONT edits were overwritten by periodic update"))
    return false;
  screens_show(SCREEN_EDIT_PULSE);
  const uint32_t previousOn = screen_program_edit_get_preset()->pulse_on_ms;
  if (!sim_click_label("+")) return false;
  sim_pump(450);
  if (!sim_click_label("SAVE")) return false;
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
  if (!sim_click_label("CANCEL")) return false;
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
  if (!sim_expect(screens_get_current() == SCREEN_MAIN, "confirm success did not return to main"))
    return false;

  simulator_set_estop_input(true);
  g_uiResetPending.store(false);
  estop_overlay_show();
  sim_pump(300);
  lv_obj_t* blockedReset = sim_find_label_target(lv_layer_top(), "RESET BLOCKED", false, true);
  if (!sim_expect(blockedReset && lv_obj_has_state(blockedReset, LV_STATE_DISABLED),
                  "fault reset must be disabled"))
    return false;
  lv_obj_send_event(blockedReset, LV_EVENT_CLICKED, nullptr);
  if (!sim_expect(!g_uiResetPending.load(), "unsafe reset was requested")) return false;
  simulator_set_estop_input(false);
  sim_pump(300);
  lv_obj_t* readyReset = sim_find_label_target(lv_layer_top(), "RESET TO IDLE", false, true);
  if (!sim_expect(readyReset && !lv_obj_has_state(readyReset, LV_STATE_DISABLED), "safe reset unavailable"))
    return false;
  if (!sim_expect(estop_overlay_visible(), "ESTOP overlay did not show")) return false;
  estop_overlay_hide();
  sim_pump(40);
  return sim_expect(!estop_overlay_visible(), "ESTOP overlay did not hide");
}

static lv_obj_t* sim_find_type(lv_obj_t* obj, const lv_obj_class_t* type) {
  if (lv_obj_check_type(obj, type)) return obj;
  for (uint32_t i=0; i<lv_obj_get_child_count(obj); ++i)
    if (auto found = sim_find_type(lv_obj_get_child(obj, i), type)) return found;
  return nullptr;
}
static bool sim_enter_calibration_measurement() {
  if (!sim_click_label("---")) return false;
  auto root = screenRoots[SCREEN_CALIBRATION];
  auto field = sim_find_type(root, &lv_textarea_class);
  auto keyboard = sim_find_type(root, &lv_keyboard_class);
  if (!sim_expect(field && keyboard, "calibration measurement editor missing")) return false;
  lv_textarea_set_text(field, "360"); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr);
  sim_pump(80); return true;
}
static unsigned audit_labels(lv_obj_t* obj, const char* screen);
static int run_commissioning_test(const char* directory = nullptr);
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
  simulator_set_scenario("nvs-failure");
  const uint32_t ticket = storage_request_settings_save();
  sim_pump(600);
  if (!sim_expect(storage_settings_save_status(ticket) == STORAGE_ERROR, "failed save reported success")) return 7;
  simulator_set_scenario("none"); sim_pump(1500);
  if (!sim_expect(storage_settings_save_status(ticket) == STORAGE_SAVED, "save retry did not commit")) return 8;
  control_transition_to(STATE_IDLE);
  sim_pump(30);
  simulator_set_scenario("rejected-motion");
  control_start_continuous(); sim_pump(40);
  if (!sim_expect(control_get_state() == STATE_ESTOP,
                  "rejected motion did not latch fault")) return 9;
  simulator_set_scenario("none"); control_transition_to(STATE_IDLE);
  std::puts("SIM SELFTEST: fault scenarios/save generations ok");
  if (run_commissioning_test() != 0) return 10;
  std::puts("SIM SELFTEST: commissioning UI/save failure/retry ok");
  screens_show(SCREEN_MAIN); sim_pump(80);
  simulator_set_scenario("stalled-control"); sim_pump(160);
  if (!sim_expect(!ui_control_fresh() && !control_start_continuous(), "stale control admitted motion")) return 11;
  auto start = sim_find_label_target(screenRoots[SCREEN_MAIN], "START BLOCKED", false, true);
  if (!sim_expect(start && lv_obj_has_state(start, LV_STATE_DISABLED), "stale START not disabled")) return 12;
  control_stop(); sim_pump(60);
  if (!sim_expect(control_get_state() == STATE_ESTOP, "stalled executor did not fault on STOP")) return 13;
  simulator_set_scenario("none"); safety_reset_estop(); sim_pump(80);
  std::puts("SIM SELFTEST: stale control and independent STOP deadline ok");
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

static int run_commissioning_test(const char* directory) {
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
  if (!sim_expect(next && lv_obj_has_state(next, LV_STATE_DISABLED), "setup skipped unsaved motor config")) return 3;
  if (!sim_click_label("OPEN MOTOR CONFIG") || !sim_click_label("SAVE & APPLY")) return 4;
  sim_pump(600);
  if (!sim_click_back_to(SCREEN_SETUP) || !sim_click_label("NEXT")) return 5;
  capture("02_direction.bmp");
  for (const char* label : {"HOLD CW", "HOLD CCW"}) {
    if (!sim_send_label_event(label, LV_EVENT_PRESSED) || !sim_send_label_event(label, LV_EVENT_PRESSING) ||
        !sim_send_label_event(label, LV_EVENT_RELEASED)) return 6;
  }
  if (!sim_click_label("DIRECTION CORRECT") || !sim_click_label("NEXT")) return 7;
  capture("03_calibration.bmp");
  if (!sim_click_label("OPEN CALIBRATION") || !sim_click_label("MOVE 360")) return 8;
  sim_pump(900);
  if (!sim_enter_calibration_measurement() || !sim_click_label("APPLY MEASUREMENT") || !sim_click_label("MOVE 360")) return 9;
  sim_pump(900);
  if (!sim_enter_calibration_measurement() || !sim_click_label("SAVE CALIBRATION")) return 10;
  sim_pump(600);
  if (!sim_click_back_to(SCREEN_SETUP) || !sim_click_label("NEXT")) return 11;
  capture("04_function_check.bmp");
  next = sim_find_active_label_target("NEXT");
  if (!sim_expect(next && lv_obj_has_state(next, LV_STATE_DISABLED), "setup skipped physical function check")) return 12;
  simulator_set_estop_input(true); estop_overlay_show(); sim_pump(100);
  simulator_set_estop_input(false); sim_pump(300);
  auto reset = sim_find_label_target(lv_layer_top(), "RESET TO IDLE", false, true);
  if (!sim_expect(reset && !lv_obj_has_state(reset, LV_STATE_DISABLED), "wizard reset unavailable")) return 13;
  lv_obj_send_event(reset, LV_EVENT_CLICKED, nullptr); sim_pump(80); estop_overlay_hide();
  if (!sim_expect(control_get_state() == STATE_IDLE, "reset restarted motion")) return 14;
  if (!sim_click_label("TEST START") || !sim_click_label("STOP ROTATION")) return 15;
  simulator_set_scenario("nvs-failure");
  if (!sim_click_label("NEXT")) return 16;
  sim_pump(600);
  auto finish = sim_find_active_label_target("FINISH");
  if (!sim_expect(finish && lv_obj_has_state(finish, LV_STATE_DISABLED), "setup finished before durable save")) return 17;
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

static int run_screenshot_dump(const char* dir) {
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

static unsigned audit_labels(lv_obj_t* obj, const char* screen) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return 0;
  unsigned failures = 0;
  if (lv_obj_check_type(obj, &lv_label_class)) {
    const char* text = lv_label_get_text(obj);
    const auto mode = lv_label_get_long_mode(obj);
    lv_point_t measured{};
    const int32_t width = lv_obj_get_content_width(obj);
    lv_text_get_size(&measured, text, lv_obj_get_style_text_font(obj, LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN), lv_obj_get_style_text_line_space(obj, LV_PART_MAIN),
                    mode == LV_LABEL_LONG_MODE_WRAP ? width : LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    // Explicit ellipsis/scrolling are intentional for operator-supplied text.
    if (mode == LV_LABEL_LONG_MODE_CLIP && measured.x > width &&
        lv_obj_get_style_transform_scale_x(obj, LV_PART_MAIN) == 256 &&
        std::strstr(text, " / ") == nullptr && std::strstr(text, "Dropped ") != text) {
      std::printf("LAYOUT CLIP %s: width=%ld text=%ld '%s'\n", screen,
                  static_cast<long>(width), static_cast<long>(measured.x), text);
      ++failures;
    }
    if (measured.y > lv_obj_get_content_height(obj)) {
      std::printf("LAYOUT HEIGHT %s: '%s'\n", screen, text);
      ++failures;
    }
    lv_obj_t* parent = lv_obj_get_parent(obj);
    if (parent && !lv_obj_has_flag(parent, LV_OBJ_FLAG_SCROLLABLE)) {
      lv_area_t bounds{}, parentBounds{};
      lv_obj_get_coords(obj, &bounds); lv_obj_get_coords(parent, &parentBounds);
      if (bounds.x1 < parentBounds.x1 || bounds.x2 > parentBounds.x2 ||
          bounds.y1 < parentBounds.y1 || bounds.y2 > parentBounds.y2) {
        std::printf("LAYOUT BOUNDS %s: '%s'\n", screen, text);
        ++failures;
      }
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
    failures += audit_labels(lv_obj_get_child(obj, i), screen);
  return failures;
}
static int run_layout_audit() {
  unsigned failures = 0;
  speed_slider_set(MIN_RPM);
  for (int id = SCREEN_MAIN; id < SCREEN_COUNT; ++id) {
    screens_show(static_cast<ScreenId>(id)); sim_pump(250);
    lv_obj_update_layout(screenRoots[id]);
    failures += audit_labels(screenRoots[id], sim_screen_name(static_cast<ScreenId>(id)));
  }
  for (const char* scenario : {"estop", "driver-alarm", "stale-adc", "i2c-failure"}) {
    simulator_set_scenario(scenario); estop_overlay_show(); sim_pump(100);
    failures += audit_labels(lv_layer_top(), scenario);
    estop_overlay_hide(); simulator_set_scenario("none"); control_transition_to(STATE_IDLE);
  }
  std::printf("LAYOUT AUDIT: %u failures\n", failures);
  return failures ? 1 : 0;
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
  lv_sdl_mouse_create();
  lv_sdl_mousewheel_create();
  lv_sdl_keyboard_create();

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

    uint32_t waitMs = lv_timer_handler();
    if (waitMs == LV_NO_TIMER_READY || waitMs > 5) {
      waitMs = 5;
    }
    lv_delay_ms(waitMs);
  }

  return 0;
}
