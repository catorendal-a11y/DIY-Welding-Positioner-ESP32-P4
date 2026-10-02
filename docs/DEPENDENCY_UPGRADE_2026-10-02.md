# Dependency upgrade and migration analysis

Verified 2 October 2026 for unreleased source v2.1.1. The published v2.1.0 binaries and the connected device have not been replaced by this update. The existing V5 design, physical E-STOP wiring and saved presets/settings are retained.

## Dependency inventory

| Dependency | Previous project version | Selected version / source |
|---|---|---|
| PlatformIO Core | Local 6.1.19; CI unpinned | 6.2.0 in `requirements-dev.txt` |
| pioarduino platform | 55.03.37 | 55.03.312-1 |
| Arduino ESP32 / ESP-IDF | Supplied by the previous platform | 3.3.12 / 5.5.5, supplied together by the new platform |
| LVGL | 9.5.0 | 9.6.0 upstream tag |
| FastAccelStepper | 0.33.14 | 1.4.0 upstream commit `f24a65996bdc501a0497cdca6a636706003333e7` |
| ArduinoJson | 7.4.3 | 7.4.3; already current |
| PlatformIO native platform | Unpinned | 1.2.1; current stable |
| Unity | PlatformIO registry 2.6.1 | Upstream 2.7.0 commit `b6763fbd9cedfacaa89e2ad9fd00d615a234e355` |
| Vendored ST7701 | 2.0.2 | 2.0.2~2; upstream metadata/docs revision, driver code unchanged |
| Vendored esp_lcd_touch | 1.2.1 | 1.2.1; already current |
| Vendored GT911 | 1.2.0~1 | 1.2.1 |
| Vendored EK79007 | 2.0.2 | 2.0.2~1; metadata revision, auxiliary panel driver |
| SDL2 on the local Windows toolchain | 2.32.10 | 2.32.10; current compatible SDL2 series |

The new platform supplies GCC 14.2.0+20260121, GDB 17.1+20260402 and esptool 5.4.0. Its framework and tool versions are a tested bundle; independently forcing ESP-IDF 6 or a different cross-compiler is outside this upgrade. Linux CI installs its distro SDL2 package; Windows CI installs MSYS2's SDL2 package. SDL3 has a different API and is not the driver used by LVGL 9.6 here. Toolchain packages are platform-managed rather than separately overridden.

