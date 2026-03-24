// TIG Rotator Controller - LVGL Hardware Abstraction Layer Implementation
// LVGL 8.x with PSRAM buffers, MIPI-DSI DPI panel flush
// Color format: RGB565 (matching ST7701S and JC4880P433C BSP)

#include "lvgl_hal.h"
#include <Arduino.h>
#include "../config.h"
#include "display.h"

#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

// ───────────────────────────────────────────────────────────────────────────────
// TICK TIMER — 1ms tick for LVGL animations and timers
// ───────────────────────────────────────────────────────────────────────────────
static esp_timer_handle_t lvgl_tick_timer;

static void IRAM_ATTR lvgl_tick_cb(void* arg) {
  lv_tick_inc(1);
}

// ───────────────────────────────────────────────────────────────────────────────
// LVGL BUFFERS - PSRAM (NOT DMA - draw_bitmap handles DMA internally)
// ───────────────────────────────────────────────────────────────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

void lvgl_alloc_buffers() {
  // LVGL_COLOR_DEPTH=16: lv_color_t is 2 bytes (RGB565)
  // Buffer size: 480 × 80 lines × 2 bytes = 76800 bytes per buffer
  // IMPORTANT: Use internal DMA-capable RAM for DMA2D compatibility!
  // PSRAM causes esp_cache_msync crash with DMA2D enabled
  size_t buf_bytes = DISPLAY_H_RES_NATIVE * LVGL_BUF_LINES * sizeof(lv_color_t);

  // Use internal DMA-capable RAM (required for DMA2D)
  buf1 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  buf2 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  // Fallback: try without DMA flag if allocation fails
  if (!buf1 || !buf2) {
    LOG_W("DMA alloc failed, trying internal RAM without DMA flag...");
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);

    buf1 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_INTERNAL);

    if (!buf1 || !buf2) {
      LOG_E("FATAL: Cannot allocate LVGL buffers!");
      if (buf1) heap_caps_free(buf1);
      buf1 = nullptr;
      return;
    }
    LOG_W("Using internal RAM (non-DMA) - may have issues with DMA2D");
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_H_RES_NATIVE * LVGL_BUF_LINES);
  LOG_I("LVGL buffers OK: %zu bytes each, internal DMA-RAM, RGB565, no DMA2D", buf_bytes);
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY FLUSH CALLBACK — RGB565 direct pass-through (no conversion needed!)
// CRITICAL: lv_disp_flush_ready() is called by vsync callback, NOT here!
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  // Safety check - prevent crash on null buffer or display
  if (!display_panel || !color_p || !area) {
    lv_disp_flush_ready(disp);
    return;
  }

  int x1 = area->x1;
  int y1 = area->y1;
  int x2 = area->x2;
  int y2 = area->y2;

  // RGB565 direct pass-through - no conversion needed!
  // LVGL RGB565 (2 bytes/pixel) matches MIPI-DPI RGB565 format
  esp_lcd_panel_draw_bitmap(display_panel, x1, y1, x2 + 1, y2 + 1, (uint16_t*)color_p);

  // CRITICAL: Do NOT call lv_disp_flush_ready() here!
  // Vsync callback will call it when hardware completes frame transfer
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH READ CALLBACK — Called by LVGL to poll GT911 touch input
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  // Safety check - don't crash if touch failed to init
  if (display_touch == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  // Read touch data from GT911
  esp_lcd_touch_read_data(display_touch);

  uint16_t touch_x[1];
  uint16_t touch_y[1];
  uint16_t touch_strength[1];
  uint8_t  touch_cnt = 0;

  bool touched = esp_lcd_touch_get_coordinates(display_touch,
                   touch_x, touch_y, touch_strength, &touch_cnt, 1);

  if (touched && touch_cnt > 0) {
    data->point.x = touch_x[0];
    data->point.y = touch_y[0];
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// LVGL INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_hal_init() {
  LOG_I("LVGL HAL init starting...");

  // Initialize LVGL core
  lv_init();

  // ─────────────────────────────────────────────────────────────────────────
  // ALLOCATE PSRAM BUFFERS
  // ─────────────────────────────────────────────────────────────────────────
  lvgl_alloc_buffers();

  if (buf1 == nullptr || buf2 == nullptr) {
    LOG_E("LVGL buffer allocation failed - cannot continue");
    return;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER DISPLAY DRIVER (LVGL 8.x API)
  // ─────────────────────────────────────────────────────────────────────────
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  // Native portrait resolution - LVGL will rotate for landscape
  disp_drv.hor_res = DISPLAY_H_RES_NATIVE;  // 480
  disp_drv.ver_res = DISPLAY_V_RES_NATIVE;  // 800
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 0;  // Partial refresh
  disp_drv.sw_rotate = 1;  // Enable software rotation for landscape
  lv_disp_drv_register(&disp_drv);

  // Set 90° rotation for landscape (800×480)
  lv_disp_t *display = lv_disp_get_next(NULL);
  lv_disp_set_rotation(display, LV_DISP_ROT_90);

  // CRITICAL: Register vsync callback for MIPI DSI Video Mode
  // lv_disp_flush_ready() will be called by hardware when frame transfer completes
  display_register_lvgl_vsync(&disp_drv);

  LOG_I("LVGL display driver registered: %dx%d → 90° rotation → %dx%d landscape, RGB565",
        DISPLAY_H_RES_NATIVE, DISPLAY_V_RES_NATIVE, DISPLAY_V_RES_NATIVE, DISPLAY_H_RES_NATIVE);

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER INPUT DEVICE (GT911 touch via LVGL 8.x API)
  // ─────────────────────────────────────────────────────────────────────────
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touchpad_read_cb;
  lv_indev_drv_register(&indev_drv);

  LOG_I("LVGL touch input registered (GT911)");

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE 1MS TICK TIMER
  // ─────────────────────────────────────────────────────────────────────────
  const esp_timer_create_args_t tick_timer_args = {
    .callback = &lvgl_tick_cb,
    .arg = nullptr,
    .dispatch_method = esp_timer_dispatch_t::ESP_TIMER_TASK,
    .name = "lvgl_tick"
  };

  esp_timer_create(&tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, 1000);  // 1ms = 1000us

  LOG_I("LVGL tick timer started (1ms)");
  LOG_I("LVGL HAL init complete");
}
