// Storage - NVS persistence for settings and presets (Preferences)
#include "storage.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <cstring>
#include <vector>

// NVS namespace and keys (names <= 15 chars for ESP-IDF NVS)
#define NVS_NS "wrot"
#define NVS_KEY_SETTINGS "cfg"
#define NVS_KEY_PRESETS "prs"

std::vector<Preset> g_presets;
SemaphoreHandle_t g_presets_mutex;
SemaphoreHandle_t g_settings_mutex;
SemaphoreHandle_t g_nvs_mutex;
SystemSettings g_settings = { 7500, 16, MAX_RPM, 1.0f, true, 150, 60, true, false, 0, 3, STEPPER_DRIVER_DM542T, 1 };
std::atomic<bool> g_dir_switch_cache{true};
std::atomic<bool> g_flashWriting{false};
volatile bool g_screenRedraw = false;

static volatile bool savePending = false;
static uint32_t lastSaveMs = 0;
static bool presetsSavePending = false;
static uint32_t lastPresetsSaveMs = 0;

static Preferences g_prefs;
static bool g_prefs_open = false;

static void storage_migrate_littlefs_to_nvs();
static bool storage_apply_settings_doc(JsonObjectConst doc);
static bool storage_parse_presets_buffer(const uint8_t* data, size_t len);

void storage_init() {
    LOG_I("Initializing NVS storage...");
    g_nvs_mutex = xSemaphoreCreateMutex();
    if (!g_nvs_mutex) {
        LOG_E("Failed to create NVS mutex!");
        ESP.restart();
    }
    g_presets_mutex = xSemaphoreCreateMutex();
    if (!g_presets_mutex) {
        LOG_E("Failed to create presets mutex!");
        ESP.restart();
    }
    g_settings_mutex = xSemaphoreCreateMutex();
    if (!g_settings_mutex) {
        LOG_E("Failed to create settings mutex!");
        ESP.restart();
    }

    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    if (!g_prefs.begin(NVS_NS, false)) {
        LOG_E("NVS namespace open failed");
        xSemaphoreGive(g_nvs_mutex);
        ESP.restart();
    }
    g_prefs_open = true;
    xSemaphoreGive(g_nvs_mutex);

    storage_migrate_littlefs_to_nvs();
    storage_load_presets();
    storage_load_settings();
    lastSaveMs = millis();
    lastPresetsSaveMs = millis();
    LOG_I("NVS storage ready.");
}

static void storage_migrate_littlefs_to_nvs() {
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const size_t haveCfg = g_prefs.getBytesLength(NVS_KEY_SETTINGS);
    const size_t havePrs = g_prefs.getBytesLength(NVS_KEY_PRESETS);
    xSemaphoreGive(g_nvs_mutex);

    if (!LittleFS.begin(false)) {
        LOG_I("LittleFS not available; skipping legacy file migration");
        return;
    }

    bool migrated = false;
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    if (haveCfg == 0 && LittleFS.exists(SETTINGS_FILE)) {
        File f = LittleFS.open(SETTINGS_FILE, FILE_READ);
        if (f) {
            const size_t sz = f.size();
            std::vector<uint8_t> buf(sz);
            if (sz > 0 && f.read(buf.data(), sz) == sz) {
                if (g_prefs.putBytes(NVS_KEY_SETTINGS, buf.data(), sz)) {
                    LOG_I("Migrated settings from LittleFS to NVS");
                    migrated = true;
                }
            }
            f.close();
        }
    }
    if (havePrs == 0 && LittleFS.exists(PRESETS_FILE)) {
        File f = LittleFS.open(PRESETS_FILE, FILE_READ);
        if (f) {
            const size_t sz = f.size();
            std::vector<uint8_t> buf(sz);
            if (sz > 0 && f.read(buf.data(), sz) == sz) {
                if (g_prefs.putBytes(NVS_KEY_PRESETS, buf.data(), sz)) {
                    LOG_I("Migrated presets from LittleFS to NVS");
                    migrated = true;
                }
            }
            f.close();
        }
    }
    xSemaphoreGive(g_nvs_mutex);
    LittleFS.end();
    if (migrated) {
        LOG_I("Legacy LittleFS migration finished (partition may still exist unused)");
    }
}

