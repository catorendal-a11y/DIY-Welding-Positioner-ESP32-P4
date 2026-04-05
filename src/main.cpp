// TIG Welding Rotator Controller - Main Entry Point
// Waveshare/Guition ESP32-P4 4.3" Touch Display Dev Board

#include <Arduino.h>
#include "config.h"
#include "ui/display.h"
#include "ui/lvgl_hal.h"
#include "ui/theme.h"
#include "ui/screens.h"
#include "motor/motor.h"
#include "motor/speed.h"
#include "motor/acceleration.h"
#include "motor/microstep.h"
#include "motor/calibration.h"
#include "control/control.h"

#include <atomic>

extern std::atomic<bool> motorConfigApplyPending;
#include "safety/safety.h"

volatile bool g_wakePending = false;
#include "storage/storage.h"
extern volatile bool wifiConnectPending;
extern char wifiPendingSsid[33];
extern char wifiPendingPass[65];
#include "ble/ble.h"
#include <esp_timer.h>
#include "WiFi.h"
#include "esp32-hal-hosted.h"
#include "esp_task_wdt.h"
#include "esp_cache.h"
#include <cstdint>
#include "onchip_temp.h"

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS (defined in safety.cpp)
// ───────────────────────────────────────────────────────────────────────────────
extern volatile bool g_estopPending;
extern volatile uint32_t g_estopTriggerMs;
extern bool speed_get_pedal_enabled();
extern bool speed_pedal_connected();

// ───────────────────────────────────────────────────────────────────────────────
// TASK HANDLES — for health monitoring (FIX-09)
// ───────────────────────────────────────────────────────────────────────────────
TaskHandle_t safetyHandle  = nullptr;
TaskHandle_t motorHandle   = nullptr;
TaskHandle_t controlHandle = nullptr;
TaskHandle_t lvglHandle    = nullptr;
TaskHandle_t storageHandle = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// FREERTOS TASKS
// ───────────────────────────────────────────────────────────────────────────────

// LVGL handler task (Core 1, priority 1) — also handles screen updates
void lvglTask(void* pvParameters) {
  LOG_I("LVGL task started on Core %d", xPortGetCoreID());

  // Initialize theme colors from settings before creating screens
  theme_init();

  // Initialize screens after LVGL is ready
  screens_init();

  // Show boot screen during initialization
  screens_show(SCREEN_BOOT);
  screen_boot_update(10, "INITIALIZING DISPLAY");

  for (int i = 0; i < 5; i++) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  screen_boot_update(30, "LOADING SETTINGS");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(100));

  screen_boot_update(50, "MOTOR SYSTEM");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(100));

  screen_boot_update(70, "SAFETY SYSTEM");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(100));

  screen_boot_update(90, "CONNECTIVITY");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(100));

  screen_boot_update(100, "READY");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(50));

  // Transition to main screen
  screens_show(SCREEN_MAIN);

  for (;;) {
    screens_process_pending();
    lvgl_lock();
    uint32_t handlerStart = millis();
    // Return value = ms until next LVGL timer work (see LVGL integration docs); avoids fixed 10ms when idle.
    uint32_t next_lv_ms = lv_timer_handler();
    uint32_t handlerMs = millis() - handlerStart;
    if (handlerMs > 50) {
      LOG_W("LVGL handler took %lums", (unsigned long)handlerMs);
    }

    // Update current screen (200ms interval for smooth UI)
    static uint32_t lastScreenUpdate = 0;
    if (millis() - lastScreenUpdate >= 200) {
      lastScreenUpdate = millis();
      screens_update_current();
    }

    // Check ESTOP state and show/hide overlay
    SystemState state = control_get_state();
    if (state == STATE_ESTOP && !estop_overlay_visible()) {
      estop_overlay_show();
    } else if (state != STATE_ESTOP && estop_overlay_visible()) {
      estop_overlay_hide();
    }

    if (estop_overlay_visible()) {
      estop_overlay_update();
    }
    lvgl_unlock();

    // Outside LVGL mutex: backlight dim uses GT911 read — must not nest with lv_indev touch read
    dim_update();

    #if DEBUG_BUILD
    static uint32_t lastLvglStackLog = 0;
    if (millis() - lastLvglStackLog >= 30000) {
      lastLvglStackLog = millis();
      LOG_I("LVGL stack watermark: %u bytes free", uxTaskGetStackHighWaterMark(NULL) * 4);
    }
    #endif

    // Sleep until next LVGL tick hint; cap so we still poll touch/input regularly.
    // After a heavy frame, add a short pause so other tasks/ISRs on this core get CPU (helps IWDT margins).
    uint32_t sleep_ms = 10;
    if (next_lv_ms != UINT32_MAX && next_lv_ms < sleep_ms) {
      sleep_ms = next_lv_ms ? next_lv_ms : 1u;
    }
    if (sleep_ms < 1u) sleep_ms = 1u;
    if (sleep_ms > 25u) sleep_ms = 25u;
    if (handlerMs > 40u) sleep_ms += 2u;

    vTaskDelay(pdMS_TO_TICKS(sleep_ms));
  }
}

