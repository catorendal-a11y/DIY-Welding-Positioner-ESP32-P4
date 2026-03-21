// TIG Rotator Controller - LVGL Hardware Abstraction Layer Implementation
// LVGL 8.x with PSRAM buffers, ESP-IDF MIPI-DSI DPI flush, GT911 touch

#include "lvgl_hal.h"
#include <Arduino.h>
#include "../config.h"
#include "display.h"

#include <esp_timer.h>
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
// DISPLAY FLUSH CALLBACK — Called by LVGL to render to screen via MIPI-DSI DPI
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  int x1 = area->x1;
  int y1 = area->y1;
  int x2 = area->x2;
  int y2 = area->y2;

  // Push pixels to DPI panel via esp_lcd_panel_draw_bitmap
  esp_lcd_panel_draw_bitmap(display_panel, x1, y1, x2 + 1, y2 + 1, color_p);

  // Tell LVGL we're done with this buffer
  lv_disp_flush_ready(disp);
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH READ CALLBACK — Called by LVGL to poll GT911 touch input
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_touchpad_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  uint16_t touch_x[1];
  uint16_t touch_y[1];
  uint16_t touch_strength[1];
  uint8_t  touch_cnt = 0;

  // Read touch data from GT911
  esp_lcd_touch_read_data(display_touch);
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
  // ALLOCATE DISPLAY BUFFERS IN PSRAM
  // ESP32-P4 has 32MB PSRAM — plenty of room for double-buffered 800×80
  // ─────────────────────────────────────────────────────────────────────────
  size_t buf_size = DISPLAY_H_RES * LVGL_BUFFER_HEIGHT * sizeof(lv_color_t);

  lv_color_t* buf1 = (lv_color_t*)ps_malloc(buf_size);
  lv_color_t* buf2 = (lv_color_t*)ps_malloc(buf_size);

  static lv_disp_draw_buf_t draw_buf;

  if (!buf1 || !buf2) {
    // Graceful fallback to internal SRAM if PSRAM fails
    LOG_E("PSRAM alloc failed — falling back to internal SRAM");
    free(buf1); free(buf2);

    // Fallback: 800×10 buffers in internal SRAM (16 KB each)
    static lv_color_t fb1[800 * 10];
    static lv_color_t fb2[800 * 10];
    lv_disp_draw_buf_init(&draw_buf, fb1, fb2, 800 * 10);
    LOG_W("LVGL using SRAM fallback buffers (800×10)");
  } else {
    // PSRAM buffers available — use full size
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISPLAY_H_RES * LVGL_BUFFER_HEIGHT);
    LOG_I("LVGL PSRAM buffers OK: 2 x %zu KB in PSRAM", buf_size / 1024);
  }

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER DISPLAY DRIVER (LVGL 8.x API)
  // ─────────────────────────────────────────────────────────────────────────
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DISPLAY_H_RES;    // 800
  disp_drv.ver_res = DISPLAY_V_RES;    // 480
  disp_drv.flush_cb = lvgl_flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Set default screen background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0B1929), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

  LOG_I("LVGL display driver registered: %dx%d", DISPLAY_H_RES, DISPLAY_V_RES);

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
