#include "storage.h"

std::vector<Preset> g_presets;
SystemSettings g_settings = { 5000, 8, 1.0f, false, 150, 60, false };

static volatile bool savePending = false;
static uint32_t lastSaveMs = 0;


void storage_init() {
    LOG_I("Initializing LittleFS for Preset Storage...");
    // `true` forces a format if mounting fails (e.g., first ever boot)
    if (!LittleFS.begin(true)) {
        LOG_E("LittleFS Mount Failed!");
        return;
    }
    LOG_I("LittleFS mounted successfully.");
    storage_load_presets();
    storage_load_settings();
}

bool storage_load_presets() {
    g_presets.clear();
    
    if (!LittleFS.exists(PRESETS_FILE)) {
        LOG_W("Presets file '%s' does not exist, starting with empty list", PRESETS_FILE);
        // We could seed a default preset here if desired, but empty is safe for now
        return true; 
    }

    File file = LittleFS.open(PRESETS_FILE, FILE_READ);
    if (!file) {
        LOG_E("Failed to open presets file for reading");
        return false;
    }

    // Allocate JSON document (Dynamic allocation is safe on ESP32-P4 with lots of RAM)
    // 16 presets takes ~2-3KB to parse
    JsonDocument doc; 

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_E("Failed to parse presets JSON file: %s", error.c_str());
        return false;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        if (g_presets.size() >= MAX_PRESETS) break;

        Preset p;
        p.id = obj["id"] | 0;
        strlcpy(p.name, obj["name"] | "Unnamed", sizeof(p.name));
        p.mode = (SystemState)(obj["mode"] | (int)STATE_RUNNING);
        p.rpm = obj["rpm"] | 1.0f;
        p.pulse_on_ms = obj["pulse_on"] | 500;
        p.pulse_off_ms = obj["pulse_off"] | 500;
        p.step_angle = obj["step_angle"] | 90.0f;
        p.timer_ms = obj["timer_ms"] | 5000;

        // Extended fields (v1.3.0) — safe defaults for old files
        p.direction        = obj["direction"] | 0;
        p.pulse_cycles     = obj["pulse_cycles"] | 0;
        p.step_repeats     = obj["step_repeats"] | 1;
        p.step_dwell_sec   = obj["step_dwell_sec"] | 0.0f;
        p.timer_auto_stop  = obj["timer_auto_stop"] | 1;
        p.cont_soft_start  = obj["cont_soft_start"] | 0;

        g_presets.push_back(p);
    }

    LOG_I("Loaded %d presets from LittleFS.", g_presets.size());
    return true;
}

bool storage_save_presets() {
    File file = LittleFS.open(PRESETS_FILE, FILE_WRITE);
    if (!file) {
        LOG_E("Failed to open presets file for writing");
        return false;
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& p : g_presets) {
        JsonObject obj = array.add<JsonObject>();
        obj["id"] = p.id;
        obj["name"] = p.name;
        obj["mode"] = (int)p.mode;
        obj["rpm"] = p.rpm;
        obj["pulse_on"] = p.pulse_on_ms;
        obj["pulse_off"] = p.pulse_off_ms;
        obj["step_angle"] = p.step_angle;
        obj["timer_ms"] = p.timer_ms;
        obj["direction"] = p.direction;
        obj["pulse_cycles"] = p.pulse_cycles;
        obj["step_repeats"] = p.step_repeats;
        obj["step_dwell_sec"] = p.step_dwell_sec;
        obj["timer_auto_stop"] = p.timer_auto_stop;
        obj["cont_soft_start"] = p.cont_soft_start;
    }

    if (serializeJson(doc, file) == 0) {
        LOG_E("Failed to write JSON sequence to presets file");
        file.close();
        return false;
    }

    file.close();
    LOG_I("Successfully saved %d presets to LittleFS.", g_presets.size());
    return true;
}

bool storage_load_settings() {
    if (!LittleFS.exists(SETTINGS_FILE)) {
        LOG_W("Settings file '%s' does not exist, using defaults", SETTINGS_FILE);
        return true; 
    }

    File file = LittleFS.open(SETTINGS_FILE, FILE_READ);
    if (!file) {
        LOG_E("Failed to open settings file for reading");
        return false;
    }

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_E("Failed to parse settings JSON file: %s", error.c_str());
        return false;
    }

    g_settings.acceleration = doc["acceleration"] | 5000;
    g_settings.microstep = doc["microstep"] | 8;
    g_settings.calibration_factor = doc["calibration_factor"] | 1.0f;
    g_settings.rpm_buttons_enabled = doc["rpm_buttons_enabled"] | true;
    g_settings.brightness = doc["brightness"] | 150;
    g_settings.dim_timeout = doc["dim_timeout"] | 60;
    g_settings.dir_switch_enabled = doc["dir_switch_enabled"] | false;

    LOG_I("Loaded system settings from LittleFS.");
    return true;
}

static bool storage_save_settings_internal() {
    File file = LittleFS.open(SETTINGS_FILE, FILE_WRITE);
    if (!file) {
        LOG_E("Failed to open settings file for writing");
        return false;
    }

    JsonDocument doc;
    doc["acceleration"] = g_settings.acceleration;
    doc["microstep"] = g_settings.microstep;
    doc["calibration_factor"] = g_settings.calibration_factor;
    doc["rpm_buttons_enabled"] = g_settings.rpm_buttons_enabled;
    doc["brightness"] = g_settings.brightness;
    doc["dim_timeout"] = g_settings.dim_timeout;
    doc["dir_switch_enabled"] = g_settings.dir_switch_enabled;

    if (serializeJson(doc, file) == 0) {
        LOG_E("Failed to write JSON to settings file");
        file.close();
        return false;
    }

    file.close();
    LOG_I("Successfully saved settings to LittleFS.");
    return true;
}

void storage_save_settings() {
    savePending = true;
}

void storage_flush() {
    if (savePending && (millis() - lastSaveMs > 500)) {
        savePending = false;
        lastSaveMs = millis();
        storage_save_settings_internal();
    }
}

Preset* storage_get_preset(uint8_t id) {
    for (auto& p : g_presets) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

bool storage_delete_preset(uint8_t id) {
    for (auto it = g_presets.begin(); it != g_presets.end(); ++it) {
        if (it->id == id) {
            g_presets.erase(it);
            return storage_save_presets();
        }
    }
    return false;
}

void storage_get_usage(size_t* used, size_t* total) {
    *used = LittleFS.usedBytes();
    *total = LittleFS.totalBytes();
}

void storage_format() {
    g_presets.clear();
    g_settings = { 5000, 8, 1.0f, false, 150, 60, false };
    LittleFS.format();
    LOG_I("Storage formatted - all data erased");
}
