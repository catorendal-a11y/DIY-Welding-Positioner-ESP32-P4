// TIG Rotator Controller - Settings Screen
// Brutalist v2.0 design - only functional settings
// Rows: Max RPM (display), Acceleration, Microstepping, Calibration

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../display.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../motor/motor.h"
#include <cstdio>

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* accelLabel = nullptr;
static lv_obj_t* microstepLabel = nullptr;
static lv_obj_t* rpmBtnToggle = nullptr;
static lv_obj_t* rpmBtnLabel = nullptr;
static lv_obj_t* brightnessLabel = nullptr;
static lv_obj_t* storageLabel = nullptr;
static lv_obj_t* dimLabel = nullptr;
static lv_obj_t* dirSwToggle = nullptr;
static lv_obj_t* dirSwLabel = nullptr;
static lv_obj_t* pinTa = nullptr;
static lv_obj_t* pinKb = nullptr;
static const char* PIN_CODE = "1234";

static const int accel_values[] = {1000, 2500, 5000, 10000};
static const char* accel_strings[] = {"LOW", "MED", "HIGH", "MAX"};
static int accel_idx = 2;

static const int microstep_values[] = {2, 4, 8, 16, 32};
static const char* microstep_strings[] = {"1/2", "1/4", "1/8", "1/16", "1/32"};
static int microstep_idx = 2;

static const int dim_values[] = {0, 30, 60, 120, 300};
static const char* dim_strings[] = {"OFF", "30s", "1m", "2m", "5m"};
static int dim_idx = 2;

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MENU); }

static void accel_click_cb(lv_event_t* e) {
  accel_idx = (accel_idx + 1) % 4;
  g_settings.acceleration = accel_values[accel_idx];
  motor_apply_settings();
  if (accelLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%s  >", accel_strings[accel_idx]);
    lv_label_set_text(accelLabel, buf);
  }
}

static void microstep_click_cb(lv_event_t* e) {
  microstep_idx = (microstep_idx + 1) % 5;
  g_settings.microstep = microstep_values[microstep_idx];
  if (microstepLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%s  >", microstep_strings[microstep_idx]);
    lv_label_set_text(microstepLabel, buf);
  }
}

static void dim_click_cb(lv_event_t* e) {
  dim_idx = (dim_idx + 1) % 5;
  g_settings.dim_timeout = dim_values[dim_idx];
  if (dimLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%s  >", dim_strings[dim_idx]);
    lv_label_set_text(dimLabel, buf);
  }
}

static void save_calib_cb(lv_event_t* e) {
  storage_save_settings();
  LOG_I("System settings saved successfully.");
}

static void brightness_adj_cb(lv_event_t* e) {
  int delta = (intptr_t)(lv_obj_t*)lv_event_get_user_data(e);
  int val = (int)g_settings.brightness + delta * 10;
  if (val < 10) val = 10;
  if (val > 255) val = 255;
  g_settings.brightness = (uint8_t)val;
  display_set_brightness(g_settings.brightness);
  if (brightnessLabel) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%  >", (int)(g_settings.brightness * 100 / 255));
    lv_label_set_text(brightnessLabel, buf);
  }
}

