// TIG Rotator Controller - Display Implementation
// ESP32-P4: ST7701S 480×800 MIPI-DSI
// Based on working RetroESP32-P4 configuration

#include "display.h"
#include <Arduino.h>
#include "../config.h"
#include "../storage/storage.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "lvgl.h"

// Forward declarations for LDO
extern "C" {
  typedef struct esp_ldo_channel_t *esp_ldo_channel_handle_t;
  typedef struct {
    int chan_id;
    int voltage_mv;
  } esp_ldo_channel_config_t;
  esp_err_t esp_ldo_acquire_channel(const esp_ldo_channel_config_t *config, esp_ldo_channel_handle_t *ret);
}

static const char* TAG = "display";

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH CONFIGURATION (GT911 - from JC4880P433C BSP)
// ───────────────────────────────────────────────────────────────────────────────
#define TOUCH_I2C_SDA    7
#define TOUCH_I2C_SCL    8
#define TOUCH_I2C_FREQ   400000

static i2c_master_bus_handle_t touch_i2c_bus = nullptr;
static esp_lcd_panel_io_handle_t touch_io = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// ST7701 VENDOR INITIALIZATION COMMANDS (from JC4880P433C BSP)
// Critical for proper panel initialization
// ───────────────────────────────────────────────────────────────────────────────
static const st7701_lcd_init_cmd_t st7701_lcd_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},
    {0xB0, (uint8_t[]){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t[]){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x58}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x29, (uint8_t[]){0x00}, 1, 20},
};

// ───────────────────────────────────────────────────────────────────────────────
// GLOBAL HANDLES
// ───────────────────────────────────────────────────────────────────────────────
esp_lcd_panel_handle_t   display_panel = nullptr;
esp_lcd_touch_handle_t   display_touch = nullptr;
void*                    display_framebuffer = nullptr;  // DPI framebuffer for direct writes

static esp_lcd_dsi_bus_handle_t     dsi_bus    = nullptr;
static esp_lcd_panel_io_handle_t    dsi_io     = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// BACKLIGHT CONTROL - RetroESP32-P4 working configuration
// ───────────────────────────────────────────────────────────────────────────────
#define BL_GPIO         23    // Backlight GPIO (JC4880P433)
#define LCD_RST         5     // Reset pin (JC4880P433)
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_TIMER   LEDC_TIMER_0
#define BL_LEDC_FREQ    5000
#define BL_MAX_DUTY     1023

static void backlight_init() {
  ledc_timer_config_t timer = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num       = BL_LEDC_TIMER,
    .freq_hz         = BL_LEDC_FREQ,
    .clk_cfg         = LEDC_AUTO_CLK,
  };
  ledc_timer_config(&timer);

  ledc_channel_config_t channel = {
    .gpio_num   = BL_GPIO,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = BL_LEDC_CHANNEL,
    .timer_sel  = BL_LEDC_TIMER,
    .duty       = 0,
    .hpoint     = 0,
  };
  ledc_channel_config(&channel);
}

