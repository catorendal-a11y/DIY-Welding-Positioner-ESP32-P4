#pragma once
// TIG Welding Rotator Controller - Hardware Configuration
// Waveshare/Guition ESP32-P4 4.3" Touch Display Dev Board

#define FW_VERSION "v1.3.0"

// ───────────────────────────────────────────────────────────────────────────────
// GPIO HEADER PINS (2×13 pin header)
// Available GPIOs: 28–35, 49–52 (see board pin diagram)
// ───────────────────────────────────────────────────────────────────────────────
#define PIN_POT         49   // ADC — Potentiometer speed input
                              // NOTE: GPIO 49 ADC channel must be verified against
                              // ESP32-P4 datasheet. High GPIOs (47-54) may be reserved.
#define PIN_STEP        50   // Step pulse output (FastAccelStepper RMT)
#define PIN_DIR         51   // Direction: CW=HIGH, CCW=LOW
#define PIN_ENA         52   // Enable: Active LOW to TB6600
#define PIN_ESTOP       34   // Emergency Stop: Active LOW, NC contact
#define PIN_DIR_SWITCH  28   // CW/CCW direction switch (INPUT_PULLUP, LOW=CCW, HIGH=CW)
#define PIN_SPARE       35   // Reserved for future encoder/expansion

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY & TOUCH — MIPI-DSI (handled by ESP-IDF native drivers)
// ───────────────────────────────────────────────────────────────────────────────
// Display: MIPI-DSI panel via ESP-IDF esp_lcd driver (board-specific)
//          Waveshare/Guition 4.3" uses ST7701S-class controller.
//          Do NOT use LovyanGFX — it does not support MIPI-DSI on ESP32-P4.
// Touch: GT911 capacitive, I2C — connected via board's I2C bus
// Backlight: controlled by dedicated GPIO on the board
//
// I2C for touch (directly on ESP32-P4 I2C pins from header)
#define PIN_TOUCH_SDA    7   // I2C SDA (board internal I2C bus)
#define PIN_TOUCH_SCL    8   // I2C SCL (board internal I2C bus)
#define TOUCH_ADDR_GT911 0x5D // GT911 default I2C address

// MIPI-DSI lane configuration (ESP32-P4 hardware)
#define MIPI_DSI_LANE_NUM       2    // 2-lane MIPI-DSI
#define MIPI_DSI_LANE_BITRATE   (1000 * 1000 * 1000)  // 1 Gbps per lane

// Display resolution
// Physical panel: 480x800 portrait
// Logical (LVGL): 800x480 landscape (manual rotation in flush callback)
#define DISPLAY_H_RES   800   // Logical landscape width
#define DISPLAY_V_RES   480   // Logical landscape height
#define DISPLAY_H_RES_NATIVE 480  // Physical panel width (portrait)
#define DISPLAY_V_RES_NATIVE 800  // Physical panel height (portrait)

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR & MECHANICAL PARAMETERS
// ───────────────────────────────────────────────────────────────────────────────
#define MIN_RPM         0.1f      // Minimum workpiece RPM
#define MAX_RPM         3.0f      // Maximum workpiece RPM

#define MICROSTEPS      8         // 1/8 microstepping (TB6600 DIP)
#define STEPS_PER_REV   (200 * MICROSTEPS)   // 1600 steps/rev motor

// GEAR & ROLLER SYSTEM
#define GEAR_RATIO      108.0f    // 1:108 worm gear
#define D_EMNE          0.300f    // Workpiece diameter: 300mm
#define D_RULLE         0.080f    // Roller diameter: 80mm

// SPEED CHARACTERISTICS
#define START_SPEED     80        // Hz — low start for smooth 0.1 RPM
#define ACCELERATION    7000      // steps/s²

// ───────────────────────────────────────────────────────────────────────────────
// BUILD CONFIGURATION
// ───────────────────────────────────────────────────────────────────────────────
// DEBUG_BUILD is set by platformio.ini build flags:
//   env:esp32p4-debug   → DEBUG_BUILD=1  (verbose serial logging)
//   env:esp32p4-release → DEBUG_BUILD=0  (silent, max performance)
#ifndef DEBUG_BUILD
  #define DEBUG_BUILD   0    // Default to production (silent) if not set by build
#endif

// ───────────────────────────────────────────────────────────────────────────────
// DEBUG LOGGING MACROS
// ───────────────────────────────────────────────────────────────────────────────
#if DEBUG_BUILD
  #define LOG_D(f,...) Serial.printf("[D] " f "\n", ##__VA_ARGS__)
  #define LOG_I(f,...) Serial.printf("[I] " f "\n", ##__VA_ARGS__)
  #define LOG_W(f,...) Serial.printf("[W] " f "\n", ##__VA_ARGS__)
  #define LOG_E(f,...) Serial.printf("[E] " f "\n", ##__VA_ARGS__)
#else
  #define LOG_D(...) do{}while(0)
  #define LOG_I(...) do{}while(0)
  #define LOG_W(...) do{}while(0)
  #define LOG_E(f,...) do{}while(0)  // Suppress in release build
#endif

// ───────────────────────────────────────────────────────────────────────────────
// PROTECTED GPIO — NEVER USE (ESP32-P4 specific)
// ───────────────────────────────────────────────────────────────────────────────
// GPIO used by MIPI-DSI interface: reserved by hardware
// GPIO used by PSRAM: reserved by hardware
// GPIO used by Flash: reserved by hardware
// GPIO used by USB: reserved by hardware
