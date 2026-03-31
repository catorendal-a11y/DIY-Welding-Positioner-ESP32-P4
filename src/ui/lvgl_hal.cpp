// TIG Rotator Controller - LVGL Hardware Abstraction Layer Implementation
// LVGL 9.x with PSRAM buffers, MIPI-DSI DPI panel flush
// Color format: RGB565 (matching ST7701S and JC4880P433C BSP)
// Manual 90° CW rotation in flush callback (LVGL rotation crashes on ESP32-P4)

#include "lvgl_hal.h"
#include <Arduino.h>
#include "../config.h"
#include "display.h"
#include "../storage/storage.h"

#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

static uint32_t lastActivityMs = 0;
static bool isDimmed = false;
static uint8_t dimBrightness = 15;

void dim_reset_activity() {
  lastActivityMs = millis();
  if (isDimmed) {
    isDimmed = false;
    display_set_brightness(g_settings.brightness);
  }
}

void dim_update() {
  if (g_settings.dim_timeout == 0) return;
  if (isDimmed) return;
  if (millis() - lastActivityMs > (uint32_t)g_settings.dim_timeout * 1000) {
    isDimmed = true;
    display_set_brightness(dimBrightness);
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// TICK TIMER — 1ms tick for LVGL animations and timers
// ───────────────────────────────────────────────────────────────────────────────
static esp_timer_handle_t lvgl_tick_timer;

static void IRAM_ATTR lvgl_tick_cb(void* arg) {
  lv_tick_inc(1);
}

// ───────────────────────────────────────────────────────────────────────────────
// LVGL BUFFERS - PSRAM
// LVGL renders at 800x480 landscape, flush callback rotates to 480x800 portrait
// ───────────────────────────────────────────────────────────────────────────────
static uint8_t *buf1 = nullptr;
static uint8_t *buf2 = nullptr;
static size_t buf_bytes = 0;

// Pre-allocated rotation scratch buffer (PSRAM)
// Max strip: 800 × 80 lines = 64000 pixels × 2 bytes = 128000 bytes
static uint16_t *rot_buf = nullptr;
static size_t rot_buf_pixels = 0;

void lvgl_alloc_buffers() {
  // Buffer for LVGL rendering: 800 × 80 lines × 2 bytes = 128000 bytes
  buf_bytes = DISPLAY_H_RES * LVGL_BUF_LINES * 2;

  // Rotation scratch buffer: same max size as a strip
  rot_buf_pixels = DISPLAY_H_RES * LVGL_BUF_LINES; // 800 * 80 = 64000 pixels

  // Use PSRAM (32MB available) — DMA2D is disabled so PSRAM is safe
  buf1 = (uint8_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
  buf2 = (uint8_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
  rot_buf = (uint16_t*)heap_caps_malloc(rot_buf_pixels * 2, MALLOC_CAP_SPIRAM);

  if (!buf1 || !buf2 || !rot_buf) {
    LOG_E("FATAL: LVGL buffer alloc failed!");
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    if (rot_buf) heap_caps_free(rot_buf);
    buf1 = nullptr;
    rot_buf = nullptr;
    return;
  }
  LOG_I("LVGL buffers OK: 2x%zu bytes draw + %zu bytes rotation (PSRAM)", buf_bytes, rot_buf_pixels * 2);
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY FLUSH CALLBACK — Manual 90° CW rotation from 800x480 to 480x800
//
// LVGL renders at 800x480 (landscape). The physical panel is 480x800 (portrait).
// Rotation mapping: landscape (x, y) → portrait (y, 799-x)
//
// For a strip (x1,y1)→(x2,y2) with W=x2-x1+1, H=y2-y1+1:
//   Portrait rectangle: (y1, 799-x2) → (y2, 799-x1)
//   Portrait width = H, Portrait height = W
//   pixel rotation: rot_buf[(x2-x)*H + (y-y1)] = src_buf[(y-y1)*W + (x-x1)]
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  if (!display_panel || !px_map || !area || !rot_buf) {
    lv_display_flush_ready(disp);
    return;
  }

  int x1 = area->x1, y1 = area->y1;
  int x2 = area->x2, y2 = area->y2;

  // Clamp to logical resolution
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 >= DISPLAY_H_RES) x2 = DISPLAY_H_RES - 1;
  if (y2 >= DISPLAY_V_RES) y2 = DISPLAY_V_RES - 1;

  int W = x2 - x1 + 1;  // LVGL strip width (up to 800)
  int H = y2 - y1 + 1;  // LVGL strip height (up to 80)

  if (W <= 0 || H <= 0) {
    lv_display_flush_ready(disp);
    return;
  }

  // Check rotation buffer fits
  if ((size_t)(W * H) > rot_buf_pixels) {
    LOG_E("Rotation buffer too small: need %d, have %zu", W * H, rot_buf_pixels);
    lv_display_flush_ready(disp);
    return;
  }

  uint16_t *src = (uint16_t*)px_map;

  // 90° CW rotation: landscape (x, y) → portrait (y, 799-x)
  for (int r = 0; r < H; r++) {
    int ly = y1 + r;  // landscape y
    for (int c = 0; c < W; c++) {
      int lx = x1 + c;  // landscape x
      // Destination in portrait buffer:
      //   portrait_x = ly, portrait_y = 799 - lx
      //   In rotated strip buffer (row-major, portrait area):
      //     row = portrait_y - (799-x2) = x2 - lx
      //     col = portrait_x - y1 = ly - y1 = r
      rot_buf[(x2 - lx) * H + r] = src[r * W + c];
    }
  }

  // Portrait rectangle coordinates
  int px1 = y1;          // portrait x start
  int py1 = 799 - x2;    // portrait y start
  int px2 = y2;          // portrait x end
  int py2 = 799 - x1;    // portrait y end

  esp_lcd_panel_draw_bitmap(display_panel, px1, py1, px2 + 1, py2 + 1, rot_buf);
  lv_display_flush_ready(disp);
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH READ CALLBACK — Called by LVGL to poll GT911 touch input
// Touch is 480x800 portrait, but LVGL sees 800x480 landscape
// So we swap x/y to match the landscape orientation
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_touchpad_read_cb(lv_indev_t *indev_drv, lv_indev_data_t *data) {
  if (display_touch == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  esp_lcd_touch_read_data(display_touch);

  uint16_t touch_x[1];
  uint16_t touch_y[1];
  uint16_t touch_strength[1];
  uint8_t  touch_cnt = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  bool touched = esp_lcd_touch_get_coordinates(display_touch,
                    touch_x, touch_y, touch_strength, &touch_cnt, 1);
#pragma GCC diagnostic pop

  if (touched && touch_cnt > 0) {
    uint16_t px = touch_x[0];
    uint16_t py = touch_y[0];

    data->point.x = 799 - py;      // landscape x = 799 - portrait y
    data->point.y = px;             // landscape y = portrait x
    data->state = LV_INDEV_STATE_PRESSED;
    dim_reset_activity();
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// LVGL INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_hal_init() {
  LOG_I("LVGL HAL init starting...");
  lastActivityMs = millis();

  lv_init();

  // ─────────────────────────────────────────────────────────────────────────
  // ALLOCATE PSRAM BUFFERS
  // ─────────────────────────────────────────────────────────────────────────
  lvgl_alloc_buffers();

  if (buf1 == nullptr || buf2 == nullptr || rot_buf == nullptr) {
    LOG_E("LVGL buffer allocation failed - cannot continue");
    return;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER DISPLAY DRIVER (LVGL 9.x API)
  // Logical resolution is 800x480 landscape (NO LVGL rotation)
  // Flush callback handles rotation to 480x800 physical panel
  // ─────────────────────────────────────────────────────────────────────────
  lv_display_t *disp = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  // NO lv_display_set_rotation — manual rotation in flush callback

  display_register_lvgl_vsync(disp);

  LOG_I("LVGL display driver registered: %dx%d landscape, manual rotation to %dx%d physical, RGB565",
        DISPLAY_H_RES, DISPLAY_V_RES, DISPLAY_H_RES_NATIVE, DISPLAY_V_RES_NATIVE);

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER INPUT DEVICE (GT911 touch)
  // ─────────────────────────────────────────────────────────────────────────
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read_cb);

  LOG_I("LVGL touch input registered (GT911, manual coordinate swap)");

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
