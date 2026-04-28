// TIG Rotator Controller - System Info Screen
// FW version, free heap, uptime, core info display
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "../../onchip_temp.h"
#include <cstdio>
#include <cstring>


static lv_obj_t* uptimeLabel = nullptr;
static lv_obj_t* heapBar = nullptr;
static lv_obj_t* heapValueLabel = nullptr;
static lv_obj_t* psramBar = nullptr;
static lv_obj_t* psramValueLabel = nullptr;
static lv_obj_t* coreLoadLabel = nullptr;
static lv_obj_t* tempLabel = nullptr;
static uint32_t bootMs = 0;
static uint32_t lastTempRead = 0;
static float cachedTemp = 0.0f;
static uint32_t lastUptimeSec = UINT32_MAX;
static uint32_t lastMemoryRead = 0;
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

static lv_obj_t* make_key_label(lv_obj_t* parent, int x, int y, const char* text, const lv_font_t* font) {
  lv_obj_t* lbl = lv_label_create(parent);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, font, 0);
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
  lv_obj_set_style_bg_color(bar, COL_SLIDER_TRACK, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
  return bar;
}

void screen_sysinfo_create() {
  lv_obj_t* screen = screenRoots[SCREEN_SYSINFO];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  bootMs = millis();
  lastUptimeSec = UINT32_MAX;
  lastMemoryRead = 0;
  lastTempRead = 0;
  lastCoreRead = 0;

  ui_create_settings_header(screen, "SYSTEM INFO", "", COL_TEXT_DIM);
  ui_create_post_card(screen, SYSINFO_CARD_X, SYSINFO_CARD1_Y, SYSINFO_CARD_W, SYSINFO_CARD1_H);
  ui_create_post_card(screen, SYSINFO_CARD_X, SYSINFO_CARD2_Y, SYSINFO_CARD_W, SYSINFO_CARD2_H);
  ui_create_post_card(screen, SYSINFO_CARD_X, SYSINFO_CARD3_Y, SYSINFO_CARD_W, SYSINFO_CARD3_H);

  const int yFw = SYSINFO_CARD1_Y + 22;
  const int yUp = SYSINFO_CARD1_Y + 48;
  char buf[48];
  snprintf(buf, sizeof(buf), "%s  %s %s", FW_VERSION, __DATE__, __TIME__);
  make_key_label(screen, SYSINFO_TEXT_X, yFw, "FIRMWARE", FONT_NORMAL);
  make_val_label(screen, SYSINFO_VAL_COL, yFw, buf);
  make_key_label(screen, SYSINFO_TEXT_X, yUp, "UPTIME", FONT_NORMAL);
  uptimeLabel = make_val_label(screen, SYSINFO_VAL_COL, yUp, "00:00:00");

  lv_obj_t* memTitle = lv_label_create(screen);
  lv_label_set_text(memTitle, "MEMORY");
  lv_obj_set_style_text_font(memTitle, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(memTitle, COL_ACCENT, 0);
  lv_obj_set_pos(memTitle, SYSINFO_TEXT_X, SYSINFO_MEM_TITLE_Y);

  make_key_label(screen, SYSINFO_TEXT_X, SYSINFO_HEAP_KEY_Y, "HEAP", FONT_SMALL);
  size_t freeHeap = esp_get_free_heap_size();
  size_t totalHeap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  int heapPct = totalHeap > 0 ? (int)((uint64_t)freeHeap * 100 / totalHeap) : 0;
  snprintf(buf, sizeof(buf), "%u KB", (unsigned)(freeHeap / 1024));
  heapValueLabel = make_val_label(screen, SYSINFO_HEAP_VAL_X, SYSINFO_HEAP_KEY_Y, buf);
  heapBar = make_bar(screen, SYSINFO_BAR_X, SYSINFO_HEAP_BAR_Y, SYSINFO_BAR_W, SET_BAR_H, COL_GREEN);
  lv_bar_set_value(heapBar, heapPct, LV_ANIM_OFF);

  make_key_label(screen, SYSINFO_TEXT_X, SYSINFO_PSRAM_KEY_Y, "PSRAM", FONT_SMALL);
  size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  int psramPct = psramTotal > 0 ? (int)((uint64_t)freePsram * 100 / psramTotal) : 0;
  snprintf(buf, sizeof(buf), "%.1f MB", (double)freePsram / (1024.0 * 1024.0));
  psramValueLabel = make_val_label(screen, SYSINFO_HEAP_VAL_X, SYSINFO_PSRAM_KEY_Y, buf);
  psramBar = make_bar(screen, SYSINFO_BAR_X, SYSINFO_PSRAM_BAR_Y, SYSINFO_BAR_W, SET_BAR_H, COL_GREEN);
  lv_bar_set_value(psramBar, psramPct, LV_ANIM_OFF);

  make_key_label(screen, SYSINFO_TEXT_X, SYSINFO_SYS_ROW_Y, "CPU TEMP", FONT_NORMAL);
  tempLabel = make_val_label(screen, SYSINFO_VAL_COL, SYSINFO_SYS_ROW_Y, "-- C");
  make_key_label(screen, SYSINFO_CORE_KEY_X, SYSINFO_SYS_ROW_Y, "CORE LOAD", FONT_NORMAL);
  coreLoadLabel = make_val_label(screen, SYSINFO_CORE_VAL_X, SYSINFO_SYS_ROW_Y, "-- / --");

  const int gap = 20;
  ui_create_btn(screen, SYSINFO_CARD_X, SYSINFO_FOOTER_Y, SYSINFO_FOOT_BTN_W, SYSINFO_FOOTER_H, "<  BACK",
                SET_BTN_FONT, UI_BTN_NORMAL, back_cb, nullptr);
  ui_create_btn(screen, SYSINFO_CARD_X + SYSINFO_FOOT_BTN_W + gap, SYSINFO_FOOTER_Y, SYSINFO_FOOT_BTN_W,
                SYSINFO_FOOTER_H, "REBOOT", SET_BTN_FONT, UI_BTN_NORMAL, reboot_cb, nullptr);

  LOG_I("Screen sysinfo: system info screen created");
}

void screen_sysinfo_invalidate_widgets() {
  uptimeLabel = nullptr;
  heapBar = nullptr;
  heapValueLabel = nullptr;
  psramBar = nullptr;
  psramValueLabel = nullptr;
  coreLoadLabel = nullptr;
  tempLabel = nullptr;
  lastUptimeSec = UINT32_MAX;
  lastMemoryRead = 0;
  lastTempRead = 0;
  lastCoreRead = 0;
}

void screen_sysinfo_update() {
  if (!uptimeLabel) return;
  char buf[24];
  uint32_t now = millis();

  // Uptime
  uint32_t elapsed = (now - bootMs) / 1000;
  if (elapsed != lastUptimeSec) {
    lastUptimeSec = elapsed;
    uint32_t h = elapsed / 3600;
    uint32_t m = (elapsed % 3600) / 60;
    uint32_t s = elapsed % 60;
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    lv_label_set_text(uptimeLabel, buf);
  }

  if (now - lastMemoryRead >= 1000) {
    lastMemoryRead = now;

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

  }

  // Core load (update every 2 seconds, delta-based)
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
  if (coreLoadLabel) {
    snprintf(buf, sizeof(buf), "%d%% / %d%%", cachedCore0Pct, cachedCore1Pct);
    lv_label_set_text(coreLoadLabel, buf);
  }

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
