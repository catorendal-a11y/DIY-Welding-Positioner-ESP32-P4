// Storage - NVS persistence for settings and presets (Preferences)
#include "storage.h"
#include "settings_policy.h"
#include "save_request.h"
#include "name_policy.h"
#include "../app_state.h"
#include "../control/setup_policy.h"
#include "../motor/speed.h"
#include "../event_log.h"
#include <cmath>
#include <Preferences.h>
#include <LittleFS.h>
#include <atomic>
#include <cstring>
#include <vector>
#include <esp_partition.h>
#include <nvs.h>

// NVS namespace and keys (names <= 15 chars for ESP-IDF NVS)
#define NVS_NS "wrot"
#define NVS_KEY_SETTINGS "cfg"
#define NVS_KEY_PRESETS "prs"

std::vector<Preset> g_presets;
SemaphoreHandle_t g_presets_mutex;
SemaphoreHandle_t g_settings_mutex;
SemaphoreHandle_t g_nvs_mutex;
// Single source of truth for factory defaults; decode fallbacks, format and
// RAM initialization must all read this instead of copying literals around.
SystemSettings default_settings() {
  return SystemSettings{7500, 16, MAX_RPM, 1.0f, 150, 60, true, false, 0, 0, 3,
                        STEPPER_DRIVER_DM542T, false, 1};
}
SystemSettings g_settings = default_settings();
// Cross-core atomics (g_dir_switch_cache, g_flashWriting, g_screenRedraw) live in app_state.cpp.

static SaveRequest settingsSave{1000}, presetsSave{500};
StorageStatus storage_status() {
  if (settingsSave.failed() || presetsSave.failed()) return STORAGE_ERROR;
  return settingsSave.pending() || presetsSave.pending() ? STORAGE_PENDING : STORAGE_SAVED;
}
uint32_t storage_request_settings_save() { return settingsSave.request(); }
StorageStatus storage_settings_save_status(uint32_t ticket) {
  if (settingsSave.saved(ticket)) return STORAGE_SAVED;
  return settingsSave.failed() ? STORAGE_ERROR : STORAGE_PENDING;
}

uint32_t storage_request_presets_save() { return presetsSave.request(); }
StorageStatus storage_presets_save_status(uint32_t ticket) {
  if (presetsSave.saved(ticket)) return STORAGE_SAVED;
  return presetsSave.failed() ? STORAGE_ERROR : STORAGE_PENDING;
}
static constexpr size_t SETTINGS_BLOB_MAX = 4096, PRESETS_BLOB_MAX = 16384;
static Preferences g_prefs;
static bool g_prefs_open = false;

static void storage_migrate_littlefs_to_nvs();
static bool storage_apply_settings_doc(JsonObjectConst doc);
static bool storage_parse_presets_buffer(const uint8_t* data, size_t len);
static int storage_sanitize_microstep(int value);
static bool storage_decode_settings_doc(JsonObjectConst doc, SystemSettings& decoded);
static bool storage_decode_presets_buffer(const uint8_t* data, size_t len, float cap, std::vector<Preset>& loaded);