bool storage_load_presets() {
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const size_t len = g_prefs.getBytesLength(NVS_KEY_PRESETS);
    std::vector<uint8_t> buf;
    if (len > 0) {
        buf.resize(len);
        g_prefs.getBytes(NVS_KEY_PRESETS, buf.data(), len);
    }
    xSemaphoreGive(g_nvs_mutex);

    if (buf.empty()) {
        LOG_W("No presets in NVS, starting with empty list");
        xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
        g_presets.clear();
        xSemaphoreGive(g_presets_mutex);
        return true;
    }

    return storage_parse_presets_buffer(buf.data(), buf.size());
}

void preset_clamp_mode_to_mask(Preset* p) {
  if (p->mode_mask == 0) p->mode_mask = preset_mode_to_mask(p->mode);
  if ((preset_mode_to_mask(p->mode) & p->mode_mask) == 0) {
    p->mode = preset_first_in_mask(p->mode_mask);
  }
}

static bool storage_parse_presets_buffer(const uint8_t* data, size_t len) {
    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, data, len);
    if (error) {
        LOG_E("Failed to parse presets JSON from NVS: %s", error.c_str());
        return false;
    }

    std::vector<Preset> loaded;
    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        if (loaded.size() >= MAX_PRESETS) {
            break;
        }

        Preset p;
        p.id = obj["id"] | 0;
        strlcpy(p.name, obj["name"] | "Unnamed", sizeof(p.name));
        sanitize_ascii(p.name, sizeof(p.name));
        p.mode = (SystemState)(obj["mode"] | (int)STATE_RUNNING);
        p.mode_mask = (uint8_t)(obj["mode_mask"] | 0) & PRESET_MASK_ALL;
        if (p.mode_mask == 0) {
          p.mode_mask = preset_mode_to_mask(p.mode);
        }
        preset_clamp_mode_to_mask(&p);
        p.rpm = obj["rpm"] | 1.0f;
        p.pulse_on_ms = obj["pulse_on"] | 500;
        p.pulse_off_ms = obj["pulse_off"] | 500;
        p.step_angle = obj["step_angle"] | 90.0f;
        p.timer_ms = obj["timer_ms"] | 5000;

        p.direction = obj["direction"] | 0;
        p.pulse_cycles = obj["pulse_cycles"] | 0;
        p.step_repeats = obj["step_repeats"] | 1;
        p.step_dwell_sec = obj["step_dwell_sec"] | 0.0f;
        p.timer_auto_stop = obj["timer_auto_stop"] | 1;
        p.cont_soft_start = obj["cont_soft_start"] | 0;

        float rpmCap = MAX_RPM;
        xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
        rpmCap = g_settings.max_rpm;
        xSemaphoreGive(g_settings_mutex);
        if (rpmCap < MIN_RPM) rpmCap = MIN_RPM;
        if (rpmCap > MAX_RPM) rpmCap = MAX_RPM;
        p.rpm = constrain(p.rpm, MIN_RPM, rpmCap);
        p.pulse_on_ms = constrain(p.pulse_on_ms, (uint32_t)10, (uint32_t)60000);
        p.pulse_off_ms = constrain(p.pulse_off_ms, (uint32_t)10, (uint32_t)60000);
        p.step_angle = constrain(p.step_angle, 0.1f, 360.0f);
        p.timer_ms = constrain(p.timer_ms, (uint32_t)1, (uint32_t)3600000);

        loaded.push_back(p);
    }

    const size_t count = loaded.size();
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    g_presets = std::move(loaded);
    xSemaphoreGive(g_presets_mutex);

    LOG_I("Loaded %u presets from NVS.", (unsigned)count);
    return true;
}

