# File maintenance review — updated 2026-10-03

GitHub displays the last commit that changed each file. A six-month-old date does not indicate that the file is missing from the current build or release.

The v2.1.1 update includes the complete source tree, not only files with recent dates. Firmware builds and regression tests compile and exercise older files alongside the updated implementation. This does not imply every line or physical hardware behavior is covered by tests.

## Follow-up corrections

- Editor settings no longer contain a contributor's personal Python or PlatformIO installation path.
- Editor tasks include release, debug, mirror and native tests. Windows uses the standard PlatformIO installation; other systems use `pio` on PATH.
- The simulator launcher stops immediately when CMake configuration fails.
- The old browser demo is explicitly marked as an offline historical concept, with no device connection or Wi-Fi support.
- May USB mirror plans are marked historical and link to the implemented viewer guide.
- EMI guidance distinguishes historical field experience from qualification of the current firmware.

The subsequent v2.1.1 migration updates dependency pins, driver metadata, control/calibration/program behavior, current process/wiki/issue guidance and release packaging. Historical reports and original font/hardware provenance remain dated records. See the [dependency report](DEPENDENCY_UPGRADE_2026-10-02.md) and [1.4.0 re-audit](FASTACCELSTEPPER_1_4_REAUDIT.md).

## Older files retained

| Area | Reason |
| --- | --- |
| `LICENSE` | Existing MIT license and 2026 copyright remain applicable. |
| `lib/esp_lcd_*` | Vendored upstream drivers retain original source, notices and component metadata. |
| Hardware photographs and wiring illustrations | Describe the existing hardware; the V5 UI update does not replace the wiring. |
| Simulator stubs and configuration | Still used by the actual LVGL simulator and its regression checks. |
| Unchanged motor helpers, headers and tests | Part of the current source and build; changes are made when behavior requires them. |
| `CODEOWNERS` and issue configuration | Ownership and issue routing remain valid. |
| Earlier UI concepts and validation logs | Historical records; current UI images are in `docs/images/ui_runtime_v5/`. |

Files are not rewritten solely to reset their GitHub timestamp. Current release documentation is in [v2.1.1.md](releases/v2.1.1.md), with installation instructions in [FLASHING.md](releases/FLASHING.md).