void storage_init() {
  LOG_I("Initializing NVS storage...");
  g_nvs_mutex = xSemaphoreCreateMutex();
  if (!g_nvs_mutex) fatal_halt("storage: NVS mutex alloc");
  g_presets_mutex = xSemaphoreCreateMutex();
  if (!g_presets_mutex) fatal_halt("storage: presets mutex alloc");
  g_settings_mutex = xSemaphoreCreateMutex();
  if (!g_settings_mutex) fatal_halt("storage: settings mutex alloc");

  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  if (!g_prefs.begin(NVS_NS, false)) {
    xSemaphoreGive(g_nvs_mutex);
    fatal_halt("storage: NVS namespace open");
  }
  g_prefs_open = true;
  xSemaphoreGive(g_nvs_mutex);

  storage_migrate_littlefs_to_nvs();
  if (!storage_load_settings() || !storage_load_presets())
    fatal_halt("storage corrupt: motion disabled; restore validated data");
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

  auto readLegacy = [](const char* path, size_t maximum, std::vector<uint8_t>& buffer) {
    if (!LittleFS.exists(path)) return true;
    File file = LittleFS.open(path, FILE_READ);
    if (!file || !file.size() || file.size() > maximum) return false;
    buffer.resize(file.size());
    return file.read(buffer.data(), buffer.size()) == buffer.size();
  };
  std::vector<uint8_t> cfg, prs;
  bool valid = (haveCfg || readLegacy(SETTINGS_FILE, SETTINGS_BLOB_MAX, cfg)) &&
               (havePrs || readLegacy(PRESETS_FILE, PRESETS_BLOB_MAX, prs));
  SystemSettings decoded{};
  decoded.max_rpm = MAX_RPM;
  if (valid && !cfg.empty()) {
    JsonDocument doc;
    valid = !deserializeJson(doc, cfg.data(), cfg.size()) && doc.is<JsonObject>() &&
            storage_decode_settings_doc(doc.as<JsonObjectConst>(), decoded);
  }
  std::vector<Preset> programs;
  if (valid && !prs.empty()) valid = storage_decode_presets_buffer(prs.data(), prs.size(), decoded.max_rpm, programs);
  if (valid) {
    xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
    const bool cfgWritten = cfg.empty() || g_prefs.putBytes(NVS_KEY_SETTINGS, cfg.data(), cfg.size()) == cfg.size();
    const bool prsWritten = cfgWritten && (prs.empty() || g_prefs.putBytes(NVS_KEY_PRESETS, prs.data(), prs.size()) == prs.size());
    if (!prsWritten && !cfg.empty() && cfgWritten) g_prefs.remove(NVS_KEY_SETTINGS);
    xSemaphoreGive(g_nvs_mutex);
    valid = cfgWritten && prsWritten;
  }
  LittleFS.end();
  if (!valid) fatal_halt("Legacy migration failed: original files retained; motion disabled");
}

