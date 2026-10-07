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
#include "../src/motor/motor.h"
#include "../src/motor/calibration.h"
#include "../src/ui/screens.h"
#include "../src/ui/theme.h"
#include "../src/ui/value_format.h"
#include "../src/ui/value_binding.h"
#include "../src/ui/text_metrics.h"
#include "../src/ui/input_panel.h"
#include "../src/ui/screen_saver.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

LV_FONT_DECLARE(rotator_digits_104);

void simulator_init_state();
void simulator_tick();
void simulator_set_estop_input(bool active);
void simulator_fast_motion(bool fast);
bool simulator_set_scenario(const char* scenario);

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

static void sim_pump(uint32_t ms) {
  uint32_t start = lv_tick_get();
  do {
    screens_process_pending();
    simulator_tick();
    screens_update_current();
    estop_overlay_update();
    dim_update();
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
    if (lv_obj_is_clickable(obj)) {
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

static bool sim_hold_label(const char* text, uint32_t ms) {
  if (!sim_send_label_event(text, LV_EVENT_PRESSED)) return false;
  const uint32_t started = lv_tick_get();
  do {
    auto target = sim_find_active_label_target(text);
    if (!sim_expect(target && !lv_obj_is_disabled(target), "held jog button disabled during enable settle")) return false;
    if (!sim_send_label_event(text, LV_EVENT_PRESSING)) return false;
  } while (lv_tick_get() - started < ms);
  return true;
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
  if (!target || lv_obj_is_disabled(target) || lv_obj_is_hidden(target))
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
  if (!sim_click_label("MENU")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_MENU, "MENU button did not open menu")) return false;

  if (!sim_click_label("RUN MODES")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_RUN_MODES, "RUN MODES did not open run mode picker"))
    return false;

  if (!sim_click_label("PULSE")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_PULSE, "run mode PULSE did not open pulse screen"))
    return false;
  if (!sim_click_label("> START")) return false;
  sim_pump(260);  // ENA settle window
  if (!sim_expect(control_get_state() == STATE_PULSE, "pulse START did not enter PULSE")) return false;
  if (!sim_click_label("[] STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "pulse STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("STEP")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_STEP, "run mode STEP did not open step screen"))
    return false;
  if (!sim_click_label("> STEP")) return false;
  sim_pump(260);  // ENA settle window
  if (!sim_expect(control_get_state() == STATE_STEP, "step button did not enter STEP")) return false;
  if (!sim_click_label("X STOP")) return false;
  if (!sim_expect(control_get_state() == STATE_IDLE, "step STOP did not return IDLE")) return false;
  if (!sim_click_back_to(SCREEN_MAIN)) return false;

  if (!sim_show_and_check(SCREEN_RUN_MODES)) return false;
  if (!sim_click_label("JOG")) return false;
  if (!sim_expect(screens_get_current() == SCREEN_JOG, "run mode JOG did not open jog screen")) return false;
  if (!sim_hold_label("HOLD CW", 260)) return false;
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
  if (!sim_click_label("CONTINUOUS") || !sim_click_label("MODE SETTINGS >")) return false;
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

static lv_obj_t* sim_find_type(lv_obj_t* obj, const lv_obj_class_t* type) {
  if (lv_obj_check_type(obj, type)) return obj;
  for (uint32_t i=0; i<lv_obj_get_child_count(obj); ++i)
    if (auto found = sim_find_type(lv_obj_get_child(obj, i), type)) return found;
  return nullptr;
}
static bool sim_enter_calibration_measurement(const char* value = "360") {
  if (!sim_click_label("---")) return false;
  auto root = lv_layer_top();
  auto field = sim_find_type(root, &lv_textarea_class);
  auto keyboard = sim_find_type(root, &lv_keyboard_class);
  if (!sim_expect(field && keyboard, "calibration measurement editor missing")) return false;
  lv_textarea_set_text(field, value); lv_obj_send_event(keyboard, LV_EVENT_READY, nullptr);
  sim_pump(80); return true;
}
static unsigned audit_labels(lv_obj_t* obj, const char* screen);
static int run_commissioning_test(const char* directory = nullptr);
static int run_calibration_test(const char* directory = nullptr);
static int run_program_edit_test(const char* directory = nullptr);
static int run_motor_config_test(const char* directory = nullptr);
static int run_screen_saver_test(const char* directory = nullptr);
static lv_obj_t* sim_label_object(lv_obj_t* root, const char* text) {
  if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    if (auto* found = sim_label_object(lv_obj_get_child(root, i), text)) return found;
  return nullptr;
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
  if (!sim_expect(sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "STATUS STALE") &&
                  sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "---"), "diagnostics presented stale motion values as live")) return false;
  simulator_set_scenario("none"); sim_pump(80);
  if (!sim_expect(!sim_label_object(screenRoots[SCREEN_DIAGNOSTICS], "STATUS STALE"), "diagnostics failed to recover fresh status")) return false;
  speed_slider_set(previousRpm); screens_show(SCREEN_MAIN); sim_pump(40);
  return true;
}
static bool sim_test_countdown_cancellation_and_direction() {
  simulator_set_scenario("none"); safety_reset_estop(); control_stop(); sim_pump(100);
  g_settings.countdown_seconds=1;
  for (unsigned elapsed : {50u,900u}) {
    screens_show(SCREEN_TIMER); sim_pump(40);
    if (!sim_click_label("> START")) return false;
    sim_pump(elapsed);
    // Fault/reset happen without a UI tick: current safety state alone cannot detect this history.
    simulator_set_estop_input(true); control_run_cycle();
    simulator_set_estop_input(false); safety_reset_estop(); control_run_cycle();
    sim_pump(1200);
    if (!sim_expect(control_get_state()==STATE_IDLE && !motor_is_running(),"old countdown restarted after fault/reset")) return false;
  }
  screens_show(SCREEN_TIMER); sim_pump(40);
  if (!sim_click_label("> START")) return false;
  control_stop(); sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_IDLE,"idle STOP did not cancel countdown")) return false;
  if (!sim_click_label("> START")) return false;
  screens_show(SCREEN_MAIN); sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_IDLE,"navigation did not cancel countdown")) return false;
  screens_show(SCREEN_TIMER); sim_pump(40);
  if (!sim_click_label("> START")) return false;
  sim_pump(1200);
  if (!sim_expect(control_get_state()==STATE_RUNNING,"fresh countdown START failed")) return false;
  control_stop(); sim_pump(300);
  g_settings.invert_direction=true; speed_set_direction(DIR_CW);
  screens_show(SCREEN_PROGRAMS); sim_pump(40);
  if (!sim_click_label("+ NEW")) return false;
  const auto draft=screen_program_edit_get_preset();
  if (!sim_expect(draft->direction==DIR_CW && speed_resolve_direction((Direction)draft->direction)==DIR_CCW,
                  "new program inherited an already inverted direction")) return false;
  if (!sim_click_label("CANCEL")) return false;
  g_settings.invert_direction=false;
  speed_set_workpiece_diameter_mm(300); speed_slider_set(0.02f);
  screens_show(SCREEN_STEP); sim_pump(40);
  if (!sim_send_label_event("-", LV_EVENT_SHORT_CLICKED)) return false;
  if (!sim_expect(fabs(speed_get_target_rpm()-0.019f)<0.00001f,"Step fine adjustment failed")) return false;
  if (!sim_expect(audit_labels(screenRoots[SCREEN_STEP],"Step fine adjustment")==0,"Step labels overflowed")) return false;
  // Boundary geometry must produce an explanation, never a narrowed/reversed move.
  g_settings.microstep=32; g_settings.calibration_factor=1.5f;
  speed_set_workpiece_diameter_mm(20000);
  if (!sim_click_label("TARGET")) return false;
  auto field=sim_find_type(screenRoots[SCREEN_STEP],&lv_textarea_class);
  auto keyboard=sim_find_type(screenRoots[SCREEN_STEP],&lv_keyboard_class);
  if (!sim_expect(field && keyboard,"custom Step input missing")) return false;
  lv_textarea_set_text(field,"3600"); lv_obj_send_event(keyboard,LV_EVENT_READY,nullptr); sim_pump(80);
  auto blockedMove=sim_find_active_label_target("OUT OF RANGE");
  if (!sim_expect(blockedMove && lv_obj_is_disabled(blockedMove),"overflowing move was not visibly blocked")) return false;
  if (!sim_expect(audit_labels(screenRoots[SCREEN_STEP],"Step boundary values")==0,"boundary Step labels overflowed")) return false;
  g_settings.microstep=16; g_settings.calibration_factor=1; speed_set_workpiece_diameter_mm(300);
  if (!sim_click_label("90")) return false;
  screens_show(SCREEN_DISPLAY); sim_pump(40);
  lv_obj_t* dim=nullptr;
  for (const char* value : {"OFF","30s","1m","2m","5m"})
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
static int run_self_test() {
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
  std::puts("SIM SELFTEST: settings/programs ok");
  if (run_program_edit_test() != 0) return 15;
  std::puts("SIM SELFTEST: new program modes/precision/input/save/cancel ok");
  if (run_motor_config_test() != 0) return 18;
  std::puts("SIM SELFTEST: motor exact input/cancel/save lock/lifecycle ok");
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
  auto start = sim_find_label_target(screenRoots[SCREEN_MAIN], "START BLOCKED", false, true);
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



uint8_t simulator_display_brightness();
static int run_screen_saver_test(const char* directory) {
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

static int run_motor_config_test(const char* directory) {
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
  if (!openValue("ACCELERATION / STEPS/S2") || !enter("15000.5")) return 7;
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

static int run_program_edit_test(const char* directory) {
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

static int run_calibration_test(const char* directory) {
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
  if (!obj || lv_obj_is_hidden(obj)) return 0;
  unsigned failures = 0;
  if (lv_obj_check_type(obj, &lv_label_class)) {
    const char* text = lv_label_get_text(obj);
    const auto mode = lv_label_get_long_mode(obj);
    lv_point_t measured{};
    const int32_t width = lv_obj_get_content_width(obj);
    lv_text_get_size(&measured, text, lv_obj_get_style_text_font(obj, LV_PART_MAIN),
                    lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN), lv_obj_get_style_text_line_space(obj, LV_PART_MAIN),
                    mode == LV_LABEL_LONG_MODE_WRAP ? width : LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    // LVGL 9.6 sizes labels after trimming their first/last line leading.
    // Measure the same box; retain the height check for genuinely clipped text.
    const auto* font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
    const auto trim = lv_obj_get_style_text_leading_trim(obj, LV_PART_MAIN);
    if ((trim == LV_TEXT_LEADING_TRIM_CAPITAL && font->cap_height <= 0) ||
        (trim == LV_TEXT_LEADING_TRIM_LOWER && font->x_height <= 0)) {
      std::printf("LAYOUT FONT METRICS %s: '%s'\n", screen, text); ++failures;
    }
    measured.y -= lv_font_get_top_trim(font, trim) + lv_font_get_bottom_trim(font, trim);
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
    if (parent && !lv_obj_is_scrollable(parent)) {
      lv_area_t bounds{}, parentBounds{};
      lv_obj_get_coords(obj, &bounds); lv_obj_get_coords(parent, &parentBounds);
      if (bounds.x1 < parentBounds.x1 || bounds.x2 > parentBounds.x2 ||
          bounds.y1 < parentBounds.y1 || bounds.y2 > parentBounds.y2) {
        std::printf("LAYOUT BOUNDS %s: '%s'\n", screen, text);
        ++failures;
      }
    }
  }
  if (lv_obj_get_style_shadow_width(obj, LV_PART_MAIN) > 0 ||
      lv_obj_get_style_drop_shadow_radius(obj, LV_PART_MAIN) > 0) {
    std::printf("LAYOUT SHADOW %s: flat UI required\n", screen); ++failures;
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
