#pragma once
// TIG Welding Rotator Controller - Hardware Configuration
// Waveshare/Guition ESP32-P4 4.3" Touch Display Dev Board

// ───────────────────────────────────────────────────────────────────────────────
// GPIO HEADER PINS (2×13 pin header)
// Available GPIOs: 28–35, 49–52 (see board pin diagram)
// ───────────────────────────────────────────────────────────────────────────────
#define PIN_POT         49   // ADC — Potentiometer speed input (left row 7)
#define PIN_STEP        30   // Step pulse output → TB6600 PUL+ (right side, row 6)
#define PIN_DIR         31   // Direction → TB6600 DIR+ (right side, row 5)
#define PIN_ENA         33   // Enable → TB6600 ENA+ (right side, row 4)
#define PIN_ESTOP       34   // Emergency Stop: Active LOW, NC contact
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
#define DISPLAY_H_RES   800   // Landscape width (after rotation)
#define DISPLAY_V_RES   480   // Landscape height (after rotation)
#define DISPLAY_H_RES_NATIVE 480  // Native portrait width
#define DISPLAY_V_RES_NATIVE 800  // Native portrait height

// ───────────────────────────────────────────────────────────────────────────────
// MOTOR & MECHANICAL PARAMETERS
// ───────────────────────────────────────────────────────────────────────────────
#define MIN_RPM         0.1f   // Minimum output RPM
#define MAX_RPM         5.0f   // Maximum output RPM (1/108 gearing)
#define MICROSTEPS      8      // 1/8 microstepping (matches TB6600 DIP)
#define STEPS_PER_REV   (200 * MICROSTEPS)   // 1600 steps/rev motor

// User's actual setup: 1/60 gearbox + additional gears = 1/108 total ratio
#define GEAR_RATIO      108.0f // Total gear reduction (motor:output = 108:1)
#define D_EMNE          0.300f  // Workpiece diameter: 300mm (not used, kept for ref)
#define D_RULLE         0.080f  // Roller diameter: 80mm (not used, kept for ref)

// Motor: NEMA 23
// 108:1 total gear ratio
#define ACCELERATION    5000   // Stepper acceleration (steps/s²)
                                // 5000 = ~1s ramp to max speed. Lower for smoother starts.

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
  #define LOG_E(f,...) Serial.printf("[E] " f "\n", ##__VA_ARGS__)  // Always active
#endif

// ───────────────────────────────────────────────────────────────────────────────
// PROTECTED GPIO — NEVER USE (ESP32-P4 specific)
// ───────────────────────────────────────────────────────────────────────────────
// GPIO used by MIPI-DSI interface: reserved by hardware
// GPIO used by PSRAM: reserved by hardware
// GPIO used by Flash: reserved by hardware
// GPIO used by USB: reserved by hardware