bool storage_load_presets() {
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  const size_t len = g_prefs.getBytesLength(NVS_KEY_PRESETS);
  std::vector<uint8_t> buf;
  if (len > PRESETS_BLOB_MAX) { xSemaphoreGive(g_nvs_mutex); return false; }
  if (len > 0) {
    buf.resize(len);
    if (g_prefs.getBytes(NVS_KEY_PRESETS, buf.data(), len) != len) { xSemaphoreGive(g_nvs_mutex); return false; }
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

static bool storage_decode_presets_buffer(const uint8_t* data, size_t len, float rpmCap, std::vector<Preset>& loaded) {
  if (!data || !len || len > PRESETS_BLOB_MAX) return false;
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, data, len);
  if (error) {
    LOG_E("Failed to parse presets JSON from NVS: %s", error.c_str());
    return false;
  }

  if (!doc.is<JsonArray>()) return false;
  loaded.clear();
  JsonArray array = doc.as<JsonArray>();
  if (array.size() > MAX_PRESETS) return false;
  for (JsonVariantConst entry : array) {
    if (!entry.is<JsonObjectConst>()) return false;
    JsonObjectConst obj = entry.as<JsonObjectConst>();
    Preset p{};
    p.id = static_cast<uint8_t>(loaded.size() + 1); // Normalize old/duplicate IDs without dropping programs.
    const char* name = obj["name"] | "Unnamed";
    if (!program_name_valid(name, sizeof(p.name))) return false;
    strlcpy(p.name, name, sizeof(p.name));
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
    p.workpiece_diameter_mm = obj["workpiece_diameter_mm"] | 0.0f;
    p.timer_ms = obj["timer_ms"] | 5000;

    p.direction = obj["direction"] | 0;
    p.pulse_cycles = obj["pulse_cycles"] | 0;
    p.step_repeats = obj["step_repeats"] | 1;
    p.step_dwell_sec = obj["step_dwell_sec"] | 0.0f;
    p.timer_auto_stop = obj["timer_auto_stop"] | 1;
    p.cont_soft_start = obj["cont_soft_start"] | 0;

    if (rpmCap < MIN_RPM) rpmCap = MIN_RPM;
    if (rpmCap > MAX_RPM) rpmCap = MAX_RPM;
    if (!std::isfinite(p.rpm) || !std::isfinite(p.step_angle) || !std::isfinite(p.step_dwell_sec) ||
        !std::isfinite(p.workpiece_diameter_mm))
      return false;
    p.step_repeats = constrain(p.step_repeats, (uint16_t)1, (uint16_t)99);
    p.step_dwell_sec = constrain(p.step_dwell_sec, 0.0f, 30.0f);
    p.direction = p.direction == DIR_CCW ? DIR_CCW : DIR_CW;
    p.rpm = constrain(p.rpm, MIN_RPM, rpmCap);
    p.pulse_on_ms = constrain(p.pulse_on_ms, (uint32_t)PULSE_MS_MIN, (uint32_t)PULSE_MS_MAX);
    p.pulse_off_ms = constrain(p.pulse_off_ms, (uint32_t)PULSE_MS_MIN, (uint32_t)PULSE_MS_MAX);
    p.step_angle = constrain(p.step_angle, 0.1f, 360.0f);
    if (p.workpiece_diameter_mm < 1.0f || p.workpiece_diameter_mm > 20000.0f) {
      p.workpiece_diameter_mm = 0.0f;
    }
    p.timer_ms = constrain(p.timer_ms, (uint32_t)1, (uint32_t)3600000);

    loaded.push_back(p);
  }

  return true;
}

static bool storage_parse_presets_buffer(const uint8_t* data, size_t len) {
  float cap;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); cap = g_settings.max_rpm; xSemaphoreGive(g_settings_mutex);
  std::vector<Preset> loaded;
  if (!storage_decode_presets_buffer(data, len, cap, loaded)) return false;
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY); g_presets = std::move(loaded); xSemaphoreGive(g_presets_mutex);
  return true;
}

static bool storage_write_doc(const char* key, const JsonDocument& doc, size_t maximum) {
  if (doc.overflowed()) return false;
  const size_t expected = measureJson(doc);
  if (!expected || expected > maximum) return false;
  std::vector<uint8_t> buffer(expected + 1);
  if (serializeJson(doc, buffer.data(), buffer.size()) != expected) return false;
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  const bool saved = g_prefs.putBytes(key, buffer.data(), expected) == expected;
  xSemaphoreGive(g_nvs_mutex);
  return saved;
}

static bool storage_save_presets_internal() {
  std::vector<Preset> localCopy;
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  localCopy = g_presets;
  xSemaphoreGive(g_presets_mutex);

  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  if (localCopy.size() > MAX_PRESETS) return false;
  for (const auto& p : localCopy) {
    if (!program_name_valid(p.name, sizeof(p.name)) || !std::isfinite(p.rpm) ||
        !std::isfinite(p.step_angle) || !std::isfinite(p.step_dwell_sec) || !std::isfinite(p.workpiece_diameter_mm)) return false;
    JsonObject obj = array.add<JsonObject>();
    obj["id"] = p.id;
    obj["name"] = p.name;
    obj["mode"] = (int)p.mode;
    obj["mode_mask"] = p.mode_mask;
    obj["rpm"] = p.rpm;
    obj["pulse_on"] = p.pulse_on_ms;
    obj["pulse_off"] = p.pulse_off_ms;
    obj["step_angle"] = p.step_angle;
    obj["workpiece_diameter_mm"] = p.workpiece_diameter_mm;
    obj["timer_ms"] = p.timer_ms;
    obj["direction"] = p.direction;
    obj["pulse_cycles"] = p.pulse_cycles;
    obj["step_repeats"] = p.step_repeats;
    obj["step_dwell_sec"] = p.step_dwell_sec;
    obj["timer_auto_stop"] = p.timer_auto_stop;
    obj["cont_soft_start"] = p.cont_soft_start;
  }

  if (!storage_write_doc(NVS_KEY_PRESETS, doc, PRESETS_BLOB_MAX)) return false;
  LOG_I("Saved %u presets to NVS.", (unsigned)localCopy.size());
  return true;
}

