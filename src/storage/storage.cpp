// Storage - LittleFS persistence for settings and presets
#include "storage.h"
#include "WiFi.h"
#include <cstring>

std::vector<Preset> g_presets;
SemaphoreHandle_t g_presets_mutex;
SemaphoreHandle_t g_settings_mutex;
SystemSettings g_settings = { 5000, 8, 1.0f, true, 150, 60, true, false, 0, "", "", BLE_DEVICE_NAME_DEFAULT, true, true, 3, 1 };
std::atomic<bool> g_dir_switch_cache{true};

static volatile bool savePending = false;
static uint32_t lastSaveMs = 0;
static bool presetsSavePending = false;
static uint32_t lastPresetsSaveMs = 0;


void storage_init() {
    LOG_I("Initializing LittleFS for Preset Storage...");
    if (!LittleFS.begin(false)) {
        LOG_E("LittleFS mount failed, attempting format...");
        if (!LittleFS.begin(true)) {
            LOG_E("LittleFS format failed — restarting!");
            ESP.restart();
        }
        LOG_W("LittleFS formatted - default settings loaded");
    }
    LOG_I("LittleFS mounted successfully.");
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
    g_wifi_mutex = xSemaphoreCreateMutex();
    if (!g_wifi_mutex) {
        LOG_E("Failed to create wifi mutex!");
        ESP.restart();
    }
    storage_load_presets();
    storage_load_settings();
    lastSaveMs = millis();
    lastPresetsSaveMs = millis();
}

bool storage_load_presets() {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    g_presets.clear();
    
    if (!LittleFS.exists(PRESETS_FILE)) {
        LOG_W("Presets file '%s' does not exist, starting with empty list", PRESETS_FILE);
        xSemaphoreGive(g_presets_mutex);
        return true; 
    }

    File file = LittleFS.open(PRESETS_FILE, FILE_READ);
    if (!file) {
        LOG_E("Failed to open presets file for reading");
        xSemaphoreGive(g_presets_mutex);
        return false;
    }

    // Allocate JSON document (Dynamic allocation is safe on ESP32-P4 with lots of RAM)
    // 16 presets takes ~2-3KB to parse
    JsonDocument doc; 

    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        LOG_E("Failed to parse presets JSON file: %s", error.c_str());
        xSemaphoreGive(g_presets_mutex);
        return false;
    }

    JsonArray array = doc.as<JsonArray>();
    for (JsonObject obj : array) {
        if (g_presets.size() >= MAX_PRESETS) break;

        Preset p;
        p.id = obj["id"] | 0;
        strlcpy(p.name, obj["name"] | "Unnamed", sizeof(p.name));
        sanitize_ascii(p.name, sizeof(p.name));
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

        p.rpm = constrain(p.rpm, 0.02f, MAX_RPM);
        p.pulse_on_ms = constrain(p.pulse_on_ms, (uint32_t)10, (uint32_t)60000);
        p.pulse_off_ms = constrain(p.pulse_off_ms, (uint32_t)10, (uint32_t)60000);
        p.step_angle = constrain(p.step_angle, 0.1f, 360.0f);
        p.timer_ms = constrain(p.timer_ms, (uint32_t)1, (uint32_t)3600000);

        g_presets.push_back(p);
    }

    LOG_I("Loaded %u presets from LittleFS.", (unsigned)g_presets.size());
    xSemaphoreGive(g_presets_mutex);
    return true;
}

static bool storage_save_presets_internal() {
    std::vector<Preset> localCopy;
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    localCopy = g_presets;
    xSemaphoreGive(g_presets_mutex);

    File file = LittleFS.open(PRESETS_FILE ".tmp", FILE_WRITE);
    if (!file) {
        LOG_E("Failed to open presets temp file for writing");
        return false;
    }

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (const auto& p : localCopy) {
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
        LOG_E("Failed to write JSON sequence to presets temp file");
        file.close();
        LittleFS.remove(PRESETS_FILE ".tmp");
        return false;
    }

    file.close();
    if (!LittleFS.remove(PRESETS_FILE)) {
        LOG_W("Could not remove old presets file (may not exist)");
    }
    if (!LittleFS.rename(PRESETS_FILE ".tmp", PRESETS_FILE)) {
        LOG_E("Failed to rename presets temp file — data may be in .tmp");
    }
    LOG_I("Successfully saved %u presets to LittleFS.", (unsigned)localCopy.size());
    return true;
}

