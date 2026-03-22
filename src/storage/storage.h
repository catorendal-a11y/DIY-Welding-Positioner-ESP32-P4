#pragma once

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../control/control.h"

// Maximum number of presets the system can hold
#define MAX_PRESETS 16
#define PRESETS_FILE "/presets.json"

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
};

// Global active preset list loaded from flash
extern std::vector<Preset> g_presets;

// Core functions
void storage_init();
bool storage_load_presets();
bool storage_save_presets();

// Helper functions
Preset* storage_get_preset(uint8_t id);
bool storage_delete_preset(uint8_t id);
