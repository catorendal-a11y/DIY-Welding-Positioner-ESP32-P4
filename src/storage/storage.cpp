#include "storage.h"

std::vector<Preset> g_presets;

void storage_init() {
    LOG_I("Initializing LittleFS for Preset Storage...");
    // `true` forces a format if mounting fails (e.g., first ever boot)
    if (!LittleFS.begin(true)) {
        LOG_E("LittleFS Mount Failed!");
        return;
    }
    LOG_I("LittleFS mounted successfully.");
    storage_load_presets();
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
            // Immediately sync change to flash
            return storage_save_presets();
        }
    }
    return false; // ID not found
}