static bool storage_save_presets_internal() {
    std::vector<Preset> localCopy;
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    localCopy = g_presets;
    xSemaphoreGive(g_presets_mutex);

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& p : localCopy) {
        JsonObject obj = array.add<JsonObject>();
        obj["id"] = p.id;
        obj["name"] = p.name;
        obj["mode"] = (int)p.mode;
        obj["mode_mask"] = p.mode_mask;
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

    const size_t need = measureJson(doc);
    if (need == 0 && !localCopy.empty()) {
        LOG_E("Failed to measure presets JSON");
        return false;
    }
    std::vector<uint8_t> buf(need + 1);
    const size_t written = serializeJson(doc, buf.data(), buf.size());
    if (written == 0 && !localCopy.empty()) {
        LOG_E("Failed to serialize presets JSON");
        return false;
    }

    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const bool ok = g_prefs.putBytes(NVS_KEY_PRESETS, buf.data(), written);
    xSemaphoreGive(g_nvs_mutex);

    if (!ok) {
        LOG_E("NVS putBytes failed for presets");
        return false;
    }
    LOG_I("Saved %u presets to NVS.", (unsigned)localCopy.size());
    return true;
}

bool storage_save_presets() {
    presetsSavePending = true;
    return true;
}

bool storage_load_settings() {
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const size_t len = g_prefs.getBytesLength(NVS_KEY_SETTINGS);
    std::vector<uint8_t> buf;
    if (len > 0) {
        buf.resize(len);
        g_prefs.getBytes(NVS_KEY_SETTINGS, buf.data(), len);
    }
    xSemaphoreGive(g_nvs_mutex);

    if (buf.empty()) {
        LOG_W("No settings in NVS, using defaults");
        return true;
    }

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, buf.data(), buf.size());
    if (error) {
        LOG_E("Failed to parse settings JSON from NVS: %s", error.c_str());
        return false;
    }

    if (!storage_apply_settings_doc(doc.as<JsonObjectConst>())) {
        return false;
    }

    LOG_I("Loaded system settings from NVS.");
    return true;
}

static bool storage_apply_settings_doc(JsonObjectConst doc) {
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    g_settings.acceleration = constrain(doc["acceleration"] | 5000, (int)1000, (int)30000);
    g_settings.microstep = doc["microstep"] | 16;
    {
      float mx = doc["max_rpm"] | MAX_RPM;
      if (mx < MIN_RPM) mx = MIN_RPM;
      if (mx > MAX_RPM) mx = MAX_RPM;
      g_settings.max_rpm = mx;
    }
    g_settings.calibration_factor = constrain(doc["calibration_factor"] | 1.0f, 0.5f, 1.5f);
    g_settings.rpm_buttons_enabled = doc["rpm_buttons_enabled"] | true;
    g_settings.brightness = constrain(doc["brightness"] | 150, (uint8_t)10, (uint8_t)255);
    g_settings.dim_timeout = doc["dim_timeout"] | 60;
    g_settings.dir_switch_enabled = doc["dir_switch_enabled"] | true;
    g_settings.invert_direction = doc["invert_direction"] | false;
    g_settings.accent_color = constrain(doc["accent_color"] | 0, (uint8_t)0, (uint8_t)7);
    g_settings.countdown_seconds = constrain(doc["countdown_seconds"] | 3, (uint8_t)1, (uint8_t)10);
    // Missing JSON key: default to standard PUL/DIR timing for older NVS blobs (OTA / legacy).
    g_settings.stepper_driver = constrain(doc["stepper_driver"] | (int)STEPPER_DRIVER_STANDARD, 0, 1);
    g_settings.settings_version = doc["settings_version"] | 0;
    xSemaphoreGive(g_settings_mutex);

    g_dir_switch_cache.store(g_settings.dir_switch_enabled, std::memory_order_release);
    return true;
}