bool storage_save_presets() {
    presetsSavePending = true;
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

    g_settings.acceleration = constrain(doc["acceleration"] | 5000, (int)1000, (int)20000);
    g_settings.microstep = doc["microstep"] | 8;
    g_settings.calibration_factor = constrain(doc["calibration_factor"] | 1.0f, 0.5f, 1.5f);
    g_settings.rpm_buttons_enabled = doc["rpm_buttons_enabled"] | true;
    g_settings.brightness = constrain(doc["brightness"] | 150, (uint8_t)10, (uint8_t)255);
    g_settings.dim_timeout = doc["dim_timeout"] | 60;
    g_settings.dir_switch_enabled = doc["dir_switch_enabled"] | true;
    g_settings.invert_direction = doc["invert_direction"] | false;
    g_settings.accent_color = constrain(doc["accent_color"] | 0, (uint8_t)0, (uint8_t)7);
    strlcpy(g_settings.wifi_ssid, doc["wifi_ssid"] | WIFI_SSID, sizeof(g_settings.wifi_ssid));
    sanitize_ascii(g_settings.wifi_ssid, sizeof(g_settings.wifi_ssid));
    strlcpy(g_settings.wifi_pass, doc["wifi_pass"] | WIFI_PASS, sizeof(g_settings.wifi_pass));
    strlcpy(g_settings.ble_name, doc["ble_name"] | BLE_DEVICE_NAME_DEFAULT, sizeof(g_settings.ble_name));
    sanitize_ascii(g_settings.ble_name, sizeof(g_settings.ble_name));
    g_settings.ble_enabled = doc["ble_enabled"] | true;
    g_settings.wifi_enabled = doc["wifi_enabled"] | true;
    g_settings.countdown_seconds = constrain(doc["countdown_seconds"] | 3, (uint8_t)1, (uint8_t)10);
    g_settings.settings_version = doc["settings_version"] | 0;

    g_dir_switch_cache.store(g_settings.dir_switch_enabled, std::memory_order_release);

    LOG_I("Loaded system settings from LittleFS.");
    return true;
}

static bool storage_save_settings_internal() {
    SystemSettings snap;
    xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
    snap = g_settings;
    xSemaphoreGive(g_settings_mutex);

    File file = LittleFS.open(SETTINGS_FILE ".tmp", FILE_WRITE);
    if (!file) {
        LOG_E("Failed to open settings temp file for writing");
        return false;
    }

    JsonDocument doc;
    doc["acceleration"] = snap.acceleration;
    doc["microstep"] = snap.microstep;
    doc["calibration_factor"] = snap.calibration_factor;
    doc["rpm_buttons_enabled"] = snap.rpm_buttons_enabled;
    doc["brightness"] = snap.brightness;
    doc["dim_timeout"] = snap.dim_timeout;
    doc["dir_switch_enabled"] = snap.dir_switch_enabled;
    doc["invert_direction"] = snap.invert_direction;
    doc["accent_color"] = snap.accent_color;
    doc["wifi_ssid"] = snap.wifi_ssid;
    doc["wifi_pass"] = snap.wifi_pass;
    doc["ble_name"] = snap.ble_name;
    doc["ble_enabled"] = snap.ble_enabled;
    doc["wifi_enabled"] = snap.wifi_enabled;
    doc["countdown_seconds"] = snap.countdown_seconds;
    doc["settings_version"] = snap.settings_version;

    if (serializeJson(doc, file) == 0) {
        LOG_E("Failed to write JSON to settings temp file");
        file.close();
        LittleFS.remove(SETTINGS_FILE ".tmp");
        return false;
    }

    file.close();
    if (!LittleFS.remove(SETTINGS_FILE)) {
        LOG_W("Could not remove old settings file (may not exist)");
    }
    if (!LittleFS.rename(SETTINGS_FILE ".tmp", SETTINGS_FILE)) {
        LOG_E("Failed to rename settings temp file — data may be in .tmp");
    }
    LOG_I("Successfully saved settings to LittleFS.");
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
        storage_save_presets_internal();
    }

    if (savePending && (millis() - lastSaveMs > 1000)) {
        savePending = false;
        lastSaveMs = millis();
        storage_save_settings_internal();
    }
}

bool storage_get_preset(uint8_t id, Preset* out) {
    if (out == nullptr) return false;
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
    if (found) return storage_save_presets();
    return false;
}

void storage_get_usage(size_t* used, size_t* total) {
    *used = LittleFS.usedBytes();
    *total = LittleFS.totalBytes();
}

void storage_format() {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    g_presets.clear();
    g_settings = SystemSettings{ 5000, 8, 1.0f, true, 150, 60, false, false, 0, "", "", BLE_DEVICE_NAME_DEFAULT, true, true, 3, 1 };
    LittleFS.format();
    xSemaphoreGive(g_presets_mutex);
    LOG_I("Storage formatted - all data erased");
}

// ───────────────────────────────────────────────────────────────────────────────
// WIFI SHARED STATE (accessed from screen_wifi.cpp via extern)
// ESP-Hosted WiFi API is NOT thread-safe — shared SDIO bus to C6 co-processor
// ALL WiFi calls from UI go through pending flags processed by storageTask
// ───────────────────────────────────────────────────────────────────────────────
volatile bool wifiScanPending = false;
volatile bool wifiConnectPending = false;
volatile bool wifiTogglePending = false;
volatile bool wifiScanResultReady = false;
volatile int wifiScanResultCount = 0;
volatile bool wifiScanFailed = false;
volatile bool wifiIsConnected = false;
char wifiConnectedSsid[33] = "";
char wifiConnectedIp[16] = "";
volatile int wifiConnectedRssi = 0;
char wifiPendingSsid[33] = "";
char wifiPendingPass[65] = "";

