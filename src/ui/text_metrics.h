#pragma once
#include <lvgl.h>

// Trim the unused top leading, retaining the baseline/descender area for names,
// mixed-case instructions and symbols. Button alignment remains unchanged.
inline void ui_trim_text_leading(lv_obj_t* root) {
  if (!root) return;
  // Compound widgets own their internal text metrics and cursor placement.
  if (lv_obj_check_type(root, &lv_textarea_class) ||
      lv_obj_check_type(root, &lv_keyboard_class) ||
      lv_obj_check_type(root, &lv_dropdown_class) ||
      lv_obj_check_type(root, &lv_spinbox_class)) return;
  if (lv_obj_check_type(root, &lv_label_class)) {
    // Older generated fonts (including the 104 px RPM digits) have no cap-height
    // metadata. Trimming them would remove glyph pixels rather than leading.
    const auto* font = lv_obj_get_style_text_font(root, LV_PART_MAIN);
    lv_obj_set_style_text_leading_trim(root,
        font && font->cap_height > 0 ? LV_TEXT_LEADING_TRIM_CAPITAL : LV_TEXT_LEADING_TRIM_NONE, 0);
  }
  for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
    ui_trim_text_leading(lv_obj_get_child(root, i));
}
