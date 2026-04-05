// TIG Rotator Controller - System Info Screen
// FW version, free heap, uptime, core info display
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../ble/ble.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_partition.h"
#include "freertos/task.h"
#include "../../onchip_temp.h"
#include <cstdio>
#include <cstring>

extern volatile bool wifiIsConnected;
extern char wifiConnectedSsid[33];
extern char wifiConnectedIp[16];
extern volatile int wifiConnectedRssi;

static lv_obj_t* uptimeLabel = nullptr;
static lv_obj_t* heapBar = nullptr;
static lv_obj_t* heapValueLabel = nullptr;
static lv_obj_t* psramBar = nullptr;
static lv_obj_t* psramValueLabel = nullptr;
static lv_obj_t* flashBar = nullptr;
static lv_obj_t* flashValueLabel = nullptr;
static lv_obj_t* wifiStatusLabel = nullptr;
static lv_obj_t* ipLabel = nullptr;
static lv_obj_t* bleStatusLabel = nullptr;
static lv_obj_t* core0Bar = nullptr;
static lv_obj_t* core0Label = nullptr;
static lv_obj_t* core1Bar = nullptr;
static lv_obj_t* core1Label = nullptr;
static lv_obj_t* tempLabel = nullptr;
static uint32_t bootMs = 0;
static uint32_t lastTempRead = 0;
static float cachedTemp = 0.0f;
static uint32_t lastCoreRead = 0;
static int cachedCore0Pct = 0;
static int cachedCore1Pct = 0;
static uint32_t prevTotalRunTime = 0;
static uint32_t prevIdleCore0Time = 0;
static uint32_t prevIdleCore1Time = 0;

static void back_cb(lv_event_t* e) {
  screens_show(SCREEN_SETTINGS);
}

static void reboot_cb(lv_event_t* e) {
  screen_confirm_create("REBOOT DEVICE", "Are you sure? All unsaved data will be lost.",
    []() { esp_restart(); }, nullptr);
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

static lv_obj_t* make_row(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, w, h);
  lv_obj_set_pos(row, x, y);
  lv_obj_set_style_bg_color(row, COL_BG, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_radius(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  return row;
}

static lv_obj_t* make_sep(lv_obj_t* parent, int x, int y, int w) {
  lv_obj_t* sep = lv_obj_create(parent);
  lv_obj_set_size(sep, w, 1);
  lv_obj_set_pos(sep, x, y);
  lv_obj_set_style_bg_color(sep, COL_SEPARATOR, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_CLICKABLE);
  return sep;
}

static lv_obj_t* make_key_label(lv_obj_t* parent, int x, int y, const char* text) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, SET_KEY_FONT, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
  lv_obj_set_pos(lbl, x, y);
  return lbl;
}

static lv_obj_t* make_val_label(lv_obj_t* parent, int x, int y, const char* text) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, SET_VAL_FONT, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT, 0);
  lv_obj_set_pos(lbl, x, y);
  return lbl;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h, lv_color_t color) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_size(bar, w, h);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x1A1A1A), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  return bar;
}

static lv_obj_t* make_section_label(lv_obj_t* parent, int x, int y, const char* text) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, SET_SECTION_FONT, 0);
  lv_obj_set_style_text_color(lbl, COL_TEXT_VDIM, 0);
  lv_obj_set_pos(lbl, x, y);
  return lbl;
}

