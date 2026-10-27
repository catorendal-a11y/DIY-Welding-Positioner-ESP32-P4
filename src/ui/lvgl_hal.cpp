// TIG Rotator Controller - LVGL Hardware Abstraction Layer Implementation
// LVGL 9.x with internal SRAM strips, PPA rotation, MIPI-DSI DPI panel flush
// Color format: RGB565 (matching ST7701S and JC4880P433C BSP)
// 90° CCW rotation in flush callback (LVGL rotation crashes on ESP32-P4)

#include "lvgl_hal.h"
#include "rgb565_rotation.h"
#include <Arduino.h>
#include <atomic>
#include "../config.h"
#include "../app_state.h"
#include "../mirror/usb_mirror.h"
#include "display.h"
#include "screen_saver.h"
#include "../storage/storage.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "driver/ppa.h"


// ───────────────────────────────────────────────────────────────────────────────
// TICK TIMER — 1ms tick for LVGL animations and timers
// ───────────────────────────────────────────────────────────────────────────────
static esp_timer_handle_t lvgl_tick_timer;

static void IRAM_ATTR lvgl_tick_cb(void* arg) { lv_tick_inc(1); }

// ───────────────────────────────────────────────────────────────────────────────
// LVGL BUFFERS - internal SRAM, 1/10 frame draw and rotation strips
// The blocking flush does not benefit from a second LVGL draw buffer.
// ───────────────────────────────────────────────────────────────────────────────
static uint8_t* buf1 = nullptr;
static size_t buf_bytes = 0;

// The panel driver retains its PSRAM scanout framebuffers.
static uint16_t* rot_buf = nullptr;
static size_t rot_buf_pixels = 0;
static ppa_client_handle_t rotation_client = nullptr;

