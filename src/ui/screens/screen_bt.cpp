// TIG Rotator Controller - Bluetooth Settings Screen
// BLE device name, connection status, paired devices list

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../ble/ble.h"
#include <cstdio>
#include <cstring>

static lv_obj_t* bleToggleSw = nullptr;
static lv_obj_t* bleToggleLbl = nullptr;
static lv_obj_t* bleNameRow = nullptr;
static lv_obj_t* bleNameValueLbl = nullptr;
static lv_obj_t* statusDot = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* pairedList = nullptr;
static lv_obj_t* discoveredList = nullptr;
static lv_obj_t* bleTa = nullptr;
static lv_obj_t* bleKb = nullptr;
static lv_obj_t* blePromptLabel = nullptr;
static bool bleEnabled = true;
static bool scanRunning = false;

static void ble_kb_cb(lv_event_t* e);
static volatile bool bleKbClosePending = false;

static void cleanup_kb() {
  if (bleKb) {
    lv_obj_remove_event_cb(bleKb, ble_kb_cb);
    lv_keyboard_set_textarea(bleKb, nullptr);
    lv_obj_t* old = bleKb; bleKb = nullptr;
    lv_obj_delete_async(old);
  }
  if (bleTa) { lv_obj_t* old = bleTa; bleTa = nullptr; lv_obj_delete_async(old); }
  if (blePromptLabel) { lv_obj_t* old = blePromptLabel; blePromptLabel = nullptr; lv_obj_delete_async(old); }
  bleKbClosePending = false;
}

static void forget_all_cb(lv_event_t* e);
static void add_paired_placeholder();

static void back_cb(lv_event_t* e) {
  cleanup_kb();
  screens_show(SCREEN_SETTINGS);
}

static void style_kb(lv_obj_t* keyboard) {
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_border_color(keyboard, COL_ACCENT, 0);
  lv_obj_set_style_border_width(keyboard, 2, 0);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x2A2A2A), LV_PART_ITEMS);
  lv_obj_set_style_border_color(keyboard, lv_color_hex(0x444444), LV_PART_ITEMS);
  lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
  lv_obj_set_style_text_color(keyboard, COL_TEXT_WHITE, LV_PART_ITEMS);
  lv_obj_set_style_text_font(keyboard, FONT_SUBTITLE, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_bg_color(keyboard, COL_BG_ACTIVE, LV_PART_SELECTED);
  lv_obj_set_style_text_color(keyboard, COL_ACCENT, LV_PART_SELECTED);
}

static void ble_kb_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (code == LV_EVENT_READY && bleTa) {
      const char* name = lv_textarea_get_text(bleTa);
      if (name[0]) {
        strlcpy(g_settings.ble_name, name, sizeof(g_settings.ble_name));
        storage_save_settings();
        if (bleNameValueLbl) lv_label_set_text(bleNameValueLbl, name);
      }
    }
    bleKbClosePending = true;
  }
}

