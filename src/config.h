#pragma once
// TIG Welding Rotator Controller - Hardware Configuration
// Waveshare/Guition ESP32-P4 4.3" Touch Display Dev Board

#define FW_VERSION "v2.0.3"

// ───────────────────────────────────────────────────────────────────────────────
// GPIO HEADER PINS (2×13 pin header)
// Available GPIOs: 28–35, 49–52 (see board pin diagram)
// ───────────────────────────────────────────────────────────────────────────────
#define PIN_STEP        50   // Step pulse output (FastAccelStepper RMT)
#define PIN_DIR         51   // Direction: CW=HIGH, CCW=LOW
#define PIN_ENA         52   // Enable: Active LOW (typical PUL/DIR driver when wired same)
#define PIN_ESTOP       34   // Emergency Stop: Active LOW, NC contact
                            // NOTE: GPIO34 is a strapping pin (JTAG source control).
                            // Default eFuse config ignores GPIO34, so ESTOP is safe.
                            // GPIO34 has NO internal pull resistors — external pull-up
                            // from NC contact is required.
#define PIN_DIR_SWITCH  29   // CW/CCW direction switch (INPUT_PULLUP, LOW=CCW, HIGH=CW)
#define PIN_POT         49   // ADC — Potentiometer speed input (ADC2_CH0)
#define PIN_PEDAL_SW    33   // Foot pedal start switch (INPUT_PULLUP, LOW=pressed)
#define ADS1115_ADDR    0x48 // ADS1115 I2C address when ENABLE_ADS1115_PEDAL is 1
#ifndef ENABLE_ADS1115_PEDAL
#define ENABLE_ADS1115_PEDAL 0  // 1 = probe touch I2C bus for ADS1115 pedal ADC (SDA7/SCL8)
#endif

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
// DM542T: DIP microstep must match Motor Config (default 1/16 = 3200 pulses/rev). See docs/HARDWARE_SETUP.md §3.
// Workpiece RPM: pot/slider span MIN_RPM .. speed_get_rpm_max() (max stored in NVS, Motor Config).
#define MIN_RPM         0.001f    // Minimum workpiece RPM (pot CCW / clamp floor)
#define MAX_RPM         3.0f      // Absolute ceiling for Max RPM setting and firmware clamp

// Saved as g_settings.stepper_driver — must match screen + motor.cpp
#define STEPPER_DRIVER_STANDARD 0u  // PUL/DIR without extra DIR holdoff
#define STEPPER_DRIVER_DM542T   1u

// GEAR & ROLLER SYSTEM
// Stage 1: NMRV030 worm gearbox, 60:1 motor to worm-wheel shaft.
// Stage 2: spur m=1.5, 40T (on worm shaft) drives 72T output => x1.8 (see docs/images/motor.worm.svg).
// Total motor:output = 60 * (72/40) = 108.
#define GEAR_RATIO      (60.0f * 72.0f / 40.0f)   // = 108
#define D_EMNE          0.300f    // Workpiece diameter: 300mm
#define D_RULLE         0.080f    // Roller diameter: 80mm
// Kinematics note: at 3200 spr and total 1:108 only (no roller), output shaft ~960 full steps per degree.
// Workpiece RPM uses rpmToStepHz() with (D_EMNE/D_RULLE) in speed.cpp.

// SPEED CHARACTERISTICS
#define START_SPEED     100       // Hz — accel ramp start (MIN_RPM target ~216 Hz at 1/16 + default rollers)

// Pot ADC (PIN_POT): ref 3315 matches MIN-RPM end of travel (see speed.cpp).
// Snap band (0 = off): if > 0, ADC at/below this maps to RPM ceiling (EMI helper; hurts top-end precision).
#define POT_ADC_SNAP_MAX_RPM           0
#define POT_ADC_SNAP_MAX_RPM_RUNNING   0
// After +/- buttons, pot must move 200 ADC counts OR change mapped RPM by this
// much to take over (else user at pot end stop cannot override a lower slider value).
#define POT_SLIDER_OVERRIDE_RPM_DELTA  0.04f

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
// ASCII SANITIZATION — LVGL Montserrat fonts only cover 0x20-0x7E
// ───────────────────────────────────────────────────────────────────────────────
#include <cstddef>
inline void sanitize_ascii(char* buf, unsigned int len) {
  for (unsigned int i = 0; i < len && buf[i]; i++) {
    if (buf[i] < 0x20 || buf[i] > 0x7E) buf[i] = '?';
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// CROSS-CORE FLAGS
// ───────────────────────────────────────────────────────────────────────────────
volatile extern bool g_wakePending;  // Set by Core 0 (pot/dir change), cleared by Core 1 (dim_update)

// ───────────────────────────────────────────────────────────────────────────────
// PROTECTED GPIO — NEVER USE (ESP32-P4 specific)
// ───────────────────────────────────────────────────────────────────────────────
// GPIO used by MIPI-DSI interface: reserved by hardware
// GPIO used by PSRAM: reserved by hardware
// GPIO used by Flash: reserved by hardware
// GPIO used by USB: reserved by hardware
