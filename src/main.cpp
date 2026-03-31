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
#include "control/control.h"
#include "safety/safety.h"
#include "storage/storage.h"
#include "esp_task_wdt.h"
#include "esp_cache.h"

// ───────────────────────────────────────────────────────────────────────────────
// SAFETY GLOBALS (defined in safety.cpp)
// ───────────────────────────────────────────────────────────────────────────────
extern volatile bool g_estopPending;
extern volatile int64_t g_estopTriggerUs;  // microseconds, ISR-safe

// ───────────────────────────────────────────────────────────────────────────────
// EXTERNAL TASKS (defined in their respective modules)
// ───────────────────────────────────────────────────────────────────────────────
extern void safetyTask(void* pvParameters);
extern void controlTask(void* pvParameters);

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

  // Initialize screens after LVGL is ready
  screens_init();

  // Show boot screen during initialization
  screens_show(SCREEN_BOOT);
  screen_boot_update(10, "INITIALIZING HARDWARE...");

  // Give LVGL time to render boot screen
  for (int i = 0; i < 5; i++) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(50));  // Increased from 20 to 50
  }

  // Continue initialization with progress updates
  screen_boot_update(30, "LOADING CONFIGURATION...");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(500));  // Increased from 100 to 500

  screen_boot_update(50, "CHECKING MOTOR SYSTEMS...");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(700));  // Increased from 100 to 700

  screen_boot_update(80, "STARTING UI SYSTEM...");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(700));  // Increased from 100 to 700

  screen_boot_update(100, "READY");
  lv_timer_handler();
  vTaskDelay(pdMS_TO_TICKS(1000));  // Increased from 500 to 1000

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
  TickType_t t = xTaskGetTickCount();
  for (;;) {
    esp_task_wdt_reset();
    speed_apply();                   // every 5 ms
    if (++adcCycle >= 4) {           // every 20 ms (4 × 5 ms)
      speed_update_adc();            // IIR filter on pot ADC
      adcCycle = 0;
    }
    vTaskDelayUntil(&t, pdMS_TO_TICKS(5));
  }
}

// Storage task (Core 1, priority 1) — program save/load + health monitoring
void storageTask(void* pvParameters) {
  LOG_I("Storage task started on Core %d", xPortGetCoreID());
  esp_task_wdt_add(NULL);
  TickType_t t = xTaskGetTickCount();

  static uint32_t lastHealthCheck = 0;
  for (;;) {
    esp_task_wdt_reset();

    storage_flush();

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
  digitalWrite(PIN_ENA, HIGH);   // MOTOR OFF — cannot move
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

  // Cache stepper pointer for ESTOP ISR
  safety_cache_stepper();

  // Initialize storage (LittleFS + Presets API)
  storage_init();

  // Initialize control state machine
  control_init();

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE FREERTOS TASKS
  // Priority: safety(5) > motor(4) > control(3) > lvgl(1) = storage(1)
  // ESP32-P4 HP cores: Core 0 and Core 1 (RISC-V dual-core @ 360 MHz)
  // Task handles for health monitoring (FIX-09)
  // ─────────────────────────────────────────────────────────────────────────
  xTaskCreatePinnedToCore(safetyTask,  "safety",  2048,  nullptr, 5, &safetyHandle,  0);
  xTaskCreatePinnedToCore(motorTask,   "motor",   5120,  nullptr, 4, &motorHandle,   0);
  xTaskCreatePinnedToCore(controlTask, "control", 4096,  nullptr, 3, &controlHandle, 0);
  xTaskCreatePinnedToCore(lvglTask,    "lvgl",    65536, nullptr, 1, &lvglHandle,    1);  // 64KB: LVGL 9 rotation + arc rendering needs large stack
  xTaskCreatePinnedToCore(storageTask, "storage", 4096,  nullptr, 1, &storageHandle, 1);

  LOG_I("All FreeRTOS tasks started");
  LOG_I("System ready — ESP32-P4 + MIPI-DSI display");
}

// ───────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ───────────────────────────────────────────────────────────────────────────────
void loop() {
  // Nothing here — all work done in FreeRTOS tasks
  delay(100);
}
