// BLE - Disabled (WiFi and BLE removed)
#include "ble.h"

void ble_init() {}
void ble_update() {}
bool ble_is_connected() { return false; }
void ble_notify_state() {}
void ble_ota_update_c6() {}
void ble_set_enabled(bool) {}
bool ble_is_enabled() { return false; }
void ble_update_name(const char*) {}
void ble_scan_start() {}
int ble_scan_get_results(BLEDeviceInfo*, int) { return 0; }
bool ble_scan_is_running() { return false; }

volatile bool bleScanPending = false;
volatile bool bleScanDone = false;
volatile bool bleEnablePending = false;
volatile bool bleEnableValue = false;
volatile bool bleNameUpdatePending = false;
