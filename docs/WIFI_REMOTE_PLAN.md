# WiFi & BLE Remote Control — Implementation Status

> **STATUS: IMPLEMENTED (v2.0.0)**
> WiFi and BLE are implemented via ESP-Hosted using the on-board ESP32-C6 co-processor.

## Architecture

The GUITION JC4880P443C board has both an ESP32-P4 and an ESP32-C6 on the same PCB. They communicate via SDIO (GPIO 14-19, 54). This transport is shared between WiFi and BLE.

```
Phone App <--BLE/WiFi--> ESP32-C6 <--SDIO--> ESP32-P4
                                            |
                                       MIPI-DSI Display
                                       Motor Control
```

## WiFi (ESP-Hosted)

- **Mode**: STA (connects to existing networks)
- **Init**: `WiFi.begin()` must be called before `BLEDevice::init()`
- **Thread safety**: All WiFi API calls via `wifi_process_pending()` in storageTask (not from UI thread)
- **Scanning**: Async `WiFi.scanNetworks(true)`, results displayed in UI
- **Polling**: WiFi.status() every 2s to avoid SDIO blocking
- **Credentials**: Stored in /settings.json, configurable from WiFi settings screen
- **Hidden networks**: Supported via manual SSID entry

## BLE (NimBLE)

- **Stack**: NimBLE via Arduino BLE (baked into arduino-esp32 3.3.x)
- **Service**: Custom service with Nordic UART Service (NUS) compatible characteristics
- **Security**: MITM bonding with passkey 123456
- **Rate limiting**: Notify at 500ms max to avoid saturating shared SDIO bus
- **Commands**: Arm (0x00), Start (F), Stop (S/X), CW (R), CCW (L), Reverse+Start (B)
- **Pending flags**: Write callbacks set volatile flags, processed in `ble_update()` on storageTask

## Original Plan (Archived)

The original plan envisioned a separate ESP32-C6 board connected via UART with a web app. This was superseded by the on-board C6 co-processor with ESP-Hosted, which provides both WiFi and BLE through the existing SDIO transport — no external board needed.

See [FUTURE_ARCHITECTURE.md](FUTURE_ARCHITECTURE.md) for the original UART/web app plan.
