#pragma once
// TIG Welding Rotator Controller - Hardware Configuration
// Waveshare/Guition ESP32-P4 4.3" Touch Display Dev Board

// ───────────────────────────────────────────────────────────────────────────────
// GPIO HEADER PINS (2×13 pin header)
// Available GPIOs: 28–35, 49–52 (see board pin diagram)
// ───────────────────────────────────────────────────────────────────────────────
#define PIN_POT         49   // ADC — Potentiometer speed input
#define PIN_STEP        50   // Step pulse output (FastAccelStepper RMT)
#define PIN_DIR         51   // Direction: CW=HIGH, CCW=LOW
#define PIN_ENA         52   // Enable: Active LOW to TB6600
#define PIN_ESTOP       33   // Emergency Stop: Active LOW, NC contact
#define PIN_SPARE       35   // Reserved for future encoder/expansion

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY & TOUCH — MIPI-DSI (handled by ESP-IDF drivers)
// ───────────────────────────────────────────────────────────────────────────────
// Display: EK79007, 480×800 (portrait), MIPI-DSI — no GPIO pin config needed
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
#define MIN_RPM         0.1f   // Minimum workpiece RPM
#define MAX_RPM         3.0f   // Maximum workpiece RPM (6000 Hz — safe under TIG EMI with 40% margin)
                                // Raise to 5.0 in Settings only in clean environments
#define MICROSTEPS      8      // 1/8 microstepping (matches TB6600 DIP)
#define STEPS_PER_REV   (200 * MICROSTEPS)   // 1600 steps/rev motor

#define GEAR_RATIO      60.0f   // 60:1 worm gear
#define D_EMNE          0.300f  // Workpiece diameter: 300mm
#define D_RULLE         0.080f  // Roller diameter: 80mm

// Motor: NEMA 23 (3 Nm)
// 60:1 worm gear provides excellent holding torque and self-locking

// ───────────────────────────────────────────────────────────────────────────────
// BUILD CONFIGURATION
// ───────────────────────────────────────────────────────────────────────────────
#define DEBUG_BUILD     1      // 1 = debug logs enabled, 0 = production (silent)

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
  #define LOG_E(...) do{}while(0)
#endif

// ───────────────────────────────────────────────────────────────────────────────
// PROTECTED GPIO — NEVER USE (ESP32-P4 specific)
// ───────────────────────────────────────────────────────────────────────────────
// GPIO used by MIPI-DSI interface: reserved by hardware
// GPIO used by PSRAM: reserved by hardware
// GPIO used by Flash: reserved by hardware
// GPIO used by USB: reserved by hardware
