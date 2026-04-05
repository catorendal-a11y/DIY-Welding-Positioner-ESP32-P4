#pragma once

#include <Arduino.h>

void ble_init();
void ble_update();
bool ble_is_connected();
void ble_notify_state();
void ble_ota_update_c6();
void ble_set_enabled(bool enabled);
bool ble_is_enabled();

// Pending flags for deferred BLE operations (set from UI, processed by ble_update)
extern volatile bool bleScanPending;
extern volatile bool bleScanDone;
extern volatile bool bleEnablePending;
extern volatile bool bleEnableValue;

typedef struct {
  char name[33];
  char addr[18];
  int rssi;
} BLEDeviceInfo;

#define BLE_MAX_DEVICES 10

void ble_scan_start();
int ble_scan_get_results(BLEDeviceInfo* out, int max);
bool ble_scan_is_running();