void screen_sysinfo_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SYSINFO];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  const int PX = 16;
  const int CW = SCREEN_W - 2 * PX;
  bootMs = millis();

  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, 28);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "SYSTEM INFO");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PX, 6);

  int y = 34;
  int rowH = 28;
  int keyX = PX + 8;
  int valX = PX + 140;
  int barX = PX + 260;
  int barW = CW - 260 - 8;

  char buf[40];
  snprintf(buf, sizeof(buf), "%s  %s %s", FW_VERSION, __DATE__, __TIME__);
  make_key_label(screen, keyX, y, "FIRMWARE");
  make_val_label(screen, valX, y, buf);
  y += rowH;

  uptimeLabel = make_val_label(screen, valX, y, "00:00:00");
  make_key_label(screen, keyX, y, "UPTIME");
  y += rowH + 4;

  make_sep(screen, PX, y, CW);
  y += 6;

  make_section_label(screen, keyX, y, "MEMORY");
  y += 14;

  make_key_label(screen, keyX, y, "HEAP");
  size_t freeHeap = esp_get_free_heap_size();
  size_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  int heapPct = totalHeap > 0 ? (int)((uint64_t)freeHeap * 100 / totalHeap) : 0;
  snprintf(buf, sizeof(buf), "%u KB", (unsigned)(freeHeap / 1024));
  heapValueLabel = make_val_label(screen, valX, y, buf);
  heapBar = make_bar(screen, barX, y + 3, barW, SET_BAR_H, COL_GREEN);
  lv_bar_set_value(heapBar, heapPct, LV_ANIM_OFF);
  y += rowH;

  make_key_label(screen, keyX, y, "PSRAM");
  size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  int psramPct = psramTotal > 0 ? (int)((uint64_t)freePsram * 100 / psramTotal) : 0;
  snprintf(buf, sizeof(buf), "%.1f MB", (double)freePsram / (1024.0 * 1024.0));
  psramValueLabel = make_val_label(screen, valX, y, buf);
  psramBar = make_bar(screen, barX, y + 3, barW, SET_BAR_H, COL_GREEN);
  lv_bar_set_value(psramBar, psramPct, LV_ANIM_OFF);
  y += rowH;

  make_key_label(screen, keyX, y, "FLASH");
  const esp_partition_t* ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t* ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  size_t appUsed = ota0 ? ota0->size : 0;
  size_t appTotal = appUsed + (ota1 ? ota1->size : 0);
  const esp_partition_t* spiffs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
  size_t storageTotal = spiffs ? spiffs->size : 0;
  size_t flashTotal = appTotal + storageTotal;
  int flashPct = appTotal > 0 ? (int)((uint64_t)appUsed * 100 / appTotal) : 0;
  snprintf(buf, sizeof(buf), "%.1f MB", (double)flashTotal / (1024.0 * 1024.0));
  flashValueLabel = make_val_label(screen, valX, y, buf);
  flashBar = make_bar(screen, barX, y + 3, barW, SET_BAR_H, lv_color_hex(0xFF9500));
  lv_bar_set_value(flashBar, flashPct, LV_ANIM_OFF);
  y += rowH + 4;

  make_sep(screen, PX, y, CW);
  y += 6;

  make_section_label(screen, keyX, y, "CONNECTIVITY");
  y += 14;

  make_key_label(screen, keyX, y, "WiFi");
  if (wifiIsConnected) {
    char wifiBuf[64];
    snprintf(wifiBuf, sizeof(wifiBuf), "Connected - %s", wifiConnectedSsid);
    wifiStatusLabel = make_val_label(screen, valX, y, wifiBuf);
    lv_obj_set_style_text_color(wifiStatusLabel, COL_GREEN, 0);
  } else {
    wifiStatusLabel = make_val_label(screen, valX, y, "Disconnected");
  }
  y += rowH;

  make_key_label(screen, keyX, y, "IP");
  if (wifiIsConnected) {
    ipLabel = make_val_label(screen, valX, y, wifiConnectedIp);
  } else {
    ipLabel = make_val_label(screen, valX, y, "--");
  }
  y += rowH;

  make_key_label(screen, keyX, y, "BLE");
  if (ble_is_enabled()) {
    bleStatusLabel = make_val_label(screen, valX, y, ble_is_connected() ? "Active - Connected" : "Active - Advertising");
  } else {
    bleStatusLabel = make_val_label(screen, valX, y, "Disabled");
  }
  y += rowH + 4;

  make_sep(screen, PX, y, CW);
  y += 6;

  make_section_label(screen, keyX, y, "SYSTEM");
  y += 14;

  int smallBarW = 100;
  make_key_label(screen, keyX, y, "Core 0");
  core0Label = make_val_label(screen, valX, y, "--");
  core0Bar = make_bar(screen, valX + 80, y + 3, smallBarW, SET_BAR_H, COL_GREEN);
  y += rowH;

  make_key_label(screen, keyX, y, "Core 1");
  core1Label = make_val_label(screen, valX, y, "--");
  core1Bar = make_bar(screen, valX + 80, y + 3, smallBarW, SET_BAR_H, COL_GREEN);
  y += rowH;

  make_key_label(screen, keyX, y, "Temperature");
  tempLabel = make_val_label(screen, valX, y, "-- C");

  int footerY = SET_FOOTER_Y;
  int footerH = SET_FOOTER_H;
  int btnW = SET_BTN_MIN_W;
  int gap = 8;
  make_footer_btn(screen, PX, footerY, btnW, footerH, "BACK", back_cb);
  make_footer_btn(screen, PX + btnW + gap, footerY, btnW, footerH, "REBOOT", reboot_cb);

  LOG_I("Screen sysinfo: system info screen created");
}

