// BLE - Bluetooth Low Energy remote control and OTA
#include "ble.h"
#include "config.h"
#include "../control/control.h"
#include "../motor/speed.h"
#include "../safety/safety.h"
#include "../storage/storage.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include "esp32-hal-hosted.h"
#include "esp_hosted_ota.h"
#include "HTTPClient.h"
#include "NetworkClientSecure.h"
#include "WiFi.h"
#include <atomic>

// ───────────────────────────────────────────────────────────────────────────────
// BLE UUIDs (custom for TIG Rotator)
// ───────────────────────────────────────────────────────────────────────────────
#define ROTATOR_SERVICE_UUID          "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_TX_UUID                  "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_RX_UUID                  "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHAR_RPM_UUID                 "a07e7e8e-3b0c-4f1a-8d5b-2c9f4e6a1b3e"
#define CHAR_DIRECTION_UUID           "a07e7e8e-3b0c-4f1a-8d5b-2c9f4e6a1b40"

// ───────────────────────────────────────────────────────────────────────────────
// BLE globals
// ───────────────────────────────────────────────────────────────────────────────
static BLEServer* bleServer = nullptr;
static BLECharacteristic* rpmChar = nullptr;
static BLECharacteristic* stateChar = nullptr;
static BLECharacteristic* dirChar = nullptr;
static BLECharacteristic* cmdChar = nullptr;
static std::atomic<bool> bleConnected{false};
static std::atomic<bool> bleEnabled{true};
static std::atomic<bool> bleArmed{false};
static std::atomic<bool> pendingArm{false};

static BLEDeviceInfo bleScanResults[BLE_MAX_DEVICES];
static int bleScanCount = 0;
static bool bleScanRunning = false;

static volatile uint8_t blePendingCmd = 0;
static volatile float blePendingRpm = 0;
static volatile bool blePendingRpmFlag = false;
static volatile bool blePendingDirFlag = false;
static volatile uint8_t blePendingDir = 0;
volatile bool bleScanPending = false;
volatile bool bleScanDone = false;
volatile bool bleEnablePending = false;
volatile bool bleEnableValue = false;
static uint32_t lastNotifyTime = 0;
static float lastNotifiedRpm = -1;
static uint8_t lastNotifiedState = 255;
static uint8_t lastNotifiedDir = 255;
#if DEBUG_BUILD
static volatile uint8_t bleRxByte = 0;
static volatile bool bleRxFlag = false;
#endif
static uint32_t bleArmTime = 0;

class SecurityCallbacks : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 123456; }
  void onAuthenticationComplete(ble_gap_conn_desc*) override {}
};
static SecurityCallbacks securityCb;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    if (bleScanCount >= BLE_MAX_DEVICES) return;
    String name = advertisedDevice.getName().c_str();
    if (name.length() == 0) name = "Unknown";
    strlcpy(bleScanResults[bleScanCount].name, name.c_str(), sizeof(bleScanResults[bleScanCount].name));
    strlcpy(bleScanResults[bleScanCount].addr, advertisedDevice.getAddress().toString().c_str(),
             sizeof(bleScanResults[bleScanCount].addr));
    bleScanResults[bleScanCount].rssi = advertisedDevice.getRSSI();
    bleScanCount++;
  }
};

static ScanCallbacks scanCb;

// ───────────────────────────────────────────────────────────────────────────────
// Server callbacks — track connection state
// ───────────────────────────────────────────────────────────────────────────────
class RotatorServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    LOG_I("BLE client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    LOG_I("BLE client disconnected");
  }
};
static RotatorServerCallbacks serverCb;

// ───────────────────────────────────────────────────────────────────────────────
// Command characteristic callbacks
// Write values: 0x01=START, 0x02=STOP, 0x03=CW, 0x04=CCW
// ───────────────────────────────────────────────────────────────────────────────
class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    String val = pChar->getValue();
    int len = val.length();
    if (len == 0) return;
    uint8_t cmd = (uint8_t)val[0];
#if DEBUG_BUILD
    bleRxByte = cmd;
    bleRxFlag = true;
#endif
    if (cmd == 0x00) pendingArm = true;
    else if (cmd == 'F') blePendingCmd = 0x01;
    else if (cmd == 'S') blePendingCmd = 0x02;
    else if (cmd == 'R') blePendingCmd = 0x03;
    else if (cmd == 'L') blePendingCmd = 0x04;
    else if (cmd == 'B') blePendingCmd = 0x05;
    else if (cmd == 'X') blePendingCmd = 0x02;
  }
};
static CommandCallbacks cmdCb;

