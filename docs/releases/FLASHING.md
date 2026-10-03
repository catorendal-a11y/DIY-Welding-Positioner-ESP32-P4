# Flashing v2.1.1

Target: GUITION JC4880P443C ESP32-P4, 16 MB flash, 800 × 480 landscape display. Use only one firmware variant at a time. Keep motor/driver power off during flashing and initial input checks.

## Recommended: build from the release source

Install PlatformIO, clone the project, check out `v2.1.1`, then run:

```sh
pio device list
pio run -e esp32p4-release -t upload --upload-port COM3
```

Replace COM3 with the detected port. Select `esp32p4-debug` or `esp32p4-mirror` for the other variants. PlatformIO supplies the correct bootloader, partition table and application offsets.

## Use the downloaded flashing bundle

Extract the ZIP, install esptool 5.x (`python -m pip install "esptool>=5,<6"`), and run these commands from the extracted directory. Replace COM3 with your port.

For a controller already using this project's matching partition layout, update only the application to preserve settings and OTA selection:

```sh
python -m esptool --chip esp32p4 --port COM3 write-flash 0x10000 firmware.bin
```

This writes the first app slot. If another OTA slot is selected, use the source/PlatformIO installation path or a complete installation below to initialize OTA selection. Confirm the version shown by the device afterwards.

For a first installation or when deliberately replacing the partition layout:

```sh
python -m esptool --chip esp32p4 --port COM3 write-flash --flash-size 16MB 0x2000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

The bundle's partition table has NVS at 0x9000 (0x5000 bytes), OTA metadata at 0xe000, app0 at 0x10000 (0x640000 bytes), app1 at 0x650000 (0x640000 bytes), SPIFFS at 0xc90000 and coredump at 0xff0000. `default_16MB.csv` documents the same layout; generated `partitions.bin` is authoritative. Replacing a different layout may require resetting stored configuration. No filesystem upload is needed for settings and presets.

Standalone application files from the release page contain only `firmware.bin`; they are not merged full-flash images. The ZIP also includes the bootloader, partition table, OTA initialization image, build metadata, license notices and checksums. Keep the accompanying `LICENSES.txt` with distributed firmware.

## After flashing

Confirm v2.1.1 appears on the About screen. Check display/touch, GPIO34 HIGH healthy / LOW fault, driver alarm state, ENA disable polarity, pedal release and stored motor settings before enabling motor power. Test physical stop behavior on the assembled machine. The mirror variant requires Settings > Display > USB MIRROR to be armed on the physical display before accepting PC input.