Sources: [pioarduino platform release](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.312-1), [LVGL 9.6.0](https://github.com/lvgl/lvgl/releases/tag/v9.6.0), [ArduinoJson 7.4.3](https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3), [Unity 2.7.0](https://github.com/ThrowTheSwitch/Unity/releases/tag/v2.7.0), [SDL2 release](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10), [ST7701 registry](https://components.espressif.com/components/espressif/esp_lcd_st7701/versions/2.0.2~2/readme), [GT911 registry](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911/versions/1.2.1/readme), [EK79007 registry](https://components.espressif.com/components/espressif/esp_lcd_ek79007/versions/2.0.2~1/readme).

## Why FastAccelStepper uses Git, not the registry

The PlatformIO registry artifact labeled **1.4.0** and the upstream **1.4.0 Git tag** contain different code. The registry artifact calls `rmt_new_sync_manager()` with three arguments and refers to nonexistent add/delete-channel functions. All three firmware builds failed against the actual IDF 5.5.5 headers.

The pinned upstream commit uses `rmt_sync_manager_config_t`, gates hardware synchronization by SoC support and creates the manager on demand. Ordinary single-axis channel connection does not register an experimental manager. The upstream commit also contains the corrected IDF5/6 RMT fill encoder; the registry changelog only identifies the earlier extra-step bug. The dependency now downloads that exact upstream commit without local library patches, API stubs or disabled RMT functionality.

Sources: [pinned RMT implementation](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/pd_esp32/esp32_queue.cpp), [pinned changelog](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/CHANGELOG.md), [Espressif RMT synchronization contract](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32p4/api-reference/peripherals/rmt.html).

## FastAccelStepper 0.33.14 → 1.4.0

Both upstream snapshots were inspected. Every library method used by the application was checked against the pinned header and implementation; private-library includes were searched across firmware, simulator and tests.

| Change across the versions | Effect on this project / implementation |
|---|---|
| 0.34.0 reorganizes queue, ramp and platform sources | Application uses the public `FastAccelStepper.h`; no private include migration is required. Timing documentation now points to `pd_esp32/pd_config.h`. |
| 1.0.0 changes internal log2/ramp representation and adds I2S backends | Workpiece RPM/angle conversion remains in the application. Select `FasDriver::RMT` explicitly so allocation failure cannot select I2S silently. A null result still stops initialization. |
| 1.1.0 rewrites external DIR callbacks and queue retry behavior | DIR is a direct GPIO here. No external callback API is used. Single control-task ownership remains in place. |
| 1.2.4 changes internal `init()` from bool to void and allocation initialization | Application calls `engine.init(0)` and checks `stepperConnectToPin()`; it never calls private stepper initialization. No invented success check is added to a void method. |
| 1.2.5 adds driver-type introspection | Explicit RMT selection replaces reliance on the default selection order. |
| 1.2.8–1.3.3 revise `moveTimed()` capacity, pause reporting and direction retries | This machine uses the high-level ramped `move()`, not `moveTimed()`/`addQueueEntry()`. The library handles pause retries internally. No application-level retry loop or duplicate move is introduced. |
| 1.3.0 consistently enforces direction-change delay | Keep the requested 200 µs DM542T DIR delay. The new RMT implementation adds its own pipeline-drain pause before DIR changes. Timing configuration is rejected with ENA inhibited if the queue is still running. |
| 1.3.1 extends PCNT attachment | No feedback counter is attached. The IDF5 configuration disables the library PCNT helper on IDF ≥5.5; this upgrade does not promise hardware pulse feedback. |
| 1.3.4 exposes driver drain budgets | Current IDF5 RMT implementation drains up to `3 * RMT_BLOCK_TICKS` = 24,000 ticks at 16 MHz (1.5 ms) before DIR change, in addition to the configured after-change delay. Bench measurements remain necessary. |
| 1.4.0 replaces the IDF5/6 RMT fill encoder and fixes sporadic extra steps upstream | Use the corrected Git implementation and run its actual encoder regression on the PC. This is upstream/host evidence, not a measurement on this board and driver. |
| 1.4.0 adds `FasNAxis`, `FasTimed`, synchronized starts and stop causes | The controller has one axis and does not use these experimental planners. Normal ramped moves and physical ENA inhibition remain the control path. |

### API and behavior checks

- `runForward()`, `runBackward()` and `move()` return `MoveResultCode`. This typed API already existed in 0.33.14. The application now names `MoveResultCode::OK`; host adapters use the enum instead of `int8_t`, preventing tests from accepting invalid integer-result assumptions.
- `setSpeedInHz()`, `setSpeedInMilliHz()` and `setAcceleration()` still return `int8_t`; errors remain faulted. `applySpeedAcceleration()` and `setLinearAcceleration()` remain void. Live speed updates still apply staged speed/acceleration explicitly. `stopMove()` does not apply a newly staged acceleration.
- `forceStop()` remains asynchronous: it prevents further queue filling and lets queued output drain. The old wrapper treated the call as completed immediately. It now keeps cleanup pending until `isRunning()` becomes false, so fault reset cannot release ENA prematurely. Control retries without waiting inside the library. Physical E-STOP/driver-alarm inhibition stays independent of queue cleanup.
- `isRunning()` includes the driver, active ramp and nonempty queue. Smooth STOP, Pulse OFF timing and Step completion continue waiting on it. `getCurrentSpeedInMilliHz()` and `getCurrentPosition()` are const-qualified in the new API; calling them through the existing pointer remains valid.
- `getCurrentPosition()` can lead the emitted pulses while moving because RMT buffers work ahead. The Step header identifies estimated angle progress. Completed moves still require the driver to stop. Position/RPM are not encoder measurements.
- Maximum RPM, microstep choices, calibration factors, ENA polarity and saved-data formats are unchanged. The library's internal rate limits remain in force; no unsupported pulse-width define or absolute-rate override is added.

Source: [pinned public API](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/FastAccelStepper.h). These findings separate methods we use from unrelated platform and experimental planner changes.

## LVGL 9.6 and display migration

- Set `LV_COLOR_FORMAT_DEFAULT` to `LV_COLOR_FORMAT_RGB565`, replacing the old color-depth configuration. Physical display format, buffer alignment and rotation remain unchanged.
- Use public snapshot/SDL headers under `include/lvgl/`, replacing the old private `src/` paths.
- Replace 85 deprecated object flag setters with dedicated setters for visibility, clickability, scrolling and overflow. The stale-control marker uses a user state with its supported setter; motion controls remain disabled while stale.
- Select the SDL software renderer explicitly, avoiding automatic-backend assumptions.
- Keep the generated 104 px numeric font and its original provenance/OFL notice. Both firmware and simulator compile it with 9.6.
- Include the official `esp_ldo_regulator.h`; remove handwritten LDO types that conflict with Arduino 3.3.12 and omit SDK fields.
- Build GT911 I2C configuration through explicit C++ field assignments. Component 1.2.1's C macro orders a designated field differently from IDF 5.5.5; the board's existing 400 kHz configuration is preserved.
- Update firmware/simulator packages and packaging tests for LVGL's moved `scripts/generators/built_in_font` license directory.
- The simulator test pump observes elapsed time after the last render/delay before checking control deadlines, removing frame-speed-dependent STOP-test failures.

## New-feature review and adoption

The release notes and installed source were reviewed for applicability, not just version compatibility. Useful application changes are enabled; features for other hardware or a different product architecture are recorded below.

| Updated area | Adopted improvement / applicability |
|---|---|
| LVGL 9.6 software rendering | Updated blending/transform implementations are compiled automatically. Style caching is explicitly enabled. No project FPS improvement is claimed without measuring the display and mirror workloads. |
| LVGL object/state APIs | Dedicated visibility/clickability/scrolling queries and disabled/user-state queries/setters replace deprecated calls. Calibration instructions and numeric-editor errors use `lv_label_set_max_lines()`. |
| LVGL argument validation | Simulator enables argument checking, widget-tree validation, logging and abort-on-invalid-call. Production keeps argument checks without per-call widget-tree walks. Full navigation and calibration flows pass. |
| LVGL class/span checks | Upstream v9.6.0 `lv_arc.c` references undefined `obj_class` when class checks are enabled. The span widget's argument validation passes a span pointer to a widget-tree check. Class checks remain disabled; unused span support is disabled. These limitations are not hidden by a local library patch. |
| Other LVGL additions | Other-SoC GPU backends, glTF, Wayland/DRM, GStreamer, Chinese calendar and variable/dynamic fonts are not used by this RGB565 MIPI/SDL controller. Unused deprecated list/window widgets are disabled; public include paths and explicit SDL software selection are used. |
| FastAccelStepper public introspection | Cache `driverTypeString()`, before-change ticks/pause count and after-change ticks under motor ownership. Diagnostics shows actual selected driver and DIR budgets, e.g. `RMT / DIR 1500 + 200 us`. The UI reads the cache without library calls. Experimental multi-axis/I2S planners are not applicable to the single RMT axis. |
| Arduino 3.3.8–3.3.12 / IDF 5.5.5 | USB CDC/HWCDC write fixes, DMA alignment, LEDC timeout/lock fixes, Wire receive bounds, P4 LDO ownership and framework fixes are incorporated through the tested bundle. Existing bounded USB-mirror writes, shared I2C ownership and explicit display LDO allocation remain necessary. Wi-Fi/BLE/OTA/webserver additions are unused. |
| PlatformIO 6.2.0 | Pin the core in local setup and CI; installer checks its Python ≥3.9 requirement. The new `--port` option can set both upload and monitor port: `pio run -e esp32p4-release --target upload --port COM3`. Removed PVS integration is not used. |
| Unity 2.7.0 | Updated floating-point assertions and assertion fixes are used by existing/new tests. Optional generated-runner shuffling does not apply to the project's explicit `RUN_TEST` handlers. The custom runner preserves the exact upstream version. |
| ArduinoJson 7.4.3 | Already includes its floating-point string conversion buffer fix. Settings formats and APIs need no migration. |
| Touch/panel/SDL components | GT911 source fixes are included, retaining 400 kHz and shared-bus ownership. Remaining panel updates are metadata-only; SDL2 remains the supported simulator driver. |

The calibration redesign also resolves the program editor's collapsed layout: LVGL layout must be updated before reading child positions and applying the existing header offset. The layout audit now checks the editor footer and mode-row positions, in addition to label boundaries.

Sources: [LVGL 9.6 changelog](https://github.com/lvgl/lvgl/blob/v9.6.0/docs/src/changelog/CHANGELOG.mdx), [Arduino 3.3.8](https://github.com/espressif/arduino-esp32/releases/tag/3.3.8), [3.3.9](https://github.com/espressif/arduino-esp32/releases/tag/3.3.9), [3.3.10](https://github.com/espressif/arduino-esp32/releases/tag/3.3.10), [3.3.11](https://github.com/espressif/arduino-esp32/releases/tag/3.3.11), [3.3.12](https://github.com/espressif/arduino-esp32/releases/tag/3.3.12), [IDF 5.5.5](https://github.com/espressif/esp-idf/releases/tag/v5.5.5), [PlatformIO 6.2.0](https://github.com/platformio/platformio-core/releases/tag/v6.2.0), [Unity 2.7.0](https://github.com/ThrowTheSwitch/Unity/releases/tag/v2.7.0).

See [guided calibration](CALIBRATION_WORKFLOW.md) for the new UI, formula, runtime draft isolation and verified-save behavior.

## Validation

- **438/438** native cases, including **31** production-control cases using the actual dispatcher, motor wrapper and modes with fake hardware. New cases cover explicit RMT selection, pending force-stop drain and forbidden DIR reconfiguration during queued motion.
- **Six** packaging regressions pass, including required license notices and clean build identity.
- `scripts/test_fas_rmt.py` compiles the installed upstream 1.4.0 core and actual IDF5/6 encoder with upstream `test_30.cpp`. Both RMT buffer geometries pass (PART_SIZE 32 and 24; 158 profiles covering long low phases, pauses, pulse counts and direction changes). Linux and Windows CI run this test.
- The GPIO, storage and system device-test programs compile against the updated dependencies; build-only validation does not execute hardware tests.
- Release, debug and USB-mirror firmware build against the pinned platform and actual upstream FastAccelStepper commit.
- LVGL navigation/setup self-test, layout audit and commissioning/screenshot exports use the actual 9.6 source. Runtime screenshots are refreshed from this simulator.
- Release build: 32,564 / 327,680 bytes static RAM and 1,108,124 / 6,553,600 bytes application flash. Framework SPI code emits an upstream discarded-volatile warning; application compilation succeeds.

Reproduce:

```sh
python -m pip install -r requirements-dev.txt
pio pkg install -e esp32p4-release
pio test -e native -e native-control
pio run -e esp32p4-release -e esp32p4-debug -e esp32p4-mirror
python scripts/test_fas_rmt.py
python -m unittest discover -s test/tooling -v
```

Run `simulator/run.ps1 -SelfTest` and the simulator's `--audit-layout` for the actual UI. Host tests do not measure hardware timing.

## Remaining device verification

Before deploying this major motor-library update, compare commanded and measured movement in both directions over repeated Step moves and full revolutions; sweep the configured RPM range at each used microstep setting. Measure STEP high/low widths and DIR setup/hold at the driver inputs, including direction reversals. Verify normal STOP, physical E-STOP, driver alarm, queue-drain reset and fresh START without automatic restart. Repeat under display/USB-mirror load and NVS writes; upstream warns that flash writes during movement can disturb motion timing. This upgrade does not change the storage scheduling policy.

Confirm presets, calibration, setup state and display settings survive reboot. These checks require the real board/driver and have not been performed by this update. No device flashing, release publication or merge is implied by successful compilation or host regression results.