void screen_sysinfo_update() {
  if (!uptimeLabel) return;
  char buf[24];

  // Uptime
  uint32_t elapsed = (millis() - bootMs) / 1000;
  uint32_t h = elapsed / 3600;
  uint32_t m = (elapsed % 3600) / 60;
  uint32_t s = elapsed % 60;
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  lv_label_set_text(uptimeLabel, buf);

  // Heap
  size_t freeHeap = esp_get_free_heap_size();
  size_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  int heapPct = totalHeap > 0 ? (int)((uint64_t)freeHeap * 100 / totalHeap) : 0;
  if (heapBar) lv_bar_set_value(heapBar, heapPct, LV_ANIM_OFF);
  if (heapValueLabel) {
    snprintf(buf, sizeof(buf), "%u KB", (unsigned)(freeHeap / 1024));
    lv_label_set_text(heapValueLabel, buf);
  }

  // PSRAM
  size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  int psramPct = psramTotal > 0 ? (int)((uint64_t)freePsram * 100 / psramTotal) : 0;
  if (psramBar) lv_bar_set_value(psramBar, psramPct, LV_ANIM_OFF);
  if (psramValueLabel) {
    snprintf(buf, sizeof(buf), "%.1f MB", (double)freePsram / (1024.0 * 1024.0));
    lv_label_set_text(psramValueLabel, buf);
  }

  // Flash
  const esp_partition_t* ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t* ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  size_t appUsed = ota0 ? ota0->size : 0;
  size_t appTotal = appUsed + (ota1 ? ota1->size : 0);
  int flashPct = appTotal > 0 ? (int)((uint64_t)appUsed * 100 / appTotal) : 0;
  if (flashBar) lv_bar_set_value(flashBar, flashPct, LV_ANIM_OFF);

  // WiFi status (from cached values updated by storageTask)
  if (wifiIsConnected) {
    char wifiBuf[64];
    snprintf(wifiBuf, sizeof(wifiBuf), "Connected - %s", wifiConnectedSsid);
    if (wifiStatusLabel) lv_label_set_text(wifiStatusLabel, wifiBuf);
    if (wifiStatusLabel) lv_obj_set_style_text_color(wifiStatusLabel, COL_GREEN, 0);
    if (ipLabel) lv_label_set_text(ipLabel, wifiConnectedIp);
  } else {
    if (wifiStatusLabel) lv_label_set_text(wifiStatusLabel, "Disconnected");
    if (wifiStatusLabel) lv_obj_set_style_text_color(wifiStatusLabel, COL_TEXT, 0);
    if (ipLabel) lv_label_set_text(ipLabel, "--");
  }

  // Core load (update every 2 seconds, delta-based)
  uint32_t now = millis();
  if (now - lastCoreRead >= 2000) {
    lastCoreRead = now;
    TaskStatus_t* taskArray = nullptr;
    UBaseType_t taskCount = 0;
    uint32_t totalRunTime = 0;
    uint32_t idleCore0Time = 0;
    uint32_t idleCore1Time = 0;

    taskCount = uxTaskGetNumberOfTasks();
    taskArray = (TaskStatus_t*)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
    if (taskArray) {
      taskCount = uxTaskGetSystemState(taskArray, taskCount, &totalRunTime);
      for (UBaseType_t i = 0; i < taskCount; i++) {
        if (strncmp(taskArray[i].pcTaskName, "IDLE", 4) == 0) {
          if (taskArray[i].xCoreID == 0) idleCore0Time = taskArray[i].ulRunTimeCounter;
          else if (taskArray[i].xCoreID == 1) idleCore1Time = taskArray[i].ulRunTimeCounter;
        }
      }
      vPortFree(taskArray);

      if (prevTotalRunTime > 0 && totalRunTime > prevTotalRunTime) {
        uint32_t deltaTotal = totalRunTime - prevTotalRunTime;
        uint32_t deltaIdle0 = idleCore0Time - prevIdleCore0Time;
        uint32_t deltaIdle1 = idleCore1Time - prevIdleCore1Time;
        cachedCore0Pct = 100 - (int)(deltaIdle0 * 100 / deltaTotal);
        cachedCore1Pct = 100 - (int)(deltaIdle1 * 100 / deltaTotal);
        if (cachedCore0Pct < 0) cachedCore0Pct = 0;
        if (cachedCore1Pct < 0) cachedCore1Pct = 0;
      }
      prevTotalRunTime = totalRunTime;
      prevIdleCore0Time = idleCore0Time;
      prevIdleCore1Time = idleCore1Time;
    }
  }
  if (core0Label) {
    snprintf(buf, sizeof(buf), "%d%%", cachedCore0Pct);
    lv_label_set_text(core0Label, buf);
  }
  if (core0Bar) lv_bar_set_value(core0Bar, cachedCore0Pct, LV_ANIM_OFF);
  if (core1Label) {
    snprintf(buf, sizeof(buf), "%d%%", cachedCore1Pct);
    lv_label_set_text(core1Label, buf);
  }
  if (core1Bar) lv_bar_set_value(core1Bar, cachedCore1Pct, LV_ANIM_OFF);

  // Temperature (update every 3 seconds)
  if (now - lastTempRead >= 3000) {
    lastTempRead = now;
    float tsensVal = 0.0f;
    if (onchip_temp_get_celsius(&tsensVal)) {
      cachedTemp = tsensVal;
    }
  }
  if (tempLabel) {
    snprintf(buf, sizeof(buf), "%.1f C", cachedTemp);
    lv_label_set_text(tempLabel, buf);
  }
}
