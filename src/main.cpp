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
#include "control/input_policy.h"
#include "event_log.h"
#include "mirror/usb_mirror.h"

#include <atomic>
#include "app_state.h"
#include "safety/safety.h"
#include "storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "esp_task_wdt.h"
#include <cstdint>
#include "onchip_temp.h"

// Cross-module externs now declared in their respective headers:
//   g_estopPending, etc.     -> safety.h
//   speed_get_pedal_enabled  -> speed.h

// ───────────────────────────────────────────────────────────────────────────────
// TASK HANDLES — for health monitoring (FIX-09)
// ───────────────────────────────────────────────────────────────────────────────
TaskHandle_t safetyHandle = nullptr;
TaskHandle_t motorHandle = nullptr;
TaskHandle_t controlHandle = nullptr;
TaskHandle_t lvglHandle = nullptr;
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

  // Show boot screen during initialization.
  // All lv_* calls wrapped in lvgl_lock/unlock per AGENTS.md, even in boot sequence:
  // storageTask starts soon after and also acquires the LVGL mutex.
  auto boot_step = [](uint8_t pct, const char* label, uint32_t delayMs) {
    lvgl_lock();
    screen_boot_update(pct, label);
    lv_timer_handler();
    lvgl_unlock();
    vTaskDelay(pdMS_TO_TICKS(delayMs));
  };

  lvgl_lock();
  screens_show(SCREEN_BOOT);
  screen_boot_update(10, "DISPLAY + LVGL");
  for (int i = 0; i < 5; i++) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  lvgl_unlock();

  boot_step(30, "TOUCH + STORAGE", 100);
  boot_step(50, "MOTOR DRIVER CHECK", 100);
  boot_step(70, "SAFETY INPUTS", 100);
  boot_step(90, "PEDAL + SPEED INPUT", 100);
  boot_step(100, "READY HANDOFF", 50);

  lvgl_lock();
  screens_show_startup();
  lvgl_unlock();
  safety_task_ready(8u);

  for (;;) {
    screens_process_pending();

    if (g_flashWriting.load(std::memory_order_acquire)) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    if (g_screenRedraw.exchange(false, std::memory_order_acq_rel)) {
      lvgl_lock();
      lv_obj_invalidate(lv_screen_active());
      lvgl_unlock();
    }

    lvgl_lock();
    uint32_t handlerStart = millis();
    // Return value = ms until next LVGL timer work (see LVGL integration docs); avoids fixed 10ms when idle.
    uint32_t next_lv_ms = lv_timer_handler();
    uint32_t handlerMs = millis() - handlerStart;
    if (handlerMs > 50) {
      LOG_W("LVGL handler took %lums", (unsigned long)handlerMs);
    }

    if (screens_get_current() == SCREEN_PROGRAM_EDIT) {
      screen_program_edit_poll_keyboard();
    }

    // Refresh snapshots well inside the 100ms freshness budget.
    static uint32_t lastScreenUpdate = 0;
    if (millis() - lastScreenUpdate >= 40) {
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
    // Screen-saver widgets and brightness belong to the LVGL task.
    dim_update();
    lvgl_unlock();

#if DEBUG_BUILD
    static uint32_t lastLvglStackLog = 0;
    if (millis() - lastLvglStackLog >= 30000) {
      lastLvglStackLog = millis();
      LOG_I("LVGL stack watermark: %u bytes free", uxTaskGetStackHighWaterMark(NULL));
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

// Input task (Core 0): GPIO pedal every 5ms, ADC every 20ms; never calls stepper API.
void inputTask(void* pvParameters) {
  LOG_I("Input task started on Core %d", xPortGetCoreID());
  safety_register_watchdog();
  // Arm the input dead-man supervisor before declaring this task ready:
  // heartbeat first, then the ready bit (safetyTask gates on both).
  g_inputHeartbeatMs.store(millis(), std::memory_order_release);
  g_inputHeartbeatValid.store(true, std::memory_order_release);
  safety_task_ready(2u);
  uint8_t adcCycle = 0;
  PedalInterlock pedal;
  bool pedalOwnsMotion = false;
  TickType_t t = xTaskGetTickCount();

#if DEBUG_BUILD
  int32_t motorLoopUs = 0;
  int32_t motorMaxUs = 0;
  int32_t motorMinUs = INT32_MAX;
  uint32_t motorJitterCount = 0;
  uint32_t lastJitterLog = 0;
#endif

  for (;;) {
    safety_feed_watchdog();
    // Dead-man heartbeat: safetyTask latches FAULT_INPUT_STALE without it
    // (a hung inputTask must not strand a pressed pedal / lost release).
    g_inputHeartbeatMs.store(millis(), std::memory_order_release);

#if DEBUG_BUILD
    int32_t loopStart = (int32_t)esp_timer_get_time();
#endif

    if (++adcCycle >= 4) {
      speed_update_adc();
      adcCycle = 0;
    }


    const bool safe = !safety_inhibit_motion() && control_get_state() != STATE_ESTOP;
    const PedalEdge edge =
        pedal.update(speed_get_pedal_enabled() && !control_setup_active(), safe, digitalRead(PIN_PEDAL_SW) == LOW, millis());
    if (edge == PedalEdge::Start && control_get_state() == STATE_IDLE) {
      pedalOwnsMotion = control_start_continuous();
    } else if (edge == PedalEdge::Stop && pedalOwnsMotion) {
      control_stop();  // Includes a START that has not reached controlTask yet.
      pedalOwnsMotion = false;
    }
    if (!safe) pedalOwnsMotion = false;

#if DEBUG_BUILD
    int32_t loopEnd = (int32_t)esp_timer_get_time();
    motorLoopUs = loopEnd - loopStart;
    if (motorLoopUs > motorMaxUs) motorMaxUs = motorLoopUs;
    if (motorLoopUs < motorMinUs) motorMinUs = motorLoopUs;
    motorJitterCount++;

    uint32_t now = millis();
    if (now - lastJitterLog >= 30000) {
      lastJitterLog = now;
      LOG_I("Motor jitter (5ms loop): avg=%ldus min=%ldus max=%ldus samples=%lu", motorLoopUs, motorMinUs,
            motorMaxUs, (unsigned long)motorJitterCount);
      motorMaxUs = 0;
      motorMinUs = INT32_MAX;
      motorJitterCount = 0;
    }
#endif


    vTaskDelayUntil(&t, pdMS_TO_TICKS(5));
  }
}

// Storage task (Core 1, priority 1) — program save/load + health monitoring
// NOT subscribed to WDT — does blocking I/O (NVS flash writes)
void storageTask(void* pvParameters) {
  LOG_I("Storage task started on Core %d", xPortGetCoreID());
  TickType_t t = xTaskGetTickCount();

  static uint32_t lastHealthCheck = 0;
  for (;;) {
    storage_flush();

    // Health monitoring every 30 seconds (FIX-09)
    if (millis() - lastHealthCheck >= 30000) {
      lastHealthCheck = millis();
#if DEBUG_BUILD
      LOG_I("─── Health ─────────────────────────────────");
      LOG_I("Stack free:  safety=%u  motor=%u  control=%u  lvgl=%u",
            uxTaskGetStackHighWaterMark(safetyHandle), uxTaskGetStackHighWaterMark(motorHandle),
            uxTaskGetStackHighWaterMark(controlHandle), uxTaskGetStackHighWaterMark(lvglHandle));
      LOG_I("Heap: %lu B free   PSRAM: %lu B free", ESP.getFreeHeap(), ESP.getFreePsram());
      LOG_I("Internal heap: largest=%u B minimum=%u B",
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
      if (uxTaskGetStackHighWaterMark(safetyHandle) < 256) LOG_E("SAFETY STACK LOW");
      if (uxTaskGetStackHighWaterMark(motorHandle) < 512) LOG_E("MOTOR STACK LOW");
      if (uxTaskGetStackHighWaterMark(lvglHandle) < 512) LOG_E("LVGL STACK LOW");
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
  digitalWrite(PIN_ENA, HIGH);  // MOTOR OFF — cannot move

  // Foot pedal switch: active LOW, INPUT_PULLUP. Configured early so a stuck
  // pedal cannot be read as "pressed" during motorTask init.
  pinMode(PIN_PEDAL_SW, INPUT_PULLUP);
  // ─────────────────────────────────────────────────────────────────────────

  Serial.begin(USB_MIRROR_SERIAL_BAUD);
  delay(100);

#if ENABLE_USB_UI_MIRROR
  usb_mirror_begin();
#endif

  LOG_I("BOOT OK — ENA=HIGH (motor disabled)");
  LOG_I("TIG Rotator Controller %s", FW_VERSION);
  LOG_I("Hardware: ESP32-P4 4.3\" Touch Display (Waveshare/Guition)");
  event_log_init();

  // Memory verification
  LOG_I("Flash: %lu MB", ESP.getFlashChipSize() / (1024 * 1024));
  LOG_I("PSRAM: %lu MB", ESP.getPsramSize() / (1024 * 1024));

  // Initialize safety system (ESTOP, watchdog)
  safety_init();

  // Load NVS settings BEFORE display/motor so saved brightness, acceleration, microstep, etc.
  // apply on first init (previously storage ran after motor_apply_settings and stepper kept defaults).
  storage_init();

  // Initialize display (MIPI-DSI + GT911 touch)
  display_init();

  onchip_temp_init();

  // motor_init() before speed_init() is required: motor_gpio_init() configures STEP/DIR/ESTOP/DIR_SW;
  // speed_init() reads PIN_DIR_SWITCH and uses the pot pin. Do not reorder without revisiting both.
  motor_init();

  acceleration_init();
  microstep_init();
  calibration_init();
  motor_apply_settings();
  safety_cache_stepper();
  motor_refresh_hz_cache();

  // Speed/pot + ADS1115 on display I2C bus (must run after display_init)
  speed_init();

  // Initialize LVGL
  lvgl_hal_init();

  // Initialize control state machine
  control_init();

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE FREERTOS TASKS
  // Priority: safety(5) > inputs(4) > control(3) > lvgl(2) > storage(1)
  // ESP32-P4 HP cores: Core 0 and Core 1 (RISC-V dual-core @ 360 MHz)
  // Task handles for health monitoring (FIX-09)
  // ─────────────────────────────────────────────────────────────────────────
  if (xTaskCreatePinnedToCore(safetyTask, "safety", 4096, nullptr, 5, &safetyHandle, 0) != pdPASS)
    fatal_halt("task allocation failed");
  if (xTaskCreatePinnedToCore(inputTask, "inputs", 5120, nullptr, 4, &motorHandle, 0) != pdPASS)
    fatal_halt("task allocation failed");
  if (xTaskCreatePinnedToCore(controlTask, "control", 4096, nullptr, 3, &controlHandle, 0) != pdPASS)
    fatal_halt("task allocation failed");
  if (xTaskCreatePinnedToCore(lvglTask, "lvgl", 65536, nullptr, 2, &lvglHandle, 1) != pdPASS)
    fatal_halt("task allocation failed");  // 64KB: LVGL 9 rotation + arc rendering needs large stack
  if (xTaskCreatePinnedToCore(storageTask, "storage", 12288, nullptr, 1, &storageHandle, 1) != pdPASS)
    fatal_halt("task allocation failed");
#if ENABLE_USB_UI_MIRROR
  if (xTaskCreatePinnedToCore(usbMirrorTask, "usbMirror", 8192, nullptr, 1, nullptr, 1) != pdPASS)
    fatal_halt("task allocation failed");
#endif

  LOG_I("All FreeRTOS tasks started");
  LOG_I("System ready — ESP32-P4 + MIPI-DSI display");
}

// ───────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ───────────────────────────────────────────────────────────────────────────────
void loop() { vTaskDelay(portMAX_DELAY); }
