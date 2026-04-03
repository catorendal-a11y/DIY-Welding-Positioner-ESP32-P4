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
#include "storage/storage.h"
#include "ble/ble.h"
extern bool wifiEnabled;
#include <esp_timer.h>
#include "WiFi.h"
#include "esp32-hal-hosted.h"
#include "esp_task_wdt.h"
#include "esp_cache.h"

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

  TickType_t t = xTaskGetTickCount();
  for (;;) {
    lv_timer_handler();

    dim_update();

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

    // Update ESTOP overlay if visible
    if (estop_overlay_visible()) {
      estop_overlay_update();
    }

    vTaskDelayUntil(&t, pdMS_TO_TICKS(10));
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

  // Initialize speed control (ADC)
  speed_init();

  // Initialize display (MIPI-DSI + GT911 touch)
  display_init();

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

  // Initialize WiFi via C6 co-processor (non-blocking)
  if (g_settings.wifi_enabled) {
    const char* ssid = g_settings.wifi_ssid[0] ? g_settings.wifi_ssid : WIFI_SSID;
    const char* pass = g_settings.wifi_pass[0] ? g_settings.wifi_pass : WIFI_PASS;
    LOG_I("Starting WiFi (non-blocking): %s", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    if (WiFi.status() == WL_CONNECTED) {
      LOG_I("WiFi already connected: %s", WiFi.localIP().toString().c_str());
      if (hostedHasUpdate()) {
        LOG_I("C6 co-processor firmware update available — auto-updating...");
        ble_ota_update_c6();
      }
    } else {
      LOG_I("WiFi connecting in background...");
    }
  } else {
    LOG_I("WiFi disabled (saved setting)");
  }

  // Initialize BLE (ESP-Hosted via C6 co-processor)
  ble_init();

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE FREERTOS TASKS
  // Priority: safety(5) > motor(4) > control(3) > lvgl(2) > storage(1)
  // ESP32-P4 HP cores: Core 0 and Core 1 (RISC-V dual-core @ 360 MHz)
  // Task handles for health monitoring (FIX-09)
  // ─────────────────────────────────────────────────────────────────────────
  xTaskCreatePinnedToCore(safetyTask,  "safety",  2048,  nullptr, 5, &safetyHandle,  0);
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
