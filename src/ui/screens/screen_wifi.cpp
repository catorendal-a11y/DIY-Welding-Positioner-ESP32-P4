// TIG Rotator Controller - WiFi Settings Screen
// Network scanning and connection

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "WiFi.h"
#include <cstdio>
#include <cstring>

static lv_obj_t* networkList = nullptr;
static lv_obj_t* passTa = nullptr;
static lv_obj_t* kb = nullptr;
static lv_obj_t* scrollPanel = nullptr;
static lv_obj_t* wifiToggleSw = nullptr;
static lv_obj_t* wifiToggleLbl = nullptr;
static lv_obj_t* connectedCard = nullptr;
static lv_obj_t* connSsidLabel = nullptr;
static lv_obj_t* connDetailLabel = nullptr;
static lv_obj_t* connSignalContainer = nullptr;
static lv_obj_t* connSignalBars[4] = {nullptr, nullptr, nullptr, nullptr};
static bool wifiScanRunning = false;
static bool wifiScanDone = false;

static char selectedSsid[33] = "";

// WiFi state from storage.h (include already present above)


static lv_obj_t* wifiPromptLabel = nullptr;
static lv_obj_t* wifiSsidNameLabel = nullptr;
static volatile bool kbClosePending = false;

static void wifi_kb_cb(lv_event_t* e);

static void cleanup_kb() {
  if (kb) {
    lv_obj_remove_event_cb(kb, wifi_kb_cb);
    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_t* old = kb; kb = nullptr;
    lv_obj_delete_async(old);
  }
  if (passTa) { lv_obj_t* old = passTa; passTa = nullptr; lv_obj_delete_async(old); }
  if (wifiPromptLabel) { lv_obj_t* old = wifiPromptLabel; wifiPromptLabel = nullptr; lv_obj_delete_async(old); }
  if (wifiSsidNameLabel) { lv_obj_t* old = wifiSsidNameLabel; wifiSsidNameLabel = nullptr; lv_obj_delete_async(old); }
  if (scrollPanel) lv_obj_remove_flag(scrollPanel, LV_OBJ_FLAG_HIDDEN);
  kbClosePending = false;
}

static void hide_scroll() {
  if (scrollPanel) lv_obj_add_flag(scrollPanel, LV_OBJ_FLAG_HIDDEN);
}

static void back_cb(lv_event_t* e) {
  cleanup_kb();
  screens_show(SCREEN_SETTINGS);
}

static void connect_wifi() {
  if (!selectedSsid[0]) return;
  const char* pass = passTa ? lv_textarea_get_text(passTa) : g_settings.wifi_pass;
  char passBuf[65];
  strlcpy(passBuf, pass, sizeof(passBuf));
  strlcpy(g_settings.wifi_ssid, selectedSsid, sizeof(g_settings.wifi_ssid));
  strlcpy(g_settings.wifi_pass, passBuf, sizeof(g_settings.wifi_pass));
  storage_save_settings();
  strlcpy(wifiPendingSsid, selectedSsid, sizeof(wifiPendingSsid));
  strlcpy(wifiPendingPass, passBuf, sizeof(wifiPendingPass));
  wifiConnectPending = true;
}

static volatile bool wifiConnectOnClose = false;

static void wifi_kb_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    wifiConnectOnClose = true;
    kbClosePending = true;
  } else if (code == LV_EVENT_CANCEL) {
    kbClosePending = true;
  }
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