void lvgl_alloc_buffers() {
  buf_bytes = DISPLAY_H_RES * LVGL_BUF_LINES * sizeof(uint16_t);
  rot_buf_pixels = DISPLAY_H_RES * LVGL_BUF_LINES;

  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;
  buf1 = (uint8_t*)heap_caps_aligned_calloc(64, 1, buf_bytes, caps);
  rot_buf = (uint16_t*)heap_caps_aligned_calloc(64, 1, buf_bytes, caps);

  if (!buf1 || !rot_buf) {
    LOG_E("FATAL: LVGL buffer alloc failed!");
    if (buf1) heap_caps_free(buf1);
    if (rot_buf) heap_caps_free(rot_buf);
    buf1 = nullptr;
    rot_buf = nullptr;
    return;
  }
  LOG_I("LVGL buffers OK: %u bytes draw + %u bytes rotation (internal SRAM)",
        (unsigned)buf_bytes, (unsigned)buf_bytes);
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY FLUSH CALLBACK — 90° CCW rotation from 800x480 to 480x800
//
// LVGL renders at 800x480 (landscape). The physical panel is 480x800 (portrait).
// Rotation mapping: landscape (x, y) → portrait (y, 799-x)
//
// For a strip (x1,y1)→(x2,y2) with W=x2-x1+1, H=y2-y1+1:
//   Portrait rectangle: (y1, 799-x2) → (y2, 799-x1)
//   Portrait width = H, Portrait height = W
//   pixel rotation: rot_buf[(x2-x)*H + (y-y1)] = src_buf[(y-y1)*W + (x-x1)]
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  if (!display_panel || !px_map || !area || !rot_buf) {
    lv_display_flush_ready(disp);
    return;
  }

  if (g_flashWriting.load(std::memory_order_acquire)) {
    lv_display_flush_ready(disp);
    return;
  }

  int x1 = area->x1, y1 = area->y1;
  int x2 = area->x2, y2 = area->y2;

  // Clamping would change the source stride and corrupt the rotated strip.
  if (x1 < 0 || y1 < 0 || x2 >= DISPLAY_H_RES || y2 >= DISPLAY_V_RES) {
    LOG_E("LVGL flush outside logical resolution");
    lv_display_flush_ready(disp);
    return;
  }

  int W = x2 - x1 + 1;  // LVGL strip width (up to 800)
  int H = y2 - y1 + 1;

  if (W <= 0 || H <= 0) {
    lv_display_flush_ready(disp);
    return;
  }

  // Check rotation buffer fits
  if ((size_t)(W * H) > rot_buf_pixels) {
    LOG_E("Rotation buffer too small: need %d, have %u", W * H, (unsigned)rot_buf_pixels);
    lv_display_flush_ready(disp);
    return;
  }

  uint16_t* src = (uint16_t*)px_map;

#if ENABLE_USB_UI_MIRROR
  usb_mirror_enqueue_rect(area, px_map);
#endif

  bool rotated = false;
  if (rotation_client) {
    ppa_srm_oper_config_t operation{};
    operation.in.buffer = src;
    operation.in.pic_w = operation.in.block_w = W;
    operation.in.pic_h = operation.in.block_h = H;
    operation.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    operation.out.buffer = rot_buf;
    operation.out.buffer_size = buf_bytes;
    operation.out.pic_w = H;
    operation.out.pic_h = W;
    operation.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    operation.rotation_angle = PPA_SRM_ROTATION_ANGLE_90;
    operation.scale_x = operation.scale_y = 1.0f;
    // Wait before the CPU panel copy reads the PPA destination.
    operation.mode = PPA_TRANS_MODE_BLOCKING;
    const esp_err_t result = ppa_do_scale_rotate_mirror(rotation_client, &operation);
    rotated = result == ESP_OK;
    if (!rotated) {
      LOG_W("PPA rotation failed (%s); using CPU rotation", esp_err_to_name(result));
      ppa_unregister_client(rotation_client);
      rotation_client = nullptr;
    }
  }
  if (!rotated) rotate_rgb565_to_portrait(src, rot_buf, W, H);

  // Portrait rectangle coordinates
  int px1 = y1;        // portrait x start
  int py1 = DISPLAY_H_RES - 1 - x2;  // portrait y start
  int px2 = y2;        // portrait x end
  int py2 = DISPLAY_H_RES - 1 - x1;  // portrait y end

  // use_dma2d stays disabled: draw_bitmap copies synchronously before reuse.
  const esp_err_t result = esp_lcd_panel_draw_bitmap(display_panel, px1, py1, px2 + 1, py2 + 1, rot_buf);
  if (result != ESP_OK) LOG_E("Panel flush failed: %s", esp_err_to_name(result));
  lv_display_flush_ready(disp);
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH READ CALLBACK — Called by LVGL to poll GT911 touch input
// Touch is 480x800 portrait, but LVGL sees 800x480 landscape
// So we swap x/y to match the landscape orientation
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_touchpad_read_cb(lv_indev_t* indev_drv, lv_indev_data_t* data) {
  if (display_touch == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  if (esp_lcd_touch_read_data(display_touch) != ESP_OK) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  esp_lcd_touch_point_data_t point{};
  uint8_t touch_cnt = 0;
  const esp_err_t result = esp_lcd_touch_get_data(display_touch, &point, &touch_cnt, 1);
  if (result != ESP_OK) {
    // A failed read is not proof that the wake finger was lifted.
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  if (touch_cnt == 1) {
    uint16_t px = point.x;
    uint16_t py = point.y;

    int32_t lx = 799 - (int32_t)py;  // landscape x = 799 - portrait y
    int32_t ly = (int32_t)px;        // landscape y = portrait x
    if (lx < 0) lx = 0;
    if (lx > 799) lx = 799;
    if (ly < 0) ly = 0;
    if (ly > 479) ly = 479;
    data->point.x = (lv_coord_t)lx;
    data->point.y = (lv_coord_t)ly;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  static ScreenSaverInput input;
  screen_saver_filter_input(data, input);
}

// ───────────────────────────────────────────────────────────────────────────────
#if ENABLE_USB_UI_MIRROR
static void lvgl_usb_mirror_read_cb(lv_indev_t*, lv_indev_data_t* data) {
  usb_mirror_read_pointer(data);
  static ScreenSaverInput input;
  screen_saver_filter_input(data, input);
}
#endif

// LVGL INITIALIZATION
// ───────────────────────────────────────────────────────────────────────────────
void lvgl_hal_init() {
  LOG_I("LVGL HAL init starting...");

  lv_init();

  // ─────────────────────────────────────────────────────────────────────────
  // ALLOCATE INTERNAL SRAM STRIPS
  // ─────────────────────────────────────────────────────────────────────────
  lvgl_alloc_buffers();

  if (buf1 == nullptr || rot_buf == nullptr) {
    LOG_E("LVGL buffer allocation failed - cannot continue");
    fatal_halt("lvgl buffer allocation failed");
  }

  ppa_client_config_t rotation_config{};
  rotation_config.oper_type = PPA_OPERATION_SRM;
  const esp_err_t rotation_result = ppa_register_client(&rotation_config, &rotation_client);
  if (rotation_result != ESP_OK) {
    LOG_W("PPA unavailable (%s); using CPU rotation", esp_err_to_name(rotation_result));
  }

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER DISPLAY DRIVER (LVGL 9.x API)
  // Logical resolution is 800x480 landscape (NO LVGL rotation)
  // Flush callback handles rotation to 480x800 physical panel
  // ─────────────────────────────────────────────────────────────────────────
  lv_display_t* disp = lv_display_create(DISPLAY_H_RES, DISPLAY_V_RES);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, buf1, nullptr, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  // Keep LVGL in landscape; rotate each strip before the portrait panel copy.
  // lv_display_set_rotation() previously caused access faults on this board.

  display_register_lvgl_vsync(disp);

  LOG_I("LVGL display driver registered: %dx%d landscape, strip rotation to %dx%d physical, RGB565",
        DISPLAY_H_RES, DISPLAY_V_RES, DISPLAY_H_RES_NATIVE, DISPLAY_V_RES_NATIVE);

  // ─────────────────────────────────────────────────────────────────────────
  // REGISTER INPUT DEVICE (GT911 touch)
  // ─────────────────────────────────────────────────────────────────────────
  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touchpad_read_cb);

  LOG_I("LVGL touch input registered (GT911, manual coordinate swap)");

#if ENABLE_USB_UI_MIRROR
  lv_indev_t* mirrorIndev = lv_indev_create();
  lv_indev_set_type(mirrorIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(mirrorIndev, lvgl_usb_mirror_read_cb);
  LOG_I("LVGL USB mirror input registered");
#endif

  // ─────────────────────────────────────────────────────────────────────────
  // CREATE 1MS TICK TIMER
  // ─────────────────────────────────────────────────────────────────────────
  const esp_timer_create_args_t tick_timer_args = {.callback = &lvgl_tick_cb,
                                                   .arg = nullptr,
                                                   .dispatch_method = esp_timer_dispatch_t::ESP_TIMER_TASK,
                                                   .name = "lvgl_tick"};

  esp_timer_create(&tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, 1000);  // 1ms = 1000us

  LOG_I("LVGL tick timer started (1ms)");
  LOG_I("LVGL HAL init complete");
}
