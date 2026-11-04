// Simulator test helpers: pumping, label lookup, clicks, layout checks and BMP output.
#include "sim_test_support.h"

void sim_pump(uint32_t ms) {
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

const char* sim_screen_name(ScreenId id) {
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

bool sim_fail(const char* msg) {
  std::fprintf(stderr, "SELFTEST FAIL: %s\n", msg);
  return false;
}

bool sim_expect(bool condition, const char* msg) { return condition ? true : sim_fail(msg); }

lv_obj_t* sim_clickable_ancestor(lv_obj_t* obj) {
  obj = lv_obj_get_parent(obj);
  while (obj) {
    if (lv_obj_is_clickable(obj)) {
      return obj;
    }
    obj = lv_obj_get_parent(obj);
  }
  return nullptr;
}

lv_obj_t* sim_button_ancestor(lv_obj_t* obj) {
  obj = lv_obj_get_parent(obj);
  while (obj) {
    if (lv_obj_check_type(obj, &lv_button_class)) {
      return obj;
    }
    obj = lv_obj_get_parent(obj);
  }
  return nullptr;
}

lv_obj_t* sim_find_label_target(lv_obj_t* obj, const char* text, bool contains, bool buttonOnly) {
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

lv_obj_t* sim_find_active_label_target(const char* text, bool contains) {
  ScreenId current = screens_get_current();
  if (current <= SCREEN_NONE || current >= SCREEN_COUNT) return nullptr;
  lv_obj_t* button = sim_find_label_target(screenRoots[current], text, contains, true);
  if (button) return button;
  return sim_find_label_target(screenRoots[current], text, contains, false);
}

bool sim_send_label_event(const char* text, lv_event_code_t event, bool contains) {
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

bool sim_click_label(const char* text, bool contains) {
  return sim_send_label_event(text, LV_EVENT_CLICKED, contains);
}

bool sim_hold_label(const char* text, uint32_t ms) {
  if (!sim_send_label_event(text, LV_EVENT_PRESSED)) return false;
  const uint32_t started = lv_tick_get();
  do {
    auto target = sim_find_active_label_target(text);
    if (!sim_expect(target && !lv_obj_is_disabled(target), "held jog button disabled during enable settle")) return false;
    if (!sim_send_label_event(text, LV_EVENT_PRESSING)) return false;
  } while (lv_tick_get() - started < ms);
  return true;
}

bool sim_show_and_check(ScreenId id) {
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

bool sim_click_back_to(ScreenId expected) {
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

lv_obj_t* sim_find_action(lv_obj_t* root, UiActionId id) {
  if (lv_obj_get_user_data(root) == (void*)(uintptr_t)id) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    if (lv_obj_t* result = sim_find_action(lv_obj_get_child(root, i), id)) return result;
  return nullptr;
}
bool sim_click_action(UiActionId id) {
  lv_obj_t* target = sim_find_action(lv_screen_active(), id);
  if (!target || lv_obj_is_disabled(target) || lv_obj_is_hidden(target))
    return sim_fail("action unavailable");
  lv_obj_send_event(target, LV_EVENT_CLICKED, nullptr);
  sim_pump(60);
  return true;
}

lv_obj_t* sim_find_type(lv_obj_t* obj, const lv_obj_class_t* type) {
  if (lv_obj_check_type(obj, type)) return obj;
  for (uint32_t i=0; i<lv_obj_get_child_count(obj); ++i)
    if (auto found = sim_find_type(lv_obj_get_child(obj, i), type)) return found;
  return nullptr;
}
lv_obj_t* sim_label_object(lv_obj_t* root, const char* text) {
  if (lv_obj_check_type(root, &lv_label_class) && strcmp(lv_label_get_text(root), text) == 0) return root;
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    if (auto* found = sim_label_object(lv_obj_get_child(root, i), text)) return found;
  return nullptr;
}
bool sim_write_bmp_argb8888(const char* path, const lv_draw_buf_t* buf) {
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



unsigned audit_labels(lv_obj_t* obj, const char* screen) {
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