bool storage_save_presets() {
  storage_request_presets_save();
  return true;
}

bool storage_load_settings() {
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  const size_t len = g_prefs.getBytesLength(NVS_KEY_SETTINGS);
  std::vector<uint8_t> buf;
  if (len > SETTINGS_BLOB_MAX) { xSemaphoreGive(g_nvs_mutex); return false; }
  if (len > 0) {
    buf.resize(len);
    if (g_prefs.getBytes(NVS_KEY_SETTINGS, buf.data(), len) != len) { xSemaphoreGive(g_nvs_mutex); return false; }
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

  if (!doc.is<JsonObject>() || !storage_apply_settings_doc(doc.as<JsonObjectConst>())) {
    return false;
  }

  LOG_I("Loaded system settings from NVS.");
  return true;
}

static int storage_sanitize_microstep(int value) {
  if (value == 4 || value == 8 || value == 16 || value == 32) {
    return value;
  }
  return 16;
}

static bool storage_decode_settings_doc(JsonObjectConst doc, SystemSettings& decoded) {
  if (!std::isfinite(doc["max_rpm"] | MAX_RPM) || !std::isfinite(doc["calibration_factor"] | 1.0f))
    return false;
  if (!doc["setup_completed"].isNull() && !doc["setup_completed"].is<bool>()) return false;
  decoded.acceleration =
      constrain(doc["acceleration"] | default_settings().acceleration, (int)1000, (int)30000);
  decoded.microstep = storage_sanitize_microstep(doc["microstep"] | 16);
  {
    float mx = doc["max_rpm"] | MAX_RPM;
    if (mx < MIN_RPM) mx = MIN_RPM;
    if (mx > MAX_RPM) mx = MAX_RPM;
    decoded.max_rpm = mx;
  }
  decoded.calibration_factor = constrain(doc["calibration_factor"] | 1.0f, 0.5f, 1.5f);
  decoded.brightness = constrain(doc["brightness"] | 150, 10, 255);
  decoded.dim_timeout = settings_dim_seconds(doc["dim_timeout"] | 60);
  decoded.dir_switch_enabled = doc["dir_switch_enabled"] | true;
  decoded.invert_direction = doc["invert_direction"] | false;
  decoded.accent_color = constrain(doc["accent_color"] | 0, 0, 7);
  decoded.color_scheme = constrain(doc["color_scheme"] | 0, 0, 1);
  decoded.countdown_seconds = constrain(doc["countdown_seconds"] | 3, 1, 10);
  // Missing JSON key: keep project default DM542T timing for older NVS blobs.
  decoded.stepper_driver = constrain(doc["stepper_driver"] | (int)STEPPER_DRIVER_DM542T, 0, 1);
  decoded.pedal_enabled = doc["pedal_enabled"] | false;
  decoded.settings_version = doc["settings_version"] | 0;
  decoded.setup_completed = setup_migration_completed(!doc["setup_completed"].isNull(), doc["setup_completed"] | false);
  return true;
}

static bool storage_apply_settings_doc(JsonObjectConst doc) {
  SystemSettings decoded{};
  if (!storage_decode_settings_doc(doc, decoded)) return false;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY); g_settings = decoded; xSemaphoreGive(g_settings_mutex);
  g_dir_switch_cache.store(decoded.dir_switch_enabled, std::memory_order_release);
  return true;
}

