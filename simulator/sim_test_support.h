// Shared declarations for the simulator test code.
#pragma once

#include "lvgl.h"
#include "include/lvgl/draw/lv_snapshot.h"
#include "include/lvgl/drivers/sdl/lv_sdl_window.h"
#include "include/lvgl/drivers/sdl/lv_sdl_mouse.h"
#include "include/lvgl/drivers/sdl/lv_sdl_keyboard.h"
#include "include/lvgl/drivers/sdl/lv_sdl_mousewheel.h"

#include "../src/control/control.h"
#include "../src/safety/safety.h"
void safety_reset_estop();  // Simulator-only helper (sim_stubs.cpp): clears the simulated fault.
#include "../src/motor/speed.h"
#include "../src/motor/motor.h"
#include "../src/motor/calibration.h"
#include "../src/ui/screens.h"
#include "../src/ui/theme.h"
#include "../src/ui/value_format.h"
#include "../src/ui/value_binding.h"
#include "../src/ui/tabular_font.h"
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


// sim_test_support.cpp
void sim_pump(uint32_t ms);
const char* sim_screen_name(ScreenId id);
bool sim_fail(const char* msg);
bool sim_expect(bool condition, const char* msg);
lv_obj_t* sim_clickable_ancestor(lv_obj_t* obj);
lv_obj_t* sim_button_ancestor(lv_obj_t* obj);
lv_obj_t* sim_find_label_target(lv_obj_t* obj, const char* text, bool contains, bool buttonOnly);
lv_obj_t* sim_find_active_label_target(const char* text, bool contains = false);
bool sim_send_label_event(const char* text, lv_event_code_t event, bool contains = false);
bool sim_click_label(const char* text, bool contains = false);
bool sim_hold_label(const char* text, uint32_t ms);
bool sim_show_and_check(ScreenId id);
bool sim_click_back_to(ScreenId expected);
lv_obj_t* sim_find_action(lv_obj_t* root, UiActionId id);
bool sim_click_action(UiActionId id);
lv_obj_t* sim_find_type(lv_obj_t* obj, const lv_obj_class_t* type);
lv_obj_t* sim_label_object(lv_obj_t* root, const char* text);
bool sim_write_bmp_argb8888(const char* path, const lv_draw_buf_t* buf);
unsigned audit_labels(lv_obj_t* obj, const char* screen);

// sim_previews.cpp
bool sim_enter_calibration_measurement(const char* value = "360");
int run_screen_saver_test(const char* directory = nullptr);
int run_motor_config_test(const char* directory = nullptr);
int run_program_edit_test(const char* directory = nullptr);
int run_calibration_test(const char* directory = nullptr);
int run_commissioning_test(const char* directory = nullptr);

// sim_audit.cpp
int run_screenshot_dump(const char* dir);
int run_layout_audit();

// sim_self_test.cpp
int run_self_test();