static bool storage_save_settings_internal() {
    SystemSettings snap;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    snap = g_settings;
    xSemaphoreGive(g_settings_mutex);

    JsonDocument doc;
    doc["acceleration"] = snap.acceleration;
    doc["microstep"] = snap.microstep;
    doc["max_rpm"] = snap.max_rpm;
    doc["calibration_factor"] = snap.calibration_factor;
    doc["rpm_buttons_enabled"] = snap.rpm_buttons_enabled;
    doc["brightness"] = snap.brightness;
    doc["dim_timeout"] = snap.dim_timeout;
    doc["dir_switch_enabled"] = snap.dir_switch_enabled;
    doc["invert_direction"] = snap.invert_direction;
    doc["accent_color"] = snap.accent_color;
    doc["countdown_seconds"] = snap.countdown_seconds;
    doc["stepper_driver"] = snap.stepper_driver;
    doc["settings_version"] = snap.settings_version;

    const size_t need = measureJson(doc);
    std::vector<uint8_t> buf(need + 1);
    const size_t written = serializeJson(doc, buf.data(), buf.size());
    if (written == 0) {
        LOG_E("Failed to serialize settings JSON");
        return false;
    }

    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const bool ok = g_prefs.putBytes(NVS_KEY_SETTINGS, buf.data(), written);
    xSemaphoreGive(g_nvs_mutex);

    if (!ok) {
        LOG_E("NVS putBytes failed for settings");
        return false;
    }
    LOG_I("Saved settings to NVS.");
    g_dir_switch_cache.store(snap.dir_switch_enabled, std::memory_order_release);
    return true;
}

void storage_save_settings() {
    savePending = true;
}

void storage_flush() {
    if (presetsSavePending && (millis() - lastPresetsSaveMs > 500)) {
        presetsSavePending = false;
        lastPresetsSaveMs = millis();
        LOG_I("FLUSH: presets save starting");
        g_flashWriting.store(true, std::memory_order_release);
        storage_save_presets_internal();
        g_flashWriting.store(false, std::memory_order_release);
        g_screenRedraw = true;
        LOG_I("FLUSH: presets save done");
    }

    if (savePending && (millis() - lastSaveMs > 1000)) {
        savePending = false;
        lastSaveMs = millis();
        LOG_I("FLUSH: settings save starting");
        g_flashWriting.store(true, std::memory_order_release);
        storage_save_settings_internal();
        g_flashWriting.store(false, std::memory_order_release);
        g_screenRedraw = true;
        LOG_I("FLUSH: settings save done");
    }
}

bool storage_get_preset(uint8_t id, Preset* out) {
    if (out == nullptr) {
        return false;
    }
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    bool found = false;
    for (auto& p : g_presets) {
        if (p.id == id) {
            *out = p;
            found = true;
            break;
        }
    }
    xSemaphoreGive(g_presets_mutex);
    return found;
}

bool storage_delete_preset(uint8_t id) {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    bool found = false;
    for (auto it = g_presets.begin(); it != g_presets.end(); ++it) {
        if (it->id == id) {
            g_presets.erase(it);
            found = true;
            break;
        }
    }
    xSemaphoreGive(g_presets_mutex);
    if (found) {
        return storage_save_presets();
    }
    return false;
}

void storage_get_usage(size_t* used, size_t* total) {
    // NVS data partition size from default_16MB.csv (0x6000)
    *total = 0x6000;
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    size_t u = g_prefs.getBytesLength(NVS_KEY_SETTINGS);
    u += g_prefs.getBytesLength(NVS_KEY_PRESETS);
    xSemaphoreGive(g_nvs_mutex);
    *used = u + 1536;
    if (*used > *total) {
        *used = *total;
    }
}

void storage_format() {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    g_presets.clear();
    g_settings = SystemSettings{ 5000, 8, MAX_RPM, 1.0f, true, 150, 60, false, false, 0, 3, STEPPER_DRIVER_STANDARD, 1 };
    xSemaphoreGive(g_presets_mutex);

    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    if (g_prefs_open) {
        g_prefs.clear();
    }
    xSemaphoreGive(g_nvs_mutex);

    g_dir_switch_cache.store(g_settings.dir_switch_enabled, std::memory_order_release);
    LOG_I("Storage formatted - NVS cleared");
}