static void rpm_btn_toggle_cb(lv_event_t* e) {
  g_settings.rpm_buttons_enabled = !g_settings.rpm_buttons_enabled;
  if (g_settings.rpm_buttons_enabled) {
    lv_obj_set_style_bg_color(rpmBtnToggle, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(rpmBtnToggle, COL_ACCENT, 0);
    lv_label_set_text(rpmBtnLabel, "ON  >");
  } else {
    lv_obj_set_style_bg_color(rpmBtnToggle, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(rpmBtnToggle, COL_BORDER_SM, 0);
    lv_label_set_text(rpmBtnLabel, "OFF  >");
  }
}

static void dir_sw_toggle_cb(lv_event_t* e) {
  g_settings.dir_switch_enabled = !g_settings.dir_switch_enabled;
  if (g_settings.dir_switch_enabled) {
    lv_obj_set_style_bg_color(dirSwToggle, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(dirSwToggle, COL_ACCENT, 0);
    lv_label_set_text(dirSwLabel, "ON  >");
  } else {
    lv_obj_set_style_bg_color(dirSwToggle, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(dirSwToggle, COL_BORDER_SM, 0);
    lv_label_set_text(dirSwLabel, "OFF  >");
  }
}

static void pin_kb_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    if (pinTa && strcmp(lv_textarea_get_text(pinTa), PIN_CODE) == 0) {
      storage_format();
      if (storageLabel) {
        size_t used, total;
        storage_get_usage(&used, &total);
        char buf[32];
        snprintf(buf, sizeof(buf), "%u / %u KB", (unsigned)(used / 1024), (unsigned)(total / 1024));
        lv_label_set_text(storageLabel, buf);
      }
    }
  }
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (pinKb) { lv_obj_delete(pinKb); pinKb = nullptr; }
    if (pinTa) { lv_obj_delete(pinTa); pinTa = nullptr; }
  }
}

static void delete_storage_cb(lv_event_t* e) {
  if (pinKb) return;
  pinTa = lv_textarea_create(lv_screen_active());
  lv_obj_set_size(pinTa, 200, 50);
  lv_obj_align(pinTa, LV_ALIGN_TOP_MID, 0, 180);
  lv_textarea_set_one_line(pinTa, true);
  lv_textarea_set_password_mode(pinTa, true);
  lv_textarea_set_accepted_chars(pinTa, "0123456789");
  lv_obj_set_style_bg_color(pinTa, COL_BG, 0);
  lv_obj_set_style_border_color(pinTa, COL_ACCENT, 0);
  lv_obj_set_style_border_width(pinTa, 2, 0);
  lv_obj_set_style_text_color(pinTa, COL_TEXT, 0);
  lv_textarea_set_placeholder_text(pinTa, "PIN");

  pinKb = lv_keyboard_create(lv_screen_active());
  lv_keyboard_set_mode(pinKb, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(pinKb, pinTa);
  lv_obj_set_style_bg_color(pinKb, COL_BG, 0);
  lv_obj_set_style_border_color(pinKb, COL_ACCENT, 0);
  lv_obj_set_style_border_width(pinKb, 2, 0);
  lv_obj_add_event_cb(pinKb, pin_kb_cb, LV_EVENT_ALL, nullptr);
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create a setting row (full-width, tall for gloves)
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_setting_row(lv_obj_t* parent, int y,
                                     const char* label_text,
                                     const char* value_text,
                                     lv_obj_t** value_out) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 776, 44);
  lv_obj_set_pos(row, 12, y);
  lv_obj_set_style_bg_color(row, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(row, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, COL_TEXT, 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

  lv_obj_t* value = lv_label_create(row);
  lv_label_set_text(value, value_text);
  lv_obj_set_style_text_font(value, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(value, COL_ACCENT, 0);
  lv_obj_align(value, LV_ALIGN_RIGHT_MID, -16, 0);

  if (value_out) *value_out = value;
  return row;
}

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create section label
// ───────────────────────────────────────────────────────────────────────────────
static void create_section_label(lv_obj_t* parent, int y, const char* section_name) {
  lv_obj_t* lbl = lv_label_create(parent);
  char buf[80];
  snprintf(buf, sizeof(buf), "-- %s ", section_name);
  size_t len = strlen(buf);
  while (len < 65) { buf[len++] = '-'; }
  buf[len] = '\0';
  lv_label_set_text(lbl, buf);
  lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(lbl, 12, y);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE - 4 functional rows, bigger spacing
// Layout: Header(30) + Title(30) + 4 rows(56+72gap=128) + SAVE + BACK
// Total: 30 + 50 + 4*68 + 70 + 60 = 472 (fits in 480)
// ───────────────────────────────────────────────────────────────────────────────
void screen_settings_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SETTINGS];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  // Synchronize state from g_settings
  accel_idx = 2;
  for (int i = 0; i < 4; i++) { if (accel_values[i] == g_settings.acceleration) accel_idx = i; }
  microstep_idx = 2;
  for (int i = 0; i < 5; i++) { if (microstep_values[i] == g_settings.microstep) microstep_idx = i; }
  dim_idx = 2;
  for (int i = 0; i < 5; i++) { if (dim_values[i] == g_settings.dim_timeout) dim_idx = i; }

  // ── Header bar ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "SETTINGS");
  lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 8);

  // ── Title underline ──
  lv_obj_t* underline = lv_obj_create(screen);
  lv_obj_set_size(underline, 148, 1);
  lv_obj_set_pos(underline, PAD_X, HEADER_H);
  lv_obj_set_style_bg_color(underline, COL_ACCENT, 0);
  lv_obj_set_style_bg_opa(underline, LV_OPA_30, 0);
  lv_obj_set_style_border_width(underline, 0, 0);
  lv_obj_set_style_pad_all(underline, 0, 0);
  lv_obj_set_style_radius(underline, 0, 0);
  lv_obj_remove_flag(underline, LV_OBJ_FLAG_SCROLLABLE);

  // ── MOTOR section label ──
  create_section_label(screen, 38, "MOTOR");

  // ── 4 Setting rows, 68px spacing, 56px tall (glove-friendly) ──
  const int rowStartY = 50;
  const int rowGap = 48;

  // Row 1: Max RPM (display only, fixed at 3.0)
  {
    char rpm_buf[16];
    snprintf(rpm_buf, sizeof(rpm_buf), "%.1f  >", MAX_RPM);
    create_setting_row(screen, rowStartY, "Max RPM", rpm_buf, nullptr);
  }

  // Row 2: Acceleration (clickable cycle)
  char accel_buf[16];
  snprintf(accel_buf, sizeof(accel_buf), "%s  >", accel_strings[accel_idx]);
  lv_obj_t* accelRow = create_setting_row(screen, rowStartY + rowGap,
                                           "Acceleration", accel_buf, &accelLabel);
  lv_obj_add_flag(accelRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(accelRow, accel_click_cb, LV_EVENT_CLICKED, nullptr);

  // Row 3: Microstepping (clickable cycle)
  char micro_buf[16];
  snprintf(micro_buf, sizeof(micro_buf), "%s  >", microstep_strings[microstep_idx]);
  lv_obj_t* microRow = create_setting_row(screen, rowStartY + rowGap * 2,
                                           "Microstepping", micro_buf, &microstepLabel);
  lv_obj_add_flag(microRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(microRow, microstep_click_cb, LV_EVENT_CLICKED, nullptr);

  // Row 4: RPM Buttons (toggle)
  char rpm_btn_buf[16];
  snprintf(rpm_btn_buf, sizeof(rpm_btn_buf), "%s  >", g_settings.rpm_buttons_enabled ? "ON" : "OFF");
  lv_obj_t* rpmBtnRow = create_setting_row(screen, rowStartY + rowGap * 3,
                                            "RPM Buttons", rpm_btn_buf, &rpmBtnLabel);
  rpmBtnToggle = rpmBtnRow;
  lv_obj_add_flag(rpmBtnRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(rpmBtnRow, rpm_btn_toggle_cb, LV_EVENT_CLICKED, nullptr);

  // Row 5: Brightness (via +/- buttons)
  char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d%%  >", (int)(g_settings.brightness * 100 / 255));
  create_setting_row(screen, rowStartY + rowGap * 4,
                     "Brightness", bright_buf, &brightnessLabel);

  // Brightness -/+ buttons
  lv_obj_t* brightMinusBtn = lv_button_create(screen);
  lv_obj_set_size(brightMinusBtn, 80, 48);
  lv_obj_set_pos(brightMinusBtn, 594, rowStartY + rowGap * 4 + 2);
  lv_obj_set_style_bg_color(brightMinusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(brightMinusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(brightMinusBtn, 1, 0);
  lv_obj_set_style_border_color(brightMinusBtn, COL_BORDER_SM, 0);
  lv_obj_set_style_shadow_width(brightMinusBtn, 0, 0);
  lv_obj_set_style_pad_all(brightMinusBtn, 0, 0);
  lv_obj_add_event_cb(brightMinusBtn, brightness_adj_cb, LV_EVENT_CLICKED, (void*)-1);
  lv_obj_t* brightMinusLbl = lv_label_create(brightMinusBtn);
  lv_label_set_text(brightMinusLbl, "-10%");
  lv_obj_set_style_text_font(brightMinusLbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(brightMinusLbl, COL_TEXT, 0);
  lv_obj_center(brightMinusLbl);

  lv_obj_t* brightPlusBtn = lv_button_create(screen);
  lv_obj_set_size(brightPlusBtn, 80, 48);
  lv_obj_set_pos(brightPlusBtn, 682, rowStartY + rowGap * 4 + 2);
  lv_obj_set_style_bg_color(brightPlusBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(brightPlusBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(brightPlusBtn, 1, 0);
  lv_obj_set_style_border_color(brightPlusBtn, COL_BORDER_SM, 0);
  lv_obj_set_style_shadow_width(brightPlusBtn, 0, 0);
  lv_obj_set_style_pad_all(brightPlusBtn, 0, 0);
  lv_obj_add_event_cb(brightPlusBtn, brightness_adj_cb, LV_EVENT_CLICKED, (void*)1);
  lv_obj_t* brightPlusLbl = lv_label_create(brightPlusBtn);
  lv_label_set_text(brightPlusLbl, "+10%");
  lv_obj_set_style_text_font(brightPlusLbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(brightPlusLbl, COL_TEXT, 0);
  lv_obj_center(brightPlusLbl);

  // Row 6: Auto-dim timeout (clickable cycle)
  char dim_buf[16];
  snprintf(dim_buf, sizeof(dim_buf), "%s  >", dim_strings[dim_idx]);
  lv_obj_t* dimRow = create_setting_row(screen, rowStartY + rowGap * 5,
                                        "Auto-dim", dim_buf, &dimLabel);
  lv_obj_add_flag(dimRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(dimRow, dim_click_cb, LV_EVENT_CLICKED, nullptr);

  // Row 7: Dir Switch (toggle)
  char dir_sw_buf[16];
  snprintf(dir_sw_buf, sizeof(dir_sw_buf), "%s  >", g_settings.dir_switch_enabled ? "ON" : "OFF");
  lv_obj_t* dirSwRow = create_setting_row(screen, rowStartY + rowGap * 6,
                                          "Dir Switch", dir_sw_buf, &dirSwLabel);
  dirSwToggle = dirSwRow;
  lv_obj_add_flag(dirSwRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(dirSwRow, dir_sw_toggle_cb, LV_EVENT_CLICKED, nullptr);

  // Storage usage info (compact, no section label)
  size_t used, total;
  storage_get_usage(&used, &total);
  char storage_buf[32];
  snprintf(storage_buf, sizeof(storage_buf), "%u / %u KB", (unsigned)(used / 1024), (unsigned)(total / 1024));
  create_setting_row(screen, rowStartY + rowGap * 7,
                     "Storage", storage_buf, &storageLabel);

  // ── Bottom: BACK + SAVE + DELETE (y=432) ──
  lv_obj_t* saveBtn = lv_button_create(screen);
  lv_obj_set_size(saveBtn, 160, 48);
  lv_obj_set_pos(saveBtn, 628, 432);
  lv_obj_set_style_bg_color(saveBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(saveBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(saveBtn, 2, 0);
  lv_obj_set_style_border_color(saveBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(saveBtn, 0, 0);
  lv_obj_set_style_pad_all(saveBtn, 0, 0);
  lv_obj_add_event_cb(saveBtn, save_calib_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* saveLabel = lv_label_create(saveBtn);
  lv_label_set_text(saveLabel, "SAVE");
  lv_obj_set_style_text_font(saveLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(saveLabel, COL_ACCENT, 0);
  lv_obj_center(saveLabel);

  // ── BACK button (bottom-left, 160x48) ──
  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, 160, 48);
  lv_obj_set_pos(backBtn, 12, 432);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_set_style_pad_all(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  // ── DELETE button (center, red) ──
  lv_obj_t* deleteBtn = lv_button_create(screen);
  lv_obj_set_size(deleteBtn, 200, 48);
  lv_obj_set_pos(deleteBtn, 290, 432);
  lv_obj_set_style_bg_color(deleteBtn, COL_BTN_DANGER, 0);
  lv_obj_set_style_radius(deleteBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(deleteBtn, 2, 0);
  lv_obj_set_style_border_color(deleteBtn, COL_RED, 0);
  lv_obj_set_style_shadow_width(deleteBtn, 0, 0);
  lv_obj_set_style_pad_all(deleteBtn, 0, 0);
  lv_obj_add_event_cb(deleteBtn, delete_storage_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* deleteLabel = lv_label_create(deleteBtn);
  lv_label_set_text(deleteLabel, "DELETE ALL");
  lv_obj_set_style_text_font(deleteLabel, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(deleteLabel, COL_RED, 0);
  lv_obj_center(deleteLabel);

  LOG_I("Screen settings: v2.0 layout created");
}