WifiScanEntry wifiScanBuffer[WIFI_SCAN_MAX];
int wifiScanBufferCount = 0;
SemaphoreHandle_t g_wifi_mutex = nullptr;

static bool wifiScanStarted = false;
static uint32_t wifiReconnectInterval = 30000;
static uint32_t wifiLastReconnectAttempt = 0;
static uint32_t wifiLastStatusPoll = 0;

void wifi_process_pending() {
    // WiFi toggle MUST run even when turning OFF: g_settings.wifi_enabled is already false,
    // so an early return would skip disconnect/WIFI_OFF and leave the radio active (ESP-IDF
    // station teardown: disconnect connectivity before stopping — see Wi-Fi Deinit Phase).
    if (wifiTogglePending) {
        wifiTogglePending = false;
        if (g_settings.wifi_enabled) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(g_settings.wifi_ssid, g_settings.wifi_pass);
            LOG_I("WiFi enabled");
        } else {
            WiFi.STA.disconnect();
            WiFi.mode(WIFI_OFF);
            wifiScanStarted = false;
            wifiScanPending = false;
            wifiScanBufferCount = 0;
            wifiIsConnected = false;
            wifiConnectedSsid[0] = '\0';
            wifiConnectedIp[0] = '\0';
            wifiConnectedRssi = 0;
            LOG_I("WiFi disabled");
        }
    }

    if (!g_settings.wifi_enabled) {
        return;
    }

    // Update cached connection status (every 2s to avoid SDIO bus blocking idle task)
    uint32_t now = millis();
    if (now - wifiLastStatusPoll >= 2000) {
        wifiLastStatusPoll = now;
        bool connected = (WiFi.status() == WL_CONNECTED);
        if (g_wifi_mutex) xSemaphoreTake(g_wifi_mutex, portMAX_DELAY);
        wifiIsConnected = connected;
        if (connected) {
            strlcpy(wifiConnectedSsid, WiFi.SSID().c_str(), sizeof(wifiConnectedSsid));
            strlcpy(wifiConnectedIp, WiFi.localIP().toString().c_str(), sizeof(wifiConnectedIp));
            wifiConnectedRssi = WiFi.RSSI();
        } else {
            wifiConnectedSsid[0] = '\0';
            wifiConnectedIp[0] = '\0';
            wifiConnectedRssi = 0;
        }
        if (g_wifi_mutex) xSemaphoreGive(g_wifi_mutex);
    }

    // Process WiFi scan
    if (wifiScanPending) {
        wifiScanPending = false;
        wifiScanStarted = true;
        WiFi.scanNetworks(true);
        LOG_I("WiFi scan started (async)");
    }

    if (wifiScanStarted) {
        int result = WiFi.scanComplete();
        if (result >= 0) {
            wifiScanStarted = false;
            if (g_wifi_mutex) xSemaphoreTake(g_wifi_mutex, portMAX_DELAY);
            wifiScanBufferCount = (result > WIFI_SCAN_MAX) ? WIFI_SCAN_MAX : result;
            for (int i = 0; i < wifiScanBufferCount; i++) {
                strlcpy(wifiScanBuffer[i].ssid, WiFi.SSID(i).c_str(), sizeof(wifiScanBuffer[i].ssid));
                sanitize_ascii(wifiScanBuffer[i].ssid, sizeof(wifiScanBuffer[i].ssid));
                wifiScanBuffer[i].rssi = WiFi.RSSI(i);
                wifiScanBuffer[i].enc = WiFi.encryptionType(i);
            }
            wifiScanResultCount = result;
            if (g_wifi_mutex) xSemaphoreGive(g_wifi_mutex);
            wifiScanResultReady = true;
            WiFi.scanDelete();
            LOG_I("WiFi scan complete: %d networks", result);
        } else if (result == WIFI_SCAN_FAILED) {
            wifiScanStarted = false;
            wifiScanFailed = true;
            WiFi.scanDelete();
            LOG_W("WiFi scan failed");
        }
    }

    // Process WiFi connect
    if (wifiConnectPending) {
        wifiConnectPending = false;
        WiFi.mode(WIFI_STA);
        WiFi.STA.disconnect();
        WiFi.begin(wifiPendingSsid, wifiPendingPass);
        LOG_I("WiFi connecting to: %s", wifiPendingSsid);
    }

    // Auto-reconnect with exponential backoff
    if (g_settings.wifi_enabled && g_settings.wifi_ssid[0] != '\0' && !wifiIsConnected) {
        if (millis() - wifiLastReconnectAttempt > wifiReconnectInterval) {
            wifiLastReconnectAttempt = millis();
            WiFi.begin(g_settings.wifi_ssid, g_settings.wifi_pass);
            wifiReconnectInterval = (wifiReconnectInterval < 300000) ? wifiReconnectInterval * 2 : 300000;
        }
    }
    if (wifiIsConnected) {
        wifiReconnectInterval = 30000;
    }
}
