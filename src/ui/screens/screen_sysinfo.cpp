// TIG Rotator Controller - System Info Screen
// FW version, free heap, uptime, core info display
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/task.h"
#include "../../onchip_temp.h"
#include "../../control/control.h"
#include "../../motor/motor.h"
#include "../../storage/storage.h"
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
static bool tempValid = false;
static uint32_t lastUptimeSec = UINT32_MAX;
static uint32_t lastMemoryRead = 0;
static uint32_t lastCoreRead = 0;
static int cachedCore0Pct = 0;
static int cachedCore1Pct = 0;
static uint32_t prevTotalRunTime = 0;
static uint32_t prevIdleCore0Time = 0;
static uint32_t prevIdleCore1Time = 0;

static void back_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }

static void reboot_cb(lv_event_t* e) {
  if (control_get_state() != STATE_IDLE || storage_status() != STORAGE_SAVED) return;
  screen_confirm_create(
      "REBOOT DEVICE", "Are you sure? All unsaved data will be lost.",
      []() {
        if (control_get_state() == STATE_IDLE && storage_status() == STORAGE_SAVED) {
          motor_disable();
          esp_restart();
        }
      },
      nullptr);
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
  bootMs = 0;
  lastUptimeSec = UINT32_MAX;
  lastMemoryRead = lastTempRead = lastCoreRead = 0;
  ui_create_header(screen, "System information", "CONTROLLER HEALTH", nullptr);
  lv_obj_t* fw = ui_create_post_card(screen, 24, 94, 368, 82);
  ui_create_text(fw, 16, 10, 336, "FIRMWARE", FONT_NORMAL, COL_TEXT_DIM);
  ui_create_text(fw, 16, 38, 336, FW_VERSION, FONT_XXL, COL_TEXT);
  lv_obj_t* up = ui_create_post_card(screen, 408, 94, 368, 82);
  ui_create_text(up, 16, 10, 336, "UPTIME", FONT_NORMAL, COL_TEXT_DIM);
  uptimeLabel = ui_create_text(up, 16, 38, 336, "00:00:00", FONT_XXL, COL_TEXT);
  lv_obj_t* mem = ui_create_post_card(screen, 24, 196, 368, 178);
  ui_create_text(mem, 16, 14, 336, "MEMORY AVAILABLE", FONT_NORMAL, COL_TEXT_DIM);
  ui_create_text(mem, 16, 50, 110, "Heap", FONT_SUBTITLE, COL_TEXT);
  heapValueLabel = ui_create_text(mem, 180, 50, 172, "--", FONT_SUBTITLE, COL_TEXT);
  heapBar = make_bar(mem, 16, 83, 336, 8, COL_GREEN);
  ui_create_text(mem, 16, 111, 110, "PSRAM", FONT_SUBTITLE, COL_TEXT);
  psramValueLabel = ui_create_text(mem, 180, 111, 172, "--", FONT_SUBTITLE, COL_TEXT);
  psramBar = make_bar(mem, 16, 144, 336, 8, COL_GREEN);
  lv_obj_t* cpu = ui_create_post_card(screen, 408, 196, 368, 178);
  ui_create_text(cpu, 16, 14, 336, "PROCESSOR", FONT_NORMAL, COL_TEXT_DIM);
  tempLabel = ui_create_text(cpu, 16, 48, 336, "-- C", FONT_XXL, COL_TEXT);
  ui_create_text(cpu, 16, 104, 336, "CORE 0 / CORE 1 LOAD", FONT_NORMAL, COL_TEXT_DIM);
  coreLoadLabel = ui_create_text(cpu, 16, 132, 336, "-- / --", FONT_XL, COL_TEXT);
  ui_create_btn(screen, 24, 408, 152, 56, "<  BACK", FONT_BTN, UI_BTN_NORMAL, back_cb, nullptr);
  ui_create_btn(screen, 496, 408, 280, 56, "REBOOT", FONT_BTN, UI_BTN_NORMAL, reboot_cb, nullptr);
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
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
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
          if (taskArray[i].xCoreID == 0)
            idleCore0Time = taskArray[i].ulRunTimeCounter;
          else if (taskArray[i].xCoreID == 1)
            idleCore1Time = taskArray[i].ulRunTimeCounter;
        }
      }
      vPortFree(taskArray);

      if (prevTotalRunTime > 0 && totalRunTime > prevTotalRunTime) {
        uint32_t deltaTotal = totalRunTime - prevTotalRunTime;
        uint32_t deltaIdle0 = idleCore0Time - prevIdleCore0Time;
        uint32_t deltaIdle1 = idleCore1Time - prevIdleCore1Time;
        cachedCore0Pct = 100 - (int)((uint64_t)deltaIdle0 * 100 / deltaTotal);
        cachedCore1Pct = 100 - (int)((uint64_t)deltaIdle1 * 100 / deltaTotal);
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
  if (!tempValid || now - lastTempRead >= 3000) {
    lastTempRead = now;
    float tsensVal = 0.0f;
    tempValid = onchip_temp_get_celsius(&tsensVal);
    if (tempValid) {
      cachedTemp = tsensVal;
    }
  }
  if (tempLabel) {
    snprintf(buf, sizeof(buf), "%.1f C", cachedTemp);
    lv_label_set_text(tempLabel, tempValid ? buf : "Unavailable");
  }
}