static bool storage_save_settings_internal() {
  SystemSettings snap;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  snap = g_settings;
  xSemaphoreGive(g_settings_mutex);

  if (!std::isfinite(snap.max_rpm) || !std::isfinite(snap.calibration_factor)) return false;
  JsonDocument doc;
  doc["acceleration"] = snap.acceleration;
  doc["microstep"] = snap.microstep;
  doc["max_rpm"] = snap.max_rpm;
  doc["calibration_factor"] = snap.calibration_factor;
  doc["brightness"] = snap.brightness;
  doc["dim_timeout"] = snap.dim_timeout;
  doc["dir_switch_enabled"] = snap.dir_switch_enabled;
  doc["invert_direction"] = snap.invert_direction;
  doc["accent_color"] = snap.accent_color;
  doc["color_scheme"] = snap.color_scheme;
  doc["countdown_seconds"] = snap.countdown_seconds;
  doc["stepper_driver"] = snap.stepper_driver;
  doc["pedal_enabled"] = snap.pedal_enabled;
  doc["settings_version"] = snap.settings_version;
  doc["setup_completed"] = snap.setup_completed;

  if (!storage_write_doc(NVS_KEY_SETTINGS, doc, SETTINGS_BLOB_MAX)) return false;
  LOG_I("Saved settings to NVS.");
  // Saving an older snapshot must not change the currently committed runtime cache.
  return true;
}

void storage_save_settings() { storage_request_settings_save(); }

void storage_flush() {
  auto write = [](SaveRequest& request, bool (*save)(), const char* label) {
    if (!request.begin(millis())) return;
    g_flashWriting.store(true);
    const bool saved = save();
    g_flashWriting.store(false);
    request.complete(saved);
    g_screenRedraw.store(true);
    event_log_addf("NVS %s %s", label, saved ? "SAVED" : "FAILED / RETRY");
  };
  write(presetsSave, storage_save_presets_internal, "PROGRAMS");
  write(settingsSave, storage_save_settings_internal, "SETTINGS");
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
  if (!used || !total) return;
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
  *total = partition ? partition->size : 0u;
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  size_t u = g_prefs.getBytesLength(NVS_KEY_SETTINGS);
  u += g_prefs.getBytesLength(NVS_KEY_PRESETS);
  xSemaphoreGive(g_nvs_mutex);
  *used = u; // Serialized payload bytes, not NVS physical occupancy.
}

bool storage_get_nvs_stats(size_t* used_entries, size_t* total_entries) {
  if (!used_entries || !total_entries) return false;
  nvs_stats_t stats{};
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  const esp_err_t result = nvs_get_stats("nvs", &stats);
  xSemaphoreGive(g_nvs_mutex);
  if (result != ESP_OK) return false;
  *used_entries = stats.used_entries;
  *total_entries = stats.total_entries;
  return true;
}

bool storage_format() {
  if (control_get_state() != STATE_IDLE || storage_status() != STORAGE_SAVED) return false;
  // This operation owns only its temporary inhibit. Fatal/restart latches
  // set by other tasks must never be cleared by a failed erase.
  bool inactive = false;
  if (!g_storageFormatting.compare_exchange_strong(inactive, true, std::memory_order_acq_rel)) return false;
  control_stop(); digitalWrite(PIN_ENA, HIGH);
  xSemaphoreTake(g_nvs_mutex, portMAX_DELAY);
  const bool cleared = g_prefs_open && g_prefs.clear();
  xSemaphoreGive(g_nvs_mutex);
  if (!cleared) {
    g_storageFormatting.store(false, std::memory_order_release);
    return false;
  }
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  g_presets.clear();
  xSemaphoreGive(g_presets_mutex);

  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings = default_settings();
  const bool dirSw = g_settings.dir_switch_enabled;
  xSemaphoreGive(g_settings_mutex);

  g_dir_switch_cache.store(dirSw, std::memory_order_release);
  // Runtime caches (motor acceleration, RPM caps, calibration, pedal state)
  // are NOT re-applied here by design: formatting requires a restart before
  // new motion. safety_inhibit_motion() holds until then.
  g_restartRequired.store(true, std::memory_order_release);
  g_storageFormatting.store(false, std::memory_order_release);
  LOG_I("Storage formatted - NVS cleared, restart required");
  return true;
}