// ───────────────────────────────────────────────────────────────────────────────
// RPM characteristic callbacks
// Write: 4-byte float (target RPM)
// ───────────────────────────────────────────────────────────────────────────────
class RPMCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    size_t len = pChar->getLength();
    const uint8_t* data = pChar->getData();
    if (!data || len < 4) return;
    float rpm;
    memcpy(&rpm, data, 4);
    if (isnan(rpm) || isinf(rpm)) return;
    if (rpm < MIN_RPM) rpm = MIN_RPM;
    if (rpm > MAX_RPM) rpm = MAX_RPM;
    LOG_I("BLE RPM: %.3f", rpm);
    blePendingRpm = rpm;
    blePendingRpmFlag = true;
  }
};
static RPMCallbacks rpmCb;

// ───────────────────────────────────────────────────────────────────────────────
// Init
// ───────────────────────────────────────────────────────────────────────────────
void ble_init() {
  LOG_I("Initializing BLE...");

  bleEnabled.store(g_settings.ble_enabled, std::memory_order_relaxed);

  if (!BLEDevice::init(BLE_DEVICE_NAME)) {
    LOG_E("BLE init failed — C6 co-processor may not be responding");
    return;
  }
  LOG_I("BLE device initialized: %s", BLE_DEVICE_NAME);

  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK);
  BLEDevice::setSecurityCallbacks(&securityCb);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(&serverCb);
  bleServer->advertiseOnDisconnect(true);

  BLEService* service = bleServer->createService(ROTATOR_SERVICE_UUID);

  // NUS RX (6E400003) - app writes commands here
  cmdChar = service->createCharacteristic(
    CHAR_RX_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  cmdChar->setCallbacks(&cmdCb);

  // NUS TX (6E400002) - ESP32 sends state to app (read + notify only)
  stateChar = service->createCharacteristic(
    CHAR_TX_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  uint8_t initState = (uint8_t)control_get_state();
  stateChar->setValue(&initState, 1);

  // RPM characteristic (read + write + notify)
  rpmChar = service->createCharacteristic(
    CHAR_RPM_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  rpmChar->setCallbacks(&rpmCb);
  float initRpm = speed_get_target_rpm();
  rpmChar->setValue((uint8_t*)&initRpm, 4);

class DirectionCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    size_t len = pChar->getLength();
    const uint8_t* data = pChar->getData();
    if (!data || len < 1) return;
    uint8_t dir = data[0];
    if (dir <= 1) {
      blePendingDir = dir;
      blePendingDirFlag = true;
    }
  }
};
static DirectionCallbacks dirCb;

// Direction characteristic (read + write + notify)
  dirChar = service->createCharacteristic(
    CHAR_DIRECTION_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  dirChar->setCallbacks(&dirCb);
  uint8_t initDir = (uint8_t)speed_get_direction();
  dirChar->setValue(&initDir, 1);

  service->start();

  if (bleEnabled.load(std::memory_order_relaxed)) {
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(ROTATOR_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    LOG_I("BLE advertising started");
  } else {
    LOG_I("BLE initialized but disabled (saved setting)");
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// Update (call periodically to push state changes to connected clients)
// ───────────────────────────────────────────────────────────────────────────────
void ble_update() {
  if (!bleServer) return;

  if (bleEnablePending) {
    bleEnablePending = false;
    ble_set_enabled(bleEnableValue);
  }

  if (bleScanPending) {
    bleScanPending = false;
    ble_scan_start();
    bleScanDone = true;
  }

  #if DEBUG_BUILD
  if (bleRxFlag) {
    bleRxFlag = false;
    Serial.print("BLE RX: ");
    Serial.print(bleRxByte);
    Serial.print(" (0x");
    Serial.print(bleRxByte, HEX);
    Serial.println(")");
  }
#endif

  if (blePendingRpmFlag) {
    blePendingRpmFlag = false;
    speed_slider_set(blePendingRpm);
  }

  if (blePendingDirFlag) {
    blePendingDirFlag = false;
    speed_set_direction((Direction)blePendingDir);
  }

  if (pendingArm.load(std::memory_order_relaxed)) {
    pendingArm.store(false, std::memory_order_relaxed);
    bleArmed.store(true, std::memory_order_relaxed);
    bleArmTime = millis();
  }

  if (blePendingCmd) {
    uint8_t cmd = blePendingCmd;
    blePendingCmd = 0;
#if DEBUG_BUILD
    Serial.print("BLE CMD: ");
    Serial.println(cmd);
#endif
    if (safety_is_estop_active()) return;
    if (cmd == 0x01 || cmd == 0x05 || cmd == 0x03 || cmd == 0x04) {
      if (!bleArmed.load(std::memory_order_relaxed) || millis() - bleArmTime > 5000) {
        bleArmed.store(false, std::memory_order_relaxed);
        return;
      }
      bleArmed.store(false, std::memory_order_relaxed);
    }
    switch (cmd) {
      case 0x01:
        if (control_get_state() == STATE_IDLE) control_start_continuous();
        break;
      case 0x02:
        if (control_get_state() != STATE_IDLE && control_get_state() != STATE_ESTOP)
          control_stop();
        break;
      case 0x03:
        speed_set_direction(DIR_CW);
        break;
      case 0x04:
        speed_set_direction(DIR_CCW);
        break;
      case 0x05:
        speed_set_direction(DIR_CCW);
        if (control_get_state() == STATE_IDLE) control_start_continuous();
        break;
    }
  }

  if (!bleConnected.load(std::memory_order_relaxed)) return;

  uint32_t now = millis();
  if (now - lastNotifyTime < 500) return;
  lastNotifyTime = now;

  float rpm = speed_get_target_rpm();
  if (rpm != lastNotifiedRpm) {
    lastNotifiedRpm = rpm;
    rpmChar->setValue((uint8_t*)&rpm, 4);
    rpmChar->notify();
  }

  uint8_t st = (uint8_t)control_get_state();
  if (st != lastNotifiedState) {
    lastNotifiedState = st;
    stateChar->setValue(&st, 1);
    stateChar->notify();
  }

  uint8_t dir = (uint8_t)speed_get_direction();
  if (dir != lastNotifiedDir) {
    lastNotifiedDir = dir;
    dirChar->setValue(&dir, 1);
    dirChar->notify();
  }
}

bool ble_is_connected() {
  return bleConnected.load(std::memory_order_relaxed);
}

void ble_notify_state() {
  if (!bleConnected.load(std::memory_order_relaxed) || !stateChar) return;
  uint8_t st = (uint8_t)control_get_state();
  stateChar->setValue(&st, 1);
  stateChar->notify();
}

// ───────────────────────────────────────────────────────────────────────────────
// C6 co-processor OTA update
// Connects WiFi via C6, downloads firmware from Espressif, flashes C6
// BLE is paused during OTA — both share the SDIO bus to C6 co-processor
// ───────────────────────────────────────────────────────────────────────────────
void ble_ota_update_c6() {
  LOG_I("=== C6 OTA Update Start ===");

  if (!hostedIsInitialized()) {
    LOG_E("ESP-Hosted not initialized — cannot OTA");
    return;
  }

  // Pause BLE — WiFi and BLE share the SDIO bus to the C6 co-processor.
  // Running both simultaneously causes SDIO bus contention and dropped packets.
  bool wasAdvertising = bleEnabled.load(std::memory_order_relaxed) && bleServer;
  if (wasAdvertising) {
    LOG_I("Pausing BLE for OTA (shared SDIO bus)");
    BLEDevice::stopAdvertising();
    if (bleConnected.load(std::memory_order_relaxed)) {
      bleServer->disconnect(0);
    }
  }

  char* updateUrl = hostedGetUpdateURL();
  if (!updateUrl || strlen(updateUrl) == 0) {
    LOG_E("No OTA update URL available");
    if (wasAdvertising) {
      BLEDevice::startAdvertising();
      LOG_I("BLE resumed (no OTA URL)");
    }
    return;
  }

  LOG_I("OTA URL: %s", updateUrl);

  LOG_I("Connecting WiFi via C6...");
  WiFi.STA.begin();
  if (g_settings.wifi_ssid[0] != '\0') {
    WiFi.STA.connect(g_settings.wifi_ssid, g_settings.wifi_pass);
  } else {
    LOG_E("No WiFi credentials configured - cannot OTA");
    return;
  }

  uint32_t wifiStart = millis();
  while (WiFi.STA.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(10);
    yield();
  }

  if (WiFi.STA.status() != WL_CONNECTED) {
    LOG_E("WiFi connection failed — cannot download OTA");
    WiFi.STA.disconnect();
    return;
  }

  LOG_I("WiFi connected! IP: %s", WiFi.STA.localIP().toString().c_str());

  NetworkClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!https.begin(client, updateUrl)) {
    LOG_E("HTTPS begin failed");
    WiFi.STA.disconnect();
    return;
  }

  int httpCode = https.GET();
  LOG_I("HTTP response: %d", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    LOG_E("HTTP GET failed: %d", httpCode);
    https.end();
    WiFi.STA.disconnect();
    return;
  }

  int contentLen = https.getSize();
  LOG_I("Firmware size: %d bytes", contentLen);

  if (!hostedBeginUpdate()) {
    LOG_E("hostedBeginUpdate failed");
    https.end();
    WiFi.STA.disconnect();
    return;
  }

  LOG_I("Writing firmware to C6...");
  uint8_t* buff = (uint8_t*)malloc(4096);
  if (!buff) {
    LOG_E("OTA: failed to allocate buffer");
    https.end();
    WiFi.STA.disconnect();
    return;
  }
  int totalWritten = 0;
  WiFiClient* stream = https.getStreamPtr();

  while (contentLen > 0 && totalWritten < contentLen) {
    int readNow = stream->readBytes(buff, min((size_t)contentLen - totalWritten, (size_t)4096));
    if (readNow <= 0) {
      delay(10);
      continue;
    }
    if (!hostedWriteUpdate(buff, readNow)) {
      LOG_E("hostedWriteUpdate failed at %d bytes", totalWritten);
      free(buff);
      https.end();
      WiFi.STA.disconnect();
      return;
    }
    totalWritten += readNow;
    if (totalWritten % 32768 == 0) {
      LOG_I("  OTA progress: %d / %d bytes", totalWritten, contentLen);
    }
  }

  LOG_I("OTA write complete: %d bytes", totalWritten);
  free(buff);

  if (!hostedEndUpdate()) {
    LOG_E("hostedEndUpdate failed");
    https.end();
    WiFi.STA.disconnect();
    return;
  }
  LOG_I("OTA end OK");

  if (!hostedActivateUpdate()) {
    LOG_E("hostedActivateUpdate failed");
    https.end();
    WiFi.STA.disconnect();
    return;
  }

  LOG_I("=== C6 OTA Update SUCCESS! C6 rebooting ===");

  https.end();
  WiFi.STA.disconnect();
  WiFi.mode(WIFI_OFF);

  // Resume BLE after WiFi is off — SDIO bus is free again
  if (wasAdvertising) {
    delay(500);  // Wait for C6 to reboot and SDIO to settle
    BLEDevice::startAdvertising();
    LOG_I("BLE resumed after OTA");
  }
}

void ble_set_enabled(bool enabled) {
  bleEnabled.store(enabled, std::memory_order_relaxed);
  g_settings.ble_enabled = enabled;
  storage_save_settings();
  if (bleServer) {
    if (enabled) {
      BLEDevice::startAdvertising();
      LOG_I("BLE advertising started");
    } else {
      BLEDevice::stopAdvertising();
      if (bleConnected.load(std::memory_order_relaxed)) {
        bleServer->disconnect(0);
        bleConnected.store(false, std::memory_order_relaxed);
      }
      LOG_I("BLE stopped");
    }
  }
}

bool ble_is_enabled() {
  return bleEnabled.load(std::memory_order_relaxed);
}

void ble_scan_start() {
  if (!bleEnabled.load(std::memory_order_relaxed) || bleScanRunning) return;
  bleScanCount = 0;
  memset(bleScanResults, 0, sizeof(bleScanResults));
  bleScanRunning = true;
  LOG_I("BLE scan starting...");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(&scanCb, false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(2000, false);
  bleScanRunning = false;
  LOG_I("BLE scan done: %d devices", bleScanCount);
}

int ble_scan_get_results(BLEDeviceInfo* out, int max) {
  int count = (bleScanCount < max) ? bleScanCount : max;
  memcpy(out, bleScanResults, count * sizeof(BLEDeviceInfo));
  return count;
}

bool ble_scan_is_running() {
  return bleScanRunning;
}