void display_set_brightness(uint8_t brightness) {
  uint32_t duty = (uint32_t)brightness * BL_MAX_DUTY / 255;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY INITIALIZATION — MIPI-DSI + ST7701S
// ───────────────────────────────────────────────────────────────────────────────
void display_init() {
  LOG_I("Display init: ESP32-P4 MIPI-DSI ST7701S %dx%d", DISPLAY_H_RES, DISPLAY_V_RES);

  // ─────────────────────────────────────────────────────────────────────────
  // 1. BACKLIGHT
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [1/6] Backlight init...");
  backlight_init();
  display_set_brightness(0);
  LOG_I("       Backlight OK");

  // ─────────────────────────────────────────────────────────────────────────
  // 1.5. POWER ON MIPI DSI PHY
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [1.5/6] MIPI DSI PHY power on...");
  delay(100);
  esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
  esp_ldo_channel_config_t ldo_cfg = {
    .chan_id = 3,
    .voltage_mv = 2500,
  };
  esp_err_t ldo_ret = esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy);
  if (ldo_ret == ESP_OK) {
    LOG_I("       LDO PHY powered OK");
  } else {
    LOG_W("       LDO acquire failed: %s", esp_err_to_name(ldo_ret));
  }
  delay(100);

  // ─────────────────────────────────────────────────────────────────────────
  // 2. MIPI-DSI BUS
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [2/6] MIPI-DSI bus...");
  esp_lcd_dsi_bus_config_t bus_cfg = {
    .bus_id             = 0,
    .num_data_lanes     = 2,
    .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
    .lane_bit_rate_mbps = 500,
  };
  LOG_I("       LANES=%d, BITRATE=%d", (int)bus_cfg.num_data_lanes, (int)bus_cfg.lane_bit_rate_mbps);
  esp_err_t ret = esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus);
  LOG_I("       DSI ret = %d (%s)", ret, esp_err_to_name(ret));
  if (ret != ESP_OK) {
    LOG_E("       DSI bus failed: %s", esp_err_to_name(ret));
    return;
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 3. DBI PANEL IO
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [3/6] DBI panel IO...");
  esp_lcd_dbi_io_config_t dbi_cfg = {
    .virtual_channel = 0,
    .lcd_cmd_bits    = 8,
    .lcd_param_bits  = 8,
  };
  ret = esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &dsi_io);
  LOG_I("       DBI ret = %d (%s)", ret, esp_err_to_name(ret));
  if (ret != ESP_OK) {
    LOG_E("       DBI failed: %s", esp_err_to_name(ret));
    return;
  }
  LOG_I("       DBI OK");

  // ─────────────────────────────────────────────────────────────────────────
  // 4. ST7701S PANEL DRIVER
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [4/6] ST7701S panel...");

  // DPI timing for 480x800 @ 60Hz (physical panel is portrait)
  // LVGL handles 90° rotation to 800x480 landscape in software
  esp_lcd_dpi_panel_config_t dpi_cfg = {
    .virtual_channel    = 0,
    .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 34,
    .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB565,
    .num_fbs            = 2,
    .video_timing = {
      .h_size            = 480,
      .v_size            = 800,
      .hsync_pulse_width = 12,
      .hsync_back_porch  = 42,
      .hsync_front_porch = 42,
      .vsync_pulse_width = 2,
      .vsync_back_porch  = 8,
      .vsync_front_porch = 166,
    },
    .flags = { .use_dma2d = false }
  };

  st7701_vendor_config_t vendor_cfg = {
    .init_cmds = st7701_lcd_cmds,  // Use BSP custom init sequence
    .init_cmds_size = sizeof(st7701_lcd_cmds) / sizeof(st7701_lcd_cmds[0]),
    .mipi_config = {
      .dsi_bus    = dsi_bus,
      .dpi_config = &dpi_cfg,
    },
    .flags = {
      .use_mipi_interface = 1,
    }
  };

  esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = LCD_RST,
    .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,  // RGB565 = 16 bits (BSP uses this)
    .vendor_config  = &vendor_cfg,
  };

  ret = esp_lcd_new_panel_st7701(dsi_io, &panel_cfg, &display_panel);
  LOG_I("       Panel create ret = %d (%s)", ret, esp_err_to_name(ret));
  if (ret != ESP_OK) {
    LOG_E("       panel create failed: %s", esp_err_to_name(ret));
    return;
  }
  LOG_I("       panel create OK");

  ret = esp_lcd_panel_reset(display_panel);
  if (ret != ESP_OK) {
    LOG_E("       panel reset failed: %s", esp_err_to_name(ret));
  } else {
    LOG_I("       panel reset OK");
  }
  delay(200);

  LOG_I(">>> BEFORE panel_init");
  ret = esp_lcd_panel_init(display_panel);
  LOG_I(">>> AFTER panel_init: ret=%d (%s)", ret, esp_err_to_name(ret));
  if (ret != ESP_OK) {
    LOG_E("       panel init failed: %s", esp_err_to_name(ret));
  } else {
    LOG_I("       panel init OK");
  }

  // NOTE: NOT calling get_framebuffer() - using draw_bitmap mode instead
  // get_framebuffer may switch DPI to framebuffer mode which causes issues
  display_framebuffer = nullptr;  // Not used in draw_bitmap mode

  // Clear screen to black BEFORE turning on backlight
  // esp_lcd_panel_draw_bitmap takes (x_start, y_start, x_end, y_end, data)
  LOG_I(">>> Clear screen to black...");
  uint16_t *black_buf = (uint16_t *)heap_caps_malloc(480 * 100 * 2, MALLOC_CAP_DMA);  // RGB565
  if (black_buf) {
    memset(black_buf, 0, 480 * 100 * 2);  // All zeros = black
    for (int y = 0; y < 800; y += 100) {
      esp_lcd_panel_draw_bitmap(display_panel, 0, y, 480, y + 100, black_buf);
      delay(5);  // Let DMA complete before next draw
    }
    heap_caps_free(black_buf);
  }

  ret = esp_lcd_panel_disp_on_off(display_panel, true);
  if (ret != ESP_OK) {
    LOG_E("       panel display ON failed: %s", esp_err_to_name(ret));
  } else {
    LOG_I("       panel display ON OK");
  }

  LOG_I("ST7701S MIPI-DSI init complete!");

  // ─────────────────────────────────────────────────────────────────────────
  // 5. TOUCH — GT911 via I2C
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [5/6] GT911 touch...");

  // Initialize I2C master bus (matching BSP exactly)
  i2c_master_bus_config_t i2c_bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = (gpio_num_t)TOUCH_I2C_SDA,
    .scl_io_num = (gpio_num_t)TOUCH_I2C_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
      .enable_internal_pullup = 0,  // External pull-ups present on board
    },
  };
  esp_err_t touch_ret = i2c_new_master_bus(&i2c_bus_cfg, &touch_i2c_bus);
  if (touch_ret != ESP_OK) {
    LOG_W("       I2C bus init failed: %s", esp_err_to_name(touch_ret));
  } else {
    LOG_I("       I2C bus OK");

    // Initialize GT911 touch IO
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_cfg.scl_speed_hz = TOUCH_I2C_FREQ;
    touch_ret = esp_lcd_new_panel_io_i2c(touch_i2c_bus, &tp_io_cfg, &touch_io);
    if (touch_ret != ESP_OK) {
      LOG_W("       Touch IO init failed: %s", esp_err_to_name(touch_ret));
    } else {
      // Initialize GT911 touch controller
      // Physical touch is 480x800, no swap — LVGL callback handles rotation
      esp_lcd_touch_config_t tp_cfg = {
        .x_max = 480,
        .y_max = 800,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
          .reset = 0,
          .interrupt = 0,
        },
        .flags = {
          .swap_xy = 0,  // No swap — callback handles coordinate transform
          .mirror_x = 0,
          .mirror_y = 0,
        },
      };
      touch_ret = esp_lcd_touch_new_i2c_gt911(touch_io, &tp_cfg, &display_touch);
      if (touch_ret != ESP_OK) {
        LOG_W("       GT911 init failed: %s", esp_err_to_name(touch_ret));
      } else {
        LOG_I("       GT911 touch OK!");
      }
    }
  }

  // ─────────────────────────────────────────────────────────────────────────
  // 6. BACKLIGHT ON
  // ─────────────────────────────────────────────────────────────────────────
  LOG_I("  [6/6] Backlight ON...");
  delay(100);
  display_set_brightness(g_settings.brightness);

  LOG_I("Display init complete: %dx%d", DISPLAY_H_RES, DISPLAY_V_RES);
}

// ───────────────────────────────────────────────────────────────────────────────
// VSYNC CALLBACK — REMOVED (was obsolete in LVGL 9)
// In LVGL 9, flush_ready is called at the end of lvgl_flush_cb after the
// synchronous memcpy to the DPI buffer. Registering a vsync callback that
// calls flush_ready asynchronously (60×/s) corrupts LVGL's buffer state
// machine and causes Load access faults on ESP32-P4.
// ───────────────────────────────────────────────────────────────────────────────
void display_register_lvgl_vsync(void *user_ctx) {
  // No-op: vsync callback was removed. flush_ready is handled in lvgl_flush_cb.
  LOG_I("Vsync callback skipped (LVGL 9 handles flush in lvgl_flush_cb)");
}

// ───────────────────────────────────────────────────────────────────────────────
// ASYNC CALLBACK NOT NEEDED
// We use direct framebuffer writes (see lvgl_hal.cpp), which bypass the
// async DMA2D pipeline that causes "previous draw operation" errors.
// ───────────────────────────────────────────────────────────────────────────────
