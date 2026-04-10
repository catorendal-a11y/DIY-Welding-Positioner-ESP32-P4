#pragma once

#include <Arduino.h>
#include <vector>
#include <atomic>
#include <ArduinoJson.h>
#include "../config.h"
#include "../control/control.h"

// Maximum number of presets the system can hold
#define MAX_PRESETS 16
// Legacy LittleFS paths (used only for one-time migration in storage.cpp)
#define PRESETS_FILE "/presets.json"
#define SETTINGS_FILE "/settings.json"

struct SystemSettings {
    int acceleration;
    int microstep;         // divisor: 8/16/32 -> 1600/3200/6400 PULSE/REV on NEMA 23
    float max_rpm;         // UI/pot ceiling [MIN_RPM .. MAX_RPM], default MAX_RPM
    float calibration_factor;
    bool rpm_buttons_enabled;
    uint8_t brightness;    // 0-255 backlight PWM
    uint8_t dim_timeout;   // Seconds before auto-dim (0=off, 30, 60, 120, 300)
    bool dir_switch_enabled; // CW/CCW hardware switch on GPIO28
    bool invert_direction;   // Invert CW/CCW direction
    uint8_t accent_color;  // Index into theme palette (0=Orange, 1=Cyan, etc.)
    uint8_t countdown_seconds; // Countdown before rotation start (1-10, default 3)
    uint8_t stepper_driver;    // STEPPER_DRIVER_STANDARD or STEPPER_DRIVER_DM542T (pulse/dir timing)
    uint8_t settings_version;
};

// Preset struct matching the system's control capabilities
struct Preset {
    uint8_t id;
    char name[32];
    SystemState mode; // Indicates target mode (STATE_RUNNING, STATE_PULSE, etc)
    float rpm;
    
    // Pulse settings
    uint32_t pulse_on_ms;
    uint32_t pulse_off_ms;

    // Step settings
    float step_angle; // e.g., 90.0 degrees
    
    // Timer settings
    uint32_t timer_ms;

    // Extended settings (v1.3.0)
    uint8_t direction;         // 0=CW, 1=CCW
    uint16_t pulse_cycles;     // 0=infinite
    uint16_t step_repeats;     // 1..99
    float step_dwell_sec;      // 0..30s
    uint8_t timer_auto_stop;   // 1=auto stop at end
    uint8_t cont_soft_start;   // 1=soft start enabled
};

// Global active preset list loaded from flash
extern std::vector<Preset> g_presets;
extern SemaphoreHandle_t g_presets_mutex;

// Global settings
extern SystemSettings g_settings;
extern SemaphoreHandle_t g_settings_mutex;
extern SemaphoreHandle_t g_nvs_mutex;
extern std::atomic<bool> g_dir_switch_cache;

// Flash write coordination — set true during NVS writes (flash cache may be disabled).
// lvglTask skips rendering while this is true to prevent blue screen flashes
// caused by CPU cache being disabled during flash operations.
extern std::atomic<bool> g_flashWriting;
extern volatile bool g_screenRedraw;

// Core functions
void storage_init();
bool storage_load_presets();
bool storage_save_presets();
bool storage_load_settings();
void storage_save_settings();
void storage_flush();

// Helper functions
bool storage_get_preset(uint8_t id, Preset* out);
bool storage_delete_preset(uint8_t id);
void storage_get_usage(size_t* used, size_t* total);
void storage_format();

// FreeRTOS task
void storageTask(void* pvParameters);  // Program save/load task (Core 1, priority 1)
