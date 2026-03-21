// TIG Rotator Controller - Display Implementation
// ESP32-P4: ST7701S 480×800 MIPI-DSI + GT911 touch (I2C)

#include "display.h"
#include <Arduino.h>
#include "../config.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char* TAG = "display";

// ───────────────────────────────────────────────────────────────────────────────
// GLOBAL HANDLES
// ───────────────────────────────────────────────────────────────────────────────
esp_lcd_panel_handle_t   display_panel = nullptr;
esp_lcd_touch_handle_t   display_touch = nullptr;

static esp_lcd_dsi_bus_handle_t     dsi_bus    = nullptr;
static esp_lcd_panel_io_handle_t    dsi_io     = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// BACKLIGHT CONTROL
// ───────────────────────────────────────────────────────────────────────────────
#define BL_GPIO         26    // Backlight GPIO (board-specific)
#define BL_LEDC_CH      LEDC_CHANNEL_0
#define BL_LEDC_TIMER   LEDC_TIMER_0

static void backlight_init() {
  ledc_timer_config_t timer_cfg = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num       = BL_LEDC_TIMER,
    .freq_hz         = 25000,
    .clk_cfg         = LEDC_AUTO_CLK,
  };
  ledc_timer_config(&timer_cfg);

  ledc_channel_config_t ch_cfg = {
    .gpio_num   = BL_GPIO,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = BL_LEDC_CH,
    .timer_sel  = BL_LEDC_TIMER,
    .duty       = 0,
    .hpoint     = 0,
  };
  ledc_channel_config(&ch_cfg);
}

void display_set_brightness(uint8_t brightness) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH, brightness);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CH);
}

// ───────────────────────────────────────────────────────────────────────────────
// TOUCH INITIALIZATION — GT911 via I2C
// ───────────────────────────────────────────────────────────────────────────────
static void touch_init() {
  // Configure I2C master bus
  i2c_master_bus_config_t i2c_bus_cfg = {
    .i2c_port   = I2C_NUM_0,
    .sda_io_num = (gpio_num_t)PIN_TOUCH_SDA,
    .scl_io_num = (gpio_num_t)PIN_TOUCH_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags = {
      .enable_internal_pullup = true,
    },
  };

  i2c_master_bus_handle_t i2c_bus = nullptr;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

  // Configure GT911 touch panel IO
  esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

  esp_lcd_panel_io_handle_t tp_io = nullptr;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));

  esp_lcd_touch_config_t touch_cfg = {
    .x_max          = DISPLAY_H_RES,
    .y_max          = DISPLAY_V_RES,
    .rst_gpio_num   = GPIO_NUM_NC,
    .int_gpio_num   = GPIO_NUM_NC,
    .levels = {
      .reset     = 0,
      .interrupt = 0,
    },
    .flags = {
      .swap_xy  = 1,   // Portrait → landscape
      .mirror_x = 0,
      .mirror_y = 0,
    },
  };

  ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io, &touch_cfg, &display_touch));
  LOG_I("GT911 touch init OK");
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY INITIALIZATION — MIPI-DSI + ST7701S
// ───────────────────────────────────────────────────────────────────────────────
void display_init() {
  LOG_I("Display init: ESP32-P4 MIPI-DSI ST7701S %dx%d", DISPLAY_H_RES, DISPLAY_V_RES);

  // ─────────────────────────────────────────────────────────────────────────
  // 1. BACKLIGHT — start dark, fade in after panel init
  // ─────────────────────────────────────────────────────────────────────────
  backlight_init();
  display_set_brightness(0);

  // ─────────────────────────────────────────────────────────────────────────
  // 2. MIPI-DSI BUS
  // ─────────────────────────────────────────────────────────────────────────
  esp_lcd_dsi_bus_config_t bus_cfg = {
    .bus_id             = 0,
    .num_data_lanes     = MIPI_DSI_LANE_NUM,
    .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
    .lane_bit_rate_mbps = 500,
  };
  ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus));

  // ─────────────────────────────────────────────────────────────────────────
  // 3. DBI PANEL IO (command interface over DSI)
  // ─────────────────────────────────────────────────────────────────────────
  esp_lcd_dbi_io_config_t dbi_cfg = {
    .virtual_channel = 0,
    .lcd_cmd_bits    = 8,
    .lcd_param_bits  = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &dsi_io));

  // ─────────────────────────────────────────────────────────────────────────
  // 4. ST7701S PANEL DRIVER
  // ─────────────────────────────────────────────────────────────────────────
  esp_lcd_dpi_panel_config_t dpi_cfg = {
    .virtual_channel    = 0,
    .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 26,
    .in_color_format    = LCD_COLOR_FMT_RGB565,
    .num_fbs            = 1,
    .video_timing = {
      .h_size            = DISPLAY_H_RES_NATIVE,   // 480 native portrait width
      .v_size            = DISPLAY_V_RES_NATIVE,   // 800 native portrait height
      .hsync_pulse_width = 10,
      .hsync_back_porch  = 10,
      .hsync_front_porch = 20,
      .vsync_pulse_width = 10,
      .vsync_back_porch  = 10,
      .vsync_front_porch = 10,
    },
  };

  st7701_vendor_config_t vendor_cfg = {
    .init_cmds      = nullptr,
    .init_cmds_size = 0,
    .mipi_config = {
      .dsi_bus    = dsi_bus,
      .dpi_config = &dpi_cfg,
    },
    .flags = {
      .use_mipi_interface = 1,
      .mirror_by_cmd      = 0,
      .auto_del_panel_io  = 0,
    },
  };

  esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = GPIO_NUM_NC,
    .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
    .vendor_config  = &vendor_cfg,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(dsi_io, &panel_cfg, &display_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(display_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(display_panel));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(display_panel, true));

  LOG_I("ST7701S MIPI-DSI panel init OK");

  // ─────────────────────────────────────────────────────────────────────────
  // 5. TOUCH — GT911
  // ─────────────────────────────────────────────────────────────────────────
  touch_init();

  // ─────────────────────────────────────────────────────────────────────────
  // 6. BACKLIGHT FADE-IN
  // ─────────────────────────────────────────────────────────────────────────
  delay(50);
  display_set_brightness(128);
  delay(30);
  display_set_brightness(255);

  LOG_I("Display init complete: %dx%d landscape, backlight on", DISPLAY_H_RES, DISPLAY_V_RES);
}