static void show_wifi_kb() {
  cleanup_kb();
  hide_scroll();
  lv_obj_t* screen = screenRoots[SCREEN_WIFI];
  wifiPromptLabel = lv_label_create(screen);
  lv_label_set_text(wifiPromptLabel, "Enter password for:");
  lv_obj_set_style_text_font(wifiPromptLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(wifiPromptLabel, COL_TEXT_DIM, 0);
  lv_obj_align(wifiPromptLabel, LV_ALIGN_TOP_MID, 0, 60);
  wifiSsidNameLabel = lv_label_create(screen);
  lv_label_set_text(wifiSsidNameLabel, selectedSsid);
  lv_obj_set_style_text_font(wifiSsidNameLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(wifiSsidNameLabel, COL_ACCENT, 0);
  lv_obj_align(wifiSsidNameLabel, LV_ALIGN_TOP_MID, 0, 82);
  passTa = lv_textarea_create(screen);
  lv_obj_set_size(passTa, 460, 50);
  lv_obj_align(passTa, LV_ALIGN_TOP_MID, 0, 120);
  lv_textarea_set_one_line(passTa, true);
  lv_textarea_set_placeholder_text(passTa, "Password");
  lv_textarea_set_password_mode(passTa, true);
  lv_obj_set_style_bg_color(passTa, COL_BG_INPUT, 0);
  lv_obj_set_style_border_color(passTa, COL_ACCENT, 0);
  lv_obj_set_style_border_width(passTa, 2, 0);
  lv_obj_set_style_text_color(passTa, COL_TEXT, 0);
  lv_obj_set_style_text_font(passTa, FONT_BTN, 0);
  kb = lv_keyboard_create(screen);
  lv_keyboard_set_textarea(kb, passTa);
  lv_obj_set_size(kb, 700, 220);
  lv_obj_align(kb, LV_ALIGN_TOP_MID, 0, 184);
  style_kb(kb);
  lv_obj_add_event_cb(kb, wifi_kb_cb, LV_EVENT_READY, nullptr);
  lv_obj_add_event_cb(kb, wifi_kb_cb, LV_EVENT_CANCEL, nullptr);
}

static void create_signal_bars(lv_obj_t* parent, lv_obj_t* bars[4], int filled) {
  int barW = 4;
  int barH = 4;
  int gap = 1;
  int totalH = 4 * barH + 3 * gap;
  for (int i = 0; i < 4; i++) {
    bars[i] = lv_obj_create(parent);
    lv_obj_set_size(bars[i], barW, barH);
    lv_obj_set_pos(bars[i], 0, totalH - (i + 1) * (barH + gap));
    lv_obj_set_style_bg_opa(bars[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bars[i], 0, 0);
    lv_obj_set_style_radius(bars[i], 0, 0);
    lv_obj_set_style_pad_all(bars[i], 0, 0);
    lv_obj_remove_flag(bars[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bars[i], LV_OBJ_FLAG_CLICKABLE);
    if (i < filled) {
      lv_obj_set_style_bg_color(bars[i], COL_GREEN, 0);
    } else {
      lv_obj_set_style_bg_color(bars[i], COL_TOGGLE_OFF, 0);
    }
  }
}

static int rssi_to_bars(int rssi) {
  if (rssi > -50) return 4;
  if (rssi > -70) return 3;
  if (rssi > -85) return 2;
  return 1;
}

static void network_click_cb(lv_event_t* e) {
  const char* ssid = (const char*)lv_event_get_user_data(e);
  if (!ssid) return;
  strlcpy(selectedSsid, ssid, sizeof(selectedSsid));
  show_wifi_kb();
}

static void wifi_toggle_cb(lv_event_t* e) {
  g_settings.wifi_enabled = !g_settings.wifi_enabled;
  wifiTogglePending = true;
  storage_save_settings();
  if (g_settings.wifi_enabled) {
    lv_obj_set_style_bg_color(wifiToggleSw, COL_GREEN, 0);
    lv_label_set_text(wifiToggleLbl, "ON");
  } else {
    lv_obj_set_style_bg_color(wifiToggleSw, COL_TOGGLE_OFF, 0);
    lv_label_set_text(wifiToggleLbl, "OFF");
  }
}

static void populate_scan_results() {
  WifiScanEntry localBuf[WIFI_SCAN_MAX];
  int n;
  if (g_wifi_mutex) xSemaphoreTake(g_wifi_mutex, portMAX_DELAY);
  n = wifiScanBufferCount;
  if (n > 0) memcpy(localBuf, wifiScanBuffer, n * sizeof(WifiScanEntry));
  if (g_wifi_mutex) xSemaphoreGive(g_wifi_mutex);

  lv_obj_clean(networkList);
  if (n <= 0) {
    wifiScanDone = false;
    return;
  }
  char buf[40];
  for (int i = 0; i < n; i++) {
    const char* ssid = localBuf[i].ssid;
    int rssi = localBuf[i].rssi;
    bool secured = (localBuf[i].enc != WIFI_AUTH_OPEN);

    lv_obj_t* row = lv_obj_create(networkList);
    lv_obj_set_size(row, 760, 46);
    lv_obj_set_style_bg_color(row, COL_BG_ROW, 0);
    lv_obj_set_style_border_color(row, COL_BORDER_ROW, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, RADIUS_ROW, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    snprintf(buf, sizeof(buf), "%s%s", ssid, secured ? " [L]" : "");
    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* sigCont = lv_obj_create(row);
    lv_obj_set_size(sigCont, 6, 20);
    lv_obj_align(sigCont, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_opa(sigCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sigCont, 0, 0);
    lv_obj_set_style_pad_all(sigCont, 0, 0);
    lv_obj_set_style_radius(sigCont, 0, 0);
    lv_obj_remove_flag(sigCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(sigCont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* bars[4] = {nullptr};
    create_signal_bars(sigCont, bars, rssi_to_bars(rssi));

    char* ssidCopy = new char[strlen(ssid) + 1];
    strlcpy(ssidCopy, ssid, strlen(ssid) + 1);
    lv_obj_add_event_cb(row, network_click_cb, LV_EVENT_CLICKED, ssidCopy);
    lv_obj_add_event_cb(row, [](lv_event_t* e) {
      void* ud = lv_event_get_user_data(e);
      if (ud) delete[] (char*)ud;
    }, LV_EVENT_DELETE, nullptr);
  }
  wifiScanDone = false;
}

static void scan_wifi_cb(lv_event_t* e) {
  if (wifiScanRunning || wifiScanPending) return;
  wifiScanPending = true;
  wifiScanRunning = true;
  wifiScanDone = false;
  lv_obj_clean(networkList);
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

static void update_connected_card() {
  char ssidBuf[33];
  char ipBuf[16];
  int rssi;
  bool connected;
  if (g_wifi_mutex) xSemaphoreTake(g_wifi_mutex, portMAX_DELAY);
  connected = wifiIsConnected;
  strlcpy(ssidBuf, wifiConnectedSsid, sizeof(ssidBuf));
  strlcpy(ipBuf, wifiConnectedIp, sizeof(ipBuf));
  rssi = wifiConnectedRssi;
  if (g_wifi_mutex) xSemaphoreGive(g_wifi_mutex);

  if (connected) {
    lv_obj_remove_flag(connectedCard, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(connSsidLabel, ssidBuf);
    char detail[50];
    snprintf(detail, sizeof(detail), "%s | %ddBm", ipBuf, rssi);
    lv_label_set_text(connDetailLabel, detail);
    int bars = rssi_to_bars(rssi);
    for (int i = 0; i < 4; i++) {
      if (connSignalBars[i]) {
        lv_obj_set_style_bg_color(connSignalBars[i],
          i < bars ? COL_GREEN : COL_TOGGLE_OFF, 0);
      }
    }
  } else {
    lv_obj_add_flag(connectedCard, LV_OBJ_FLAG_HIDDEN);
  }
}

void screen_wifi_create() {
  lv_obj_t* screen = screenRoots[SCREEN_WIFI];
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;
  const int CONTENT_W = SCREEN_W - 2 * PX;

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, SET_HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "WIFI");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PX, 6);

  int y = SET_HEADER_H;

  lv_obj_t* toggleRow = lv_obj_create(screen);
  lv_obj_set_size(toggleRow, CONTENT_W, SET_ROW_H);
  lv_obj_set_pos(toggleRow, PX, y);
  lv_obj_set_style_bg_color(toggleRow, COL_BG, 0);
  lv_obj_set_style_border_width(toggleRow, 0, 0);
  lv_obj_set_style_radius(toggleRow, 0, 0);
  lv_obj_set_style_pad_all(toggleRow, 0, 0);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* wifiLabel = lv_label_create(toggleRow);
  lv_label_set_text(wifiLabel, "WiFi");
  lv_obj_set_style_text_font(wifiLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(wifiLabel, COL_TEXT, 0);
  lv_obj_align(wifiLabel, LV_ALIGN_LEFT_MID, 8, 0);

  wifiToggleSw = lv_button_create(toggleRow);
  lv_obj_set_size(wifiToggleSw, SET_TOGGLE_W, SET_TOGGLE_H);
  lv_obj_align(wifiToggleSw, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_radius(wifiToggleSw, SET_TOGGLE_R, 0);
  lv_obj_set_style_border_width(wifiToggleSw, 0, 0);
  lv_obj_set_style_shadow_width(wifiToggleSw, 0, 0);
  lv_obj_set_style_pad_all(wifiToggleSw, 0, 0);
  lv_obj_remove_flag(wifiToggleSw, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(wifiToggleSw, g_settings.wifi_enabled ? COL_GREEN : COL_TOGGLE_OFF, 0);
  lv_obj_add_event_cb(wifiToggleSw, wifi_toggle_cb, LV_EVENT_CLICKED, nullptr);

  wifiToggleLbl = lv_label_create(wifiToggleSw);
  lv_label_set_text(wifiToggleLbl, g_settings.wifi_enabled ? "ON" : "OFF");
  lv_obj_set_style_text_font(wifiToggleLbl, FONT_BTN, 0);
  lv_obj_set_style_text_color(wifiToggleLbl, COL_TEXT_WHITE, 0);
  lv_obj_center(wifiToggleLbl);

  y += SET_ROW_H;

  lv_obj_t* connSecLabel = lv_label_create(screen);
  lv_label_set_text(connSecLabel, "CONNECTED");
  lv_obj_set_style_text_font(connSecLabel, SET_SECTION_FONT, 0);
  lv_obj_set_style_text_color(connSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(connSecLabel, PX, y + 3);
  y += 16;

  connectedCard = lv_obj_create(screen);
  lv_obj_set_size(connectedCard, CONTENT_W, 64);
  lv_obj_set_pos(connectedCard, PX, y);
  lv_obj_set_style_bg_color(connectedCard, COL_BG_CARD, 0);
  lv_obj_set_style_border_color(connectedCard, COL_BORDER_ROW, 0);
  lv_obj_set_style_border_width(connectedCard, 1, 0);
  lv_obj_set_style_radius(connectedCard, RADIUS_CARD, 0);
  lv_obj_set_style_pad_all(connectedCard, 0, 0);
  lv_obj_set_style_shadow_width(connectedCard, 0, 0);
  lv_obj_remove_flag(connectedCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(connectedCard, LV_OBJ_FLAG_CLICKABLE);

  connSsidLabel = lv_label_create(connectedCard);
  lv_obj_set_style_text_font(connSsidLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(connSsidLabel, COL_ACCENT, 0);
  lv_obj_set_pos(connSsidLabel, 12, 7);

  connDetailLabel = lv_label_create(connectedCard);
  lv_obj_set_style_text_font(connDetailLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(connDetailLabel, COL_TEXT_DIM, 0);
  lv_obj_set_pos(connDetailLabel, 12, 33);

  connSignalContainer = lv_obj_create(connectedCard);
  lv_obj_set_size(connSignalContainer, 6, 20);
  lv_obj_align(connSignalContainer, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_set_style_bg_opa(connSignalContainer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(connSignalContainer, 0, 0);
  lv_obj_set_style_pad_all(connSignalContainer, 0, 0);
  lv_obj_set_style_radius(connSignalContainer, 0, 0);
  lv_obj_remove_flag(connSignalContainer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(connSignalContainer, LV_OBJ_FLAG_CLICKABLE);
  create_signal_bars(connSignalContainer, connSignalBars, 0);

  y += 64;

  lv_obj_t* availSecLabel = lv_label_create(screen);
  lv_label_set_text(availSecLabel, "AVAILABLE");
  lv_obj_set_style_text_font(availSecLabel, SET_SECTION_FONT, 0);
  lv_obj_set_style_text_color(availSecLabel, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(availSecLabel, PX, y + 3);
  y += 16;

  int listH = SET_FOOTER_Y - y - 4;
  networkList = lv_obj_create(screen);
  lv_obj_set_size(networkList, CONTENT_W, listH);
  lv_obj_set_pos(networkList, PX, y);
  lv_obj_set_style_bg_color(networkList, COL_BG, 0);
  lv_obj_set_style_border_width(networkList, 0, 0);
  lv_obj_set_style_radius(networkList, 0, 0);
  lv_obj_set_style_pad_all(networkList, 0, 0);
  lv_obj_set_flex_flow(networkList, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(networkList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_add_flag(networkList, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(networkList, LV_SCROLLBAR_MODE_OFF);

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = SET_BTN_MIN_W;
  int gap = 10;

  make_footer_btn(screen, PX, footerY, btnW, footerH, "BACK", back_cb);
  make_accent_btn(screen, PX + btnW + gap, footerY, btnW, footerH, "SCAN", scan_wifi_cb);
  make_accent_btn(screen, PX + 2 * (btnW + gap), footerY, btnW + 50, footerH, "+ HIDDEN", [](lv_event_t* e) {
    strlcpy(selectedSsid, "(hidden)", sizeof(selectedSsid));
    show_wifi_kb();
  });

  scrollPanel = networkList;

  update_connected_card();
  LOG_I("Screen wifi: dedicated wifi layout created");
}

void screen_wifi_invalidate_widgets() {
  kb = nullptr;
  passTa = nullptr;
  wifiPromptLabel = nullptr;
  wifiSsidNameLabel = nullptr;
  networkList = nullptr;
  scrollPanel = nullptr;
  wifiToggleSw = nullptr;
  wifiToggleLbl = nullptr;
  connectedCard = nullptr;
  connSsidLabel = nullptr;
  connDetailLabel = nullptr;
  connSignalContainer = nullptr;
  for (int i = 0; i < 4; i++) connSignalBars[i] = nullptr;
  kbClosePending = false;
  wifiScanRunning = false;
  wifiScanDone = false;
  wifiConnectOnClose = false;
}

void screen_wifi_update() {
  if (kbClosePending) {
    bool doConnect = wifiConnectOnClose;
    wifiConnectOnClose = false;
    if (doConnect) connect_wifi();
    cleanup_kb();
  }

  if (wifiScanResultReady) {
    wifiScanResultReady = false;
    wifiScanRunning = false;
    wifiScanDone = true;
    populate_scan_results();
  }
  if (wifiScanFailed) {
    wifiScanFailed = false;
    wifiScanRunning = false;
    wifiScanDone = false;
  }

  if (connectedCard) update_connected_card();
}