static void show_ble_kb() {
  cleanup_kb();
  lv_obj_t* screen = screenRoots[SCREEN_BT];
  blePromptLabel = lv_label_create(screen);
  lv_label_set_text(blePromptLabel, "Enter device name:");
  lv_obj_set_style_text_font(blePromptLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(blePromptLabel, COL_TEXT_DIM, 0);
  lv_obj_align(blePromptLabel, LV_ALIGN_TOP_MID, 0, 80);
  bleTa = lv_textarea_create(screen);
  lv_obj_set_size(bleTa, 460, 50);
  lv_obj_align(bleTa, LV_ALIGN_TOP_MID, 0, 110);
  lv_textarea_set_one_line(bleTa, true);
  lv_textarea_set_text(bleTa, g_settings.ble_name[0] ? g_settings.ble_name : BLE_DEVICE_NAME_DEFAULT);
  lv_obj_set_style_bg_color(bleTa, COL_BG_INPUT, 0);
  lv_obj_set_style_border_color(bleTa, COL_ACCENT, 0);
  lv_obj_set_style_border_width(bleTa, 2, 0);
  lv_obj_set_style_text_color(bleTa, COL_TEXT, 0);
  lv_obj_set_style_text_font(bleTa, FONT_BTN, 0);
  bleKb = lv_keyboard_create(screen);
  lv_keyboard_set_textarea(bleKb, bleTa);
  lv_obj_set_size(bleKb, 700, 220);
  lv_obj_align(bleKb, LV_ALIGN_TOP_MID, 0, 174);
  style_kb(bleKb);
  lv_obj_add_event_cb(bleKb, ble_kb_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(bleKb, ble_kb_cb, LV_EVENT_CANCEL, nullptr);
}

static void ble_toggle_cb(lv_event_t* e) {
  bleEnabled = !bleEnabled;
  bleEnableValue = bleEnabled;
  bleEnablePending = true;
  if (bleEnabled) {
    lv_obj_set_style_bg_color(bleToggleSw, COL_GREEN, 0);
    lv_label_set_text(bleToggleLbl, "ON");
  } else {
    lv_obj_set_style_bg_color(bleToggleSw, lv_color_hex(0x333333), 0);
    lv_label_set_text(bleToggleLbl, "OFF");
  }
}

static void scan_cb(lv_event_t* e) {
  if (scanRunning) return;
  scanRunning = true;
  lv_obj_clean(discoveredList);
  bleScanDone = false;
  bleScanPending = true;
}

static void forget_all_cb(lv_event_t* e) {
  lv_obj_clean(pairedList);
  add_paired_placeholder();
  LOG_I("BT: All paired devices forgotten");
}

static lv_obj_t* make_footer_btn(lv_obj_t* parent, int x, int y, int w, int h,
                                  const char* text, lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, SET_BTN_FONT, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

static lv_obj_t* make_accent_btn(lv_obj_t* parent, int x, int y, int w, int h,
                                  const char* text, lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, 2, 0);
  lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, SET_BTN_FONT, 0);
  lv_obj_set_style_text_color(lbl, COL_ACCENT, 0);
  lv_obj_center(lbl);
  return btn;
}

static void add_paired_placeholder() {
  lv_obj_t* row = lv_obj_create(pairedList);
  lv_obj_set_size(row, 760, 44);
  lv_obj_set_style_bg_color(row, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(row, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(row, 1, 0);
  lv_obj_set_style_radius(row, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* lbl = lv_label_create(row);
  lv_label_set_text(lbl, "No paired devices");
  lv_obj_set_style_text_font(lbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT_VDIM, 0);
  lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
}

void screen_bt_invalidate_widgets() {
  bleToggleSw = nullptr;
  bleToggleLbl = nullptr;
  bleNameRow = nullptr;
  bleNameValueLbl = nullptr;
  statusDot = nullptr;
  statusLabel = nullptr;
  pairedList = nullptr;
  discoveredList = nullptr;
  bleKb = nullptr;
  bleTa = nullptr;
  blePromptLabel = nullptr;
  bleKbClosePending = false;
  scanRunning = false;
}

void screen_bt_create() {
  lv_obj_t* screen = screenRoots[SCREEN_BT];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;
  const int CW = SCREEN_W - 2 * PX;

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 28);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "BLUETOOTH");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PX, 6);

  int y = 28;

  lv_obj_t* toggleRow = lv_obj_create(screen);
  lv_obj_set_size(toggleRow, CW, SET_ROW_H);
  lv_obj_set_pos(toggleRow, PX, y);
  lv_obj_set_style_bg_color(toggleRow, COL_BG, 0);
  lv_obj_set_style_border_width(toggleRow, 0, 0);
  lv_obj_set_style_radius(toggleRow, 0, 0);
  lv_obj_set_style_pad_all(toggleRow, 0, 0);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* btLabel = lv_label_create(toggleRow);
  lv_label_set_text(btLabel, "Bluetooth");
  lv_obj_set_style_text_font(btLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(btLabel, COL_TEXT, 0);
  lv_obj_align(btLabel, LV_ALIGN_LEFT_MID, 8, 0);

  bleToggleSw = lv_button_create(toggleRow);
  lv_obj_set_size(bleToggleSw, 80, 40);
  lv_obj_align(bleToggleSw, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_radius(bleToggleSw, 12, 0);
  lv_obj_set_style_border_width(bleToggleSw, 0, 0);
  lv_obj_set_style_shadow_width(bleToggleSw, 0, 0);
  lv_obj_set_style_pad_all(bleToggleSw, 0, 0);
  lv_obj_remove_flag(bleToggleSw, LV_OBJ_FLAG_SCROLLABLE);
  bleEnabled = ble_is_enabled();
  lv_obj_set_style_bg_color(bleToggleSw, bleEnabled ? COL_GREEN : lv_color_hex(0x333333), 0);
  lv_obj_add_event_cb(bleToggleSw, ble_toggle_cb, LV_EVENT_CLICKED, nullptr);

  bleToggleLbl = lv_label_create(bleToggleSw);
  lv_label_set_text(bleToggleLbl, bleEnabled ? "ON" : "OFF");
  lv_obj_set_style_text_font(bleToggleLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(bleToggleLbl, lv_color_white(), 0);
  lv_obj_center(bleToggleLbl);

  y += SET_ROW_H;

  lv_obj_t* nameRow = lv_obj_create(screen);
  lv_obj_set_size(nameRow, CW, SET_ROW_H);
  lv_obj_set_pos(nameRow, PX, y);
  lv_obj_set_style_bg_color(nameRow, COL_BG_ROW, 0);
  lv_obj_set_style_border_color(nameRow, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(nameRow, 1, 0);
  lv_obj_set_style_radius(nameRow, RADIUS_ROW, 0);
  lv_obj_set_style_shadow_width(nameRow, 0, 0);
  lv_obj_set_style_pad_all(nameRow, 0, 0);
  lv_obj_remove_flag(nameRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(nameRow, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(nameRow, [](lv_event_t* e) { show_ble_kb(); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* nameKeyLbl = lv_label_create(nameRow);
  lv_label_set_text(nameKeyLbl, "DEVICE NAME");
  lv_obj_set_style_text_font(nameKeyLbl, FONT_BODY, 0);
  lv_obj_set_style_text_color(nameKeyLbl, COL_TEXT_DIM, 0);
  lv_obj_align(nameKeyLbl, LV_ALIGN_LEFT_MID, 8, -8);

  bleNameValueLbl = lv_label_create(nameRow);
  lv_label_set_text(bleNameValueLbl, g_settings.ble_name[0] ? g_settings.ble_name : BLE_DEVICE_NAME_DEFAULT);
  lv_obj_set_style_text_font(bleNameValueLbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(bleNameValueLbl, COL_TEXT, 0);
  lv_obj_align(bleNameValueLbl, LV_ALIGN_LEFT_MID, 8, 8);

  y += SET_ROW_H;

  lv_obj_t* statusRow = lv_obj_create(screen);
  lv_obj_set_size(statusRow, CW, SET_ROW_H);
  lv_obj_set_pos(statusRow, PX, y);
  lv_obj_set_style_bg_color(statusRow, COL_BG, 0);
  lv_obj_set_style_border_width(statusRow, 0, 0);
  lv_obj_set_style_radius(statusRow, 0, 0);
  lv_obj_set_style_pad_all(statusRow, 0, 0);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(statusRow, LV_OBJ_FLAG_CLICKABLE);

  statusDot = lv_obj_create(statusRow);
  lv_obj_set_size(statusDot, 8, 8);
  lv_obj_align(statusDot, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_radius(statusDot, 4, 0);
  lv_obj_set_style_border_width(statusDot, 0, 0);
  lv_obj_set_style_pad_all(statusDot, 0, 0);
  lv_obj_remove_flag(statusDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(statusDot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(statusDot, ble_is_connected() ? COL_GREEN : COL_TEXT_VDIM, 0);

  statusLabel = lv_label_create(statusRow);
  lv_label_set_text(statusLabel, ble_is_connected() ? "Connected" : "Advertising");
  lv_obj_set_style_text_font(statusLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(statusLabel, ble_is_connected() ? COL_GREEN : COL_TEXT_DIM, 0);
  lv_obj_align(statusLabel, LV_ALIGN_LEFT_MID, 22, 0);

  y += SET_ROW_H;

  make_accent_btn(screen, PX, y, CW, SET_ROW_H, "SCAN BLE", scan_cb);
  y += SET_ROW_H + 4;

  lv_obj_t* pairedSecLabel = lv_label_create(screen);
  lv_label_set_text(pairedSecLabel, "PAIRED");
  lv_obj_set_style_text_font(pairedSecLabel, SET_SECTION_FONT, 0);
  lv_obj_set_style_text_color(pairedSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(pairedSecLabel, PX, y + 2);
  y += 16;

  pairedList = lv_obj_create(screen);
  int pairedH = 80;
  lv_obj_set_size(pairedList, CW, pairedH);
  lv_obj_set_pos(pairedList, PX, y);
  lv_obj_set_style_bg_color(pairedList, COL_BG, 0);
  lv_obj_set_style_border_width(pairedList, 0, 0);
  lv_obj_set_style_radius(pairedList, 0, 0);
  lv_obj_set_style_pad_all(pairedList, 0, 0);
  lv_obj_set_flex_flow(pairedList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(pairedList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_add_flag(pairedList, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(pairedList, LV_SCROLLBAR_MODE_OFF);
  y += pairedH;

  lv_obj_t* discSecLabel = lv_label_create(screen);
  lv_label_set_text(discSecLabel, "DISCOVERED");
  lv_obj_set_style_text_font(discSecLabel, SET_SECTION_FONT, 0);
  lv_obj_set_style_text_color(discSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(discSecLabel, PX, y + 2);
  y += 16;

  int discH = 440 - y;
  discoveredList = lv_obj_create(screen);
  lv_obj_set_size(discoveredList, CW, discH);
  lv_obj_set_pos(discoveredList, PX, y);
  lv_obj_set_style_bg_color(discoveredList, COL_BG, 0);
  lv_obj_set_style_border_width(discoveredList, 0, 0);
  lv_obj_set_style_radius(discoveredList, 0, 0);
  lv_obj_set_style_pad_all(discoveredList, 0, 0);
  lv_obj_set_flex_flow(discoveredList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(discoveredList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_add_flag(discoveredList, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(discoveredList, LV_SCROLLBAR_MODE_OFF);

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = SET_BTN_MIN_W;
  int gap = 8;
  make_footer_btn(screen, PX, footerY, btnW, footerH, "BACK", back_cb);
  make_accent_btn(screen, PX + btnW + gap, footerY, btnW, footerH, "SCAN", scan_cb);
  make_footer_btn(screen, PX + 2 * (btnW + gap), footerY, btnW + 40, footerH, "FORGET ALL", forget_all_cb);

  add_paired_placeholder();
  LOG_I("Screen bt: bluetooth screen created");
}

void screen_bt_update() {
  if (bleKbClosePending) cleanup_kb();
  if (scanRunning && bleScanDone) {
    scanRunning = false;
    bleScanDone = false;
    BLEDeviceInfo results[BLE_MAX_DEVICES];
    int count = ble_scan_get_results(results, BLE_MAX_DEVICES);
    if (count > 0 && discoveredList) {
      char buf[65];
      for (int i = 0; i < count; i++) {
        lv_obj_t* row = lv_obj_create(discoveredList);
        lv_obj_set_size(row, 760, 44);
        lv_obj_set_style_bg_color(row, COL_BG_ROW, 0);
        lv_obj_set_style_border_color(row, COL_BORDER_ROW, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, RADIUS_ROW, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        snprintf(buf, sizeof(buf), "%s", results[i].name);
        lv_obj_t* nameLbl = lv_label_create(row);
        lv_label_set_text(nameLbl, buf);
        lv_obj_set_style_text_font(nameLbl, FONT_SUBTITLE, 0);
        lv_obj_set_style_text_color(nameLbl, COL_TEXT, 0);
        lv_obj_align(nameLbl, LV_ALIGN_LEFT_MID, 8, -8);

        snprintf(buf, sizeof(buf), "%s  %ddBm", results[i].addr, results[i].rssi);
        lv_obj_t* detailLbl = lv_label_create(row);
        lv_label_set_text(detailLbl, buf);
        lv_obj_set_style_text_font(detailLbl, FONT_BODY, 0);
        lv_obj_set_style_text_color(detailLbl, COL_TEXT_DIM, 0);
        lv_obj_align(detailLbl, LV_ALIGN_LEFT_MID, 8, 8);
      }
    }
  }
  if (statusDot && statusLabel) {
    bool connected = ble_is_connected();
    lv_obj_set_style_bg_color(statusDot, connected ? COL_GREEN : COL_TEXT_VDIM, 0);
    lv_label_set_text(statusLabel, connected ? "Connected" : "Advertising");
    lv_obj_set_style_text_color(statusLabel, connected ? COL_GREEN : COL_TEXT_DIM, 0);
  }
}