// Motor control task (Core 0, priority 4) — updates speed every 5ms, ADC every 20ms
void motorTask(void* pvParameters) {
  LOG_I("Motor task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);
  uint8_t adcCycle = 0;
  static bool pedalSwWasPressed = false;
  TickType_t t = xTaskGetTickCount();

  #if DEBUG_BUILD
  int32_t motorLoopUs = 0;
  int32_t motorMaxUs = 0;
  int32_t motorMinUs = INT32_MAX;
  uint32_t motorJitterCount = 0;
  uint32_t lastJitterLog = 0;
  #endif

  for (;;) {
    esp_task_wdt_reset();

    #if DEBUG_BUILD
    int32_t loopStart = (int32_t)esp_timer_get_time();
    #endif

    speed_apply();
    if (++adcCycle >= 4) {
      speed_update_adc();
      adcCycle = 0;
    }

    if (acceleration_has_pending_apply()) {
      acceleration_clear_pending();
      motor_apply_settings();
    }

    if (motorConfigApplyPending) {
      motorConfigApplyPending = false;
      microstep_init();
      acceleration_init();
      motor_apply_settings();
    }

    if (speed_get_pedal_enabled() && speed_pedal_connected()) {
      bool swPressed = (digitalRead(PIN_PEDAL_SW) == LOW);
      if (swPressed && !pedalSwWasPressed) {
        if (control_get_state() == STATE_IDLE) control_start_continuous();
      } else if (!swPressed && pedalSwWasPressed) {
        if (control_get_state() == STATE_RUNNING) control_stop();
      }
      pedalSwWasPressed = swPressed;
    } else {
      pedalSwWasPressed = false;
    }

    #if DEBUG_BUILD
    int32_t loopEnd = (int32_t)esp_timer_get_time();
    motorLoopUs = loopEnd - loopStart;
    if (motorLoopUs > motorMaxUs) motorMaxUs = motorLoopUs;
    if (motorLoopUs < motorMinUs) motorMinUs = motorLoopUs;
    motorJitterCount++;

    uint32_t now = millis();
    if (now - lastJitterLog >= 30000) {
      lastJitterLog = now;
      LOG_I("Motor jitter (5ms loop): avg=%ldus min=%ldus max=%ldus samples=%lu",
            motorLoopUs, motorMinUs, motorMaxUs, (unsigned long)motorJitterCount);
      motorMaxUs = 0;
      motorMinUs = INT32_MAX;
      motorJitterCount = 0;
    }
    #endif

     vTaskDelayUntil(&t, pdMS_TO_TICKS(5));
  }
}

// Storage task (Core 1, priority 1) — program save/load + health monitoring
// NOT subscribed to WDT — does blocking I/O (LittleFS, WiFi SDIO, BLE SDIO)
void storageTask(void* pvParameters) {
  LOG_I("Storage task started on Core %d", xPortGetCoreID());
  TickType_t t = xTaskGetTickCount();

  static uint32_t lastHealthCheck = 0;
  for (;;) {
    storage_flush();

    wifi_process_pending();

    ble_update();

    // Health monitoring every 30 seconds (FIX-09)
    if (millis() - lastHealthCheck >= 30000) {
      lastHealthCheck = millis();
      #if DEBUG_BUILD
      LOG_I("─── Health ─────────────────────────────────");
      LOG_I("Stack free:  safety=%u  motor=%u  control=%u  lvgl=%u",
          uxTaskGetStackHighWaterMark(safetyHandle)  * 4,
          uxTaskGetStackHighWaterMark(motorHandle)   * 4,
          uxTaskGetStackHighWaterMark(controlHandle) * 4,
          uxTaskGetStackHighWaterMark(lvglHandle)    * 4);
      LOG_I("Heap: %lu B free   PSRAM: %lu B free",
          ESP.getFreeHeap(), ESP.getFreePsram());
      lv_mem_monitor_t m; lv_mem_monitor_core(&m);
      LOG_I("LVGL: %u%% heap used", (unsigned)m.used_pct);
      if (uxTaskGetStackHighWaterMark(safetyHandle)  * 4 < 256) LOG_E("SAFETY STACK LOW");
      if (uxTaskGetStackHighWaterMark(motorHandle)   * 4 < 512) LOG_E("MOTOR STACK LOW");
      if (uxTaskGetStackHighWaterMark(lvglHandle)    * 4 < 512) LOG_E("LVGL STACK LOW");
      if (m.used_pct > 80)                                      LOG_E("LVGL HEAP >80%%");
      LOG_I("────────────────────────────────────────────");
      #endif
    }

    vTaskDelayUntil(&t, pdMS_TO_TICKS(100));
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// SETUP - FIRST CODE EXECUTED
// ───────────────────────────────────────────────────────────────────────────────
void setup() {
  // ─────────────────────────────────────────────────────────────────────────
  // CRITICAL SAFETY: ENA MUST BE HIGH BEFORE ANYTHING ELSE
  // ─────────────────────────────────────────────────────────────────────────
  pinMode(PIN_ENA, OUTPUT);
  digitalWrite(PIN_ENA, HIGH);

  pinMode(PIN_PEDAL_SW, INPUT_PULLUP);   // MOTOR OFF — cannot move
  // ─────────────────────────────────────────────────────────────────────────

  Serial.begin(115200);
  delay(100);

  LOG_I("BOOT OK — ENA=HIGH (motor disabled)");
  LOG_I("TIG Rotator Controller v2.0");
  LOG_I("Hardware: ESP32-P4 4.3\" Touch Display (Waveshare/Guition)");

  // Memory verification
  LOG_I("Flash: %lu MB", ESP.getFlashChipSize() / (1024*1024));
  LOG_I("PSRAM: %lu MB", ESP.getPsramSize() / (1024*1024));

  // Initialize safety system (ESTOP, watchdog)
  safety_init();

  // Step/dir/ENA/ESTOP + DIR switch — before speed_init() (pot + digitalRead DIR_SW)
  motor_gpio_init();

  // Initialize display (MIPI-DSI + GT911 touch)
  display_init();

  onchip_temp_init();

  // Speed/pot + ADS1115 on display I2C bus (must run after display_init)
  speed_init();

  // Initialize LVGL
  lvgl_hal_init();

  // Initialize motor control (GPIO + FastAccelStepper)
  motor_init();

  // Validate and initialize motor sub-modules
  acceleration_init();
  microstep_init();
  calibration_init();

  // Apply validated settings to stepper
  motor_apply_settings();

  // Cache stepper pointer for ESTOP ISR
  safety_cache_stepper();

  // Initialize storage (LittleFS + Presets API)
  storage_init();

  // Initialize control state machine
  control_init();

  // WiFi init deferred to storageTask (ESP-Hosted SDIO must be single-owner)
  if (g_settings.wifi_enabled && g_settings.wifi_ssid[0] != '\0') {
    WiFi.mode(WIFI_STA);
    wifiConnectPending = true;
    strlcpy(wifiPendingSsid, g_settings.wifi_ssid, sizeof(wifiPendingSsid));
    strlcpy(wifiPendingPass, g_settings.wifi_pass, sizeof(wifiPendingPass));
    LOG_I("WiFi connect deferred to storageTask: %s", g_settings.wifi_ssid);
  } else {
    LOG_I("WiFi %s", g_settings.wifi_enabled ? "enabled (no SSID)" : "disabled (saved setting)");
  }

  // Initialize BLE (ESP-Hosted via C6 co-processor)
  ble_init();

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE FREERTOS TASKS
  // Priority: safety(5) > motor(4) > control(3) > lvgl(2) > storage(1)
  // ESP32-P4 HP cores: Core 0 and Core 1 (RISC-V dual-core @ 360 MHz)
  // Task handles for health monitoring (FIX-09)
  // ─────────────────────────────────────────────────────────────────────────
  xTaskCreatePinnedToCore(safetyTask,  "safety",  4096,  nullptr, 5, &safetyHandle,  0);
  xTaskCreatePinnedToCore(motorTask,   "motor",   5120,  nullptr, 4, &motorHandle,   0);
  xTaskCreatePinnedToCore(controlTask, "control", 4096,  nullptr, 3, &controlHandle, 0);
  xTaskCreatePinnedToCore(lvglTask,    "lvgl",    65536, nullptr, 2, &lvglHandle,    1);  // 64KB: LVGL 9 rotation + arc rendering needs large stack
  xTaskCreatePinnedToCore(storageTask, "storage", 12288, nullptr, 1, &storageHandle, 1);

  LOG_I("All FreeRTOS tasks started");
  LOG_I("System ready — ESP32-P4 + MIPI-DSI display");
}

// ───────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ───────────────────────────────────────────────────────────────────────────────
void loop() {
  delay(100);
}
