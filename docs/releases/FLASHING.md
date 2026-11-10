# Flashing v2.4.0

Target: GUITION JC4880P443C ESP32-P4, 16 MB flash, 800 × 480 landscape display.

> [!WARNING]
> Use only one firmware variant at a time. Keep motor/driver power off during flashing and initial input checks.

## Recommended: build from the release source

Install PlatformIO, clone the project, check out `v2.4.0`, then run:

```sh
pio device list
pio run -e esp32p4-release -t upload --upload-port COM3
```

Replace COM3 with the detected port. Select `esp32p4-debug` or `esp32p4-mirror` for the other variants. PlatformIO supplies the correct bootloader, partition table and application offsets.

| Variant | Environment |
|---|---|
| Release | `esp32p4-release` |
| Debug | `esp32p4-debug` |
| USB mirror | `esp32p4-mirror` |

## Use the downloaded flashing bundle

Extract the ZIP, install esptool 5.x (`python -m pip install "esptool>=5,<6"`), and run these commands from the extracted directory. Replace COM3 with your port.

### Update the application only

For a controller already using this project's matching partition layout, update only the application to preserve settings and OTA selection:

```sh
python -m esptool --chip esp32p4 --port COM3 write-flash 0x10000 firmware.bin
```

This writes the first app slot. If another OTA slot is selected, use the source/PlatformIO installation path or a complete installation below to initialize OTA selection. Confirm the version shown by the device afterwards.

### First installation

For a first installation or when deliberately replacing the partition layout:

```sh
python -m esptool --chip esp32p4 --port COM3 write-flash --flash-size 16MB 0x2000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

| Offset | File |
|---|---|
| `0x2000` | `bootloader.bin` |
| `0x8000` | `partitions.bin` |
| `0xe000` | `boot_app0.bin` |
| `0x10000` | `firmware.bin` |

> [!IMPORTANT]
> Standalone application files from the release page contain only `firmware.bin`. They are not merged full-flash images. Flash `firmware.bin` at `0x10000`, never at `0x0`.

The bundle's partition table:

| Region | Offset | Size |
|---|---|---|
| NVS | `0x9000` | `0x5000` bytes |
| OTA metadata | `0xe000` | |
| app0 | `0x10000` | `0x640000` bytes |
| app1 | `0x650000` | `0x640000` bytes |
| SPIFFS | `0xc90000` | |
| coredump | `0xff0000` | |

`default_16MB.csv` documents the same layout; generated `partitions.bin` is authoritative.

> [!WARNING]
> Replacing a different layout may require resetting stored configuration. No filesystem upload is needed for settings and presets.

The ZIP also includes the bootloader, partition table, OTA initialization image, build metadata, license notices and checksums. Keep the accompanying `LICENSES.txt` with distributed firmware.

## After flashing

Confirm v2.4.0 appears on the About screen. Then work through the checklist before enabling motor power.

- [ ] Display and touch work
- [ ] GPIO34 `HIGH` is healthy, `LOW` is fault
- [ ] Driver alarm state
- [ ] ENA disable polarity
- [ ] Pedal release
- [ ] Stored motor settings

> [!IMPORTANT]
> Test physical stop behavior on the assembled machine.

> [!NOTE]
> The mirror variant requires <kbd>Settings</kbd> › <kbd>Display settings</kbd> › USB remote control to be armed on the physical display before accepting PC input.

## Windows log encoding

If redirecting PlatformIO/esptool output to a file, enable UTF-8 first so progress characters do not break the upload log reader:

```powershell
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"
pio run -e esp32p4-release -t upload --upload-port COM3
```
