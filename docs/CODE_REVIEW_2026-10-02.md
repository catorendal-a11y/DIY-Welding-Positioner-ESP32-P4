# Code review and improvement plan — 2026-10-02

Baseline: commit `1bcac75`, firmware version `v2.1.0`.

**Follow-up:** software corrections were implemented in source v2.1.1. See the [maintenance implementation and validation report](MAINTENANCE_2026-10-02.md). Findings below describe the original review baseline.

This review covers the firmware architecture, motion modes, safety and input handling, persistence, LVGL UI, simulator, USB mirror, tests, packaging and CI. Critical control paths received detailed source inspection; other screens, helpers and tooling received targeted inspection and repository searches. This is not a claim that every line of third-party or generated code was audited. No firmware behavior was changed during this review.

## Validation and limits

- Native suite: **403/403 tests passed** in this review.
- Published portable Windows simulator: **`--self-test` passed**, with only Windows system directories on PATH. This exercises real LVGL UI code with simulated hardware.
- Installed ESP32-P4 SDK headers confirm that the I2C APIs accept milliseconds. The installed `qio_qspi` SDK configuration has `CONFIG_FREERTOS_HZ=1000`.
- No new firmware build, device flashing, electrical measurements or physical motion tests were performed in this review.
- Existing tests are useful but do not prove real-time deadlines, electrical fail-safe behavior or motion accuracy. `test/README.md` explicitly distinguishes legacy modeled tests from direct production-policy checks.

Priorities: **P1** = address before expanding unattended or sustained motion testing; **P2** = next maintenance release; **P3** = product/architecture improvement. A priority describes the recommended response, not proof of a field failure.

## Findings

### 1. P1 — Normal STOP completion has no bounded lock wait

Evidence: `src/motor/motor.cpp:194–246`, `src/control/control.cpp:325–332`; multiple motion helpers acquire `g_stepperMutex` with `portMAX_DELAY`.

The command gate prioritizes STOP, but motor cleanup can wait indefinitely if the mutex holder does not release it. The physical E-STOP ISR already disables ENA directly; this finding concerns normal software STOP and cleanup, not removal of that hardware inhibit path. The watchdog is a separate backstop, not a documented normal STOP deadline.

**Improvement:** define stop-request, ENA-inhibit and motion-idle deadlines separately. Use bounded motor access and escalate a missed deadline to a latched fault and ENA inhibit. Prefer one task owning the stepper API so that UI, storage and safety cleanup cannot contend for its implementation lock.

**Verification:** inject a held motor lock and stalled executor; measure request-to-inhibit and request-to-idle separately on the device. Choose deadlines from measured deceleration and hardware requirements rather than arbitrary UI timing.

### 2. P1 — Stepper command errors are ignored

Evidence: `src/motor/motor.cpp:164,188`, `src/control/modes/step_mode.cpp:42`. The installed FastAccelStepper header declares `MoveResultCode` for `move()`, `runForward()` and `runBackward()` and documents error returns for missing direction, speed or acceleration configuration.

The wrappers can report a successful start without checking whether the driver accepted the movement. A rejected step command can later be counted as completed after the 50 ms grace period because the stepper remains idle. This is a source-level failure-handling gap; a rejection was not reproduced on the connected hardware.

**Improvement:** check every movement result, disable ENA on rejection, publish a structured fault, and increment completed-move counters only after an accepted command has finished. Apply the same contract to speed/acceleration setters where they return errors.

**Verification:** a fake stepper adapter returning each supported error must never produce RUNNING, a completed step or a successful calibration move.

### 3. P2 — Stack diagnostics overstate free stack by four

Evidence: `src/main.cpp:143,248–256`, `src/ui/screens.cpp:262` multiply `uxTaskGetStackHighWaterMark()` by four.

ESP-IDF reports this value in **bytes**, unlike the word units used by some other FreeRTOS ports. Logs therefore report four times the available stack, and low-stack warnings trigger later than intended.

**Improvement:** remove the multiplier for this target and centralize the conversion behind a documented platform helper. Revisit warning thresholds using corrected measurements under UI creation, faults, logging and flash writes.

Reference: [Espressif RAM usage guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-guides/performance/ram-usage.html).

### 4. P2 — Program confirmation can show the wrong effective direction

Evidence: `src/ui/screens/screen_programs.cpp:34–40` displays the preset direction directly; `src/motor/speed.cpp:452` and the following direction resolver apply `invert_direction` after selecting the program override.

With direction inversion enabled, confirmation can say CW while the resulting command is CCW. This concerns the software-resolved direction; physical direction still depends on wiring and configuration.

**Improvement:** use one direction resolver for confirmation, running UI, diagnostics and motion. If preset direction is intentionally logical, show both the requested and configured effective direction with clear English labels.

**Verification:** test CW/CCW × inversion on/off × panel/program sources; the preview and executed command must agree.

### 5. P2 — Slow RPM values become invisible in several views

Evidence: `src/ui/screens/screen_programs.cpp:71–109` formats RPM with one decimal. `src/ui/screens/screen_timer.cpp:49–51,220` does the same and suppresses changes smaller than approximately 0.05 RPM.

Valid values such as 0.001, 0.010 and 0.040 RPM can all appear as `0.0`. The main screen and diagnostics use different precision, so moving between screens changes the apparent value.

**Improvement:** introduce shared unit/number formatting: three decimals below 0.1 RPM and adequate precision above it. Compare the formatted string or quantized display value when deciding whether to redraw. Use the same formatter for program summaries and countdown.

**Verification:** check minimum speed, precision boundaries and maximum configured speed, including rendered text width at 800×480.

### 6. P2 — Calibration reports durable save before persistence completes

Evidence: `src/ui/screens/screen_calibration.cpp:179–193,933`; `src/motor/calibration.cpp` queues `storage_save_settings()` and logs “saved”.

The UI moves immediately to `RESULT SAVED` even though the storage worker may still be pending or retrying after a write failure. Motor configuration already handles pending/error states more carefully, but calibration does not use the same lifecycle.

**Improvement:** return a save-generation or operation identifier. Display `Saving...`, `Saved` or `Save failed` for that operation, only confirming persistence after its generation is committed. Make other save messages and logs use the same distinction between applied in RAM and persisted.

**Verification:** simulated NVS failures, retries and a second concurrent save must not produce a premature success indication.

### 7. P2 — Storage usage helper uses the wrong partition size and an estimate

Evidence: `src/storage/storage.cpp:432–442` hardcodes `0x6000` and adds 1536 bytes to JSON payload lengths. `default_16MB.csv` defines the NVS partition as `0x5000`.

The helper describes 24 KiB instead of 20 KiB and does not measure actual NVS page/entry occupancy. No corruption was identified from this mismatch; it is an incorrect diagnostic API.

**Improvement:** obtain the partition size from the partition table and use `nvs_get_stats()` for occupancy. Keep JSON payload size separate and label it as payload size if retained.

### 8. P2 — I2C timeout units contain a portability defect

Evidence: `src/motor/speed.cpp:59,65` supplies `pdMS_TO_TICKS(timeout_ms)` to ESP-IDF I2C APIs whose `xfer_timeout_ms` argument is already milliseconds.

At the installed 1000 Hz tick rate the values are numerically equal, so this is **not evidence of a current timeout failure**. A different tick rate silently changes the I2C deadline.

**Improvement:** pass milliseconds directly to the I2C APIs; reserve tick conversion for FreeRTOS delays and synchronization. Name timeout variables with their unit.

Reference: [Espressif I2C API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/api-reference/peripherals/i2c.html).

### 9. P2 — Step progress does not handle position-counter wrap

Evidence: `src/control/modes/step_mode.cpp:146–150` subtracts signed 32-bit positions after widening each operand to 64 bits.

Crossing the signed position boundary makes a small move appear to span approximately 2³² steps, which is then clamped to 100% progress. Completion itself uses `isRunning()`, so this finding does not demonstrate that physical movement ends early.

**Improvement:** compute a documented modular position delta, or maintain a wider session position in the motion adapter. Preserve the maximum-move assumption explicitly.

**Verification:** start immediately on both sides of the signed boundary and move in both directions; progress must remain monotonic and proportional.

### 10. P2 — Pulse lifecycle and cycle count need explicit semantics

Evidence: `src/control/modes/pulse.cpp:19,68–78` increments the count at OFF→ON and marks the phase ON before checking the cycle limit.

A one-cycle program performs one ON interval, decelerates, waits the OFF interval and then stops. It does **not** execute an extra ON interval. However, the completed-cycle count lags during OFF, and the phase can report ON during the transition to STOPPING. The unlimited 16-bit counter also wraps after 65535 cycles.

**Improvement:** explicit `ON`, `DECELERATING`, `OFF`, `COMPLETE` phases; define whether a finite program includes the final OFF interval. Count completed ON intervals at their completion and use a wider or saturating counter.

**Verification:** run the production controller with a fake clock for 1, 2 and unlimited cycles, interrupted deceleration and STOP at every phase boundary.

### 11. P2 — Event-log contention can discard faults and freeze the displayed snapshot

Evidence: `src/event_log.cpp:16,34,62` uses a zero-wait lock and silently drops writes or returns an empty snapshot. `src/ui/screens/screen_diagnostics.cpp:160–164` marks the log version as consumed before knowing whether the snapshot succeeded.

A contended snapshot can replace the strip with `-` and prevent a retry until a new event arrives. Contended writes also disappear without a drop count. Motion should not be blocked by logging, but important fault evidence should not silently vanish.

**Improvement:** distinguish an unavailable snapshot from an empty log, advance the displayed version only after a successful read, and expose dropped-event counts. Consider a bounded nonblocking event queue with reserved critical-fault capacity.

### 12. P2 — Simulator archive metadata can describe the checkout instead of the binary

Evidence: `scripts/package_simulator.py` packages an existing EXE but derives version/commit from current source and HEAD. It reuses the staging directory and silently omits missing license files. `.github/workflows` currently builds on Ubuntu; no Windows simulator packaging job is present.

An old EXE can be labeled with a newer commit. Reusing the directory can also include unrelated leftover files. The current published simulator passed its smoke test; this finding concerns reliability of future packaging.

**Improvement:** generate version/commit/toolchain metadata at build time, validate it during packaging, stage in a clean temporary directory, require expected license notices, and package in Windows CI from an exact commit. Validate archive contents and DLL closure on a clean runner.

### 13. P2 — Test coverage duplicates important behavior rather than executing it

Evidence: `test/README.md`, `src/control/test_logic.h`, `simulator/CMakeLists.txt`, `simulator/sim_stubs.cpp`.

The native suite includes legacy models; the UI simulator compiles real screens but substitutes motion/control behavior. Consequently, 403 passing tests and the UI smoke test do not validate all production command sequencing, mode timing or driver-error paths.

**Improvement:** extract the production state machines into a platform-independent control core. Inject clock, motor and persistence interfaces; compile the same core into firmware, native tests and the simulator. Retain adapter tests for FreeRTOS and hardware behavior.

### 14. P3 — Build defaults remain tied to one developer workstation

Evidence: `platformio.ini` sets upload/monitor to COM5; `simulator/CMakeLists.txt` uses MSYS2 paths and firmware-build dependency directories. Common warning suppression hides unused code and signed comparisons in application code as well as dependencies.

**Improvement:** auto-detect or override serial ports locally, support explicit toolchain/dependency paths with CMake presets, and enable stricter warnings for project code. Keep dependency warning exemptions narrowly scoped. Retain pinned versions and artifact checks already present.

## New solutions

### Shared motion core and single motor owner

Implement one production control core with typed commands, explicit mode phases and structured results. A hardware adapter owns FastAccelStepper; a simulator adapter models accepted/rejected commands, acceleration and sensor events. Publish an immutable snapshot for UI readers containing command ID, state, effective direction, target speed, estimated speed, inhibit reasons and persistence status.

This provides a practical route to testing actual motion logic on a PC without pretending that the PC validates physical machinery.

### Honest, consistent operator UI

Keep the approved dark/orange design. Improve behavior and information hierarchy before another visual redesign:

- Shared RPM/direction/unit formatting on all screens.
- A visible distinction between `Target RPM` and `Estimated RPM`; only show measured RPM when a real sensor exists.
- Persistent fault reason with clear reset eligibility and a separate software STOP control.
- Explicit pending/applied/saved states, with errors remaining visible until acknowledged.
- Text-fit tests using actual LVGL fonts and longest valid names, minimum RPM, maximum values and fault strings; screenshot review at native 800×480 resolution.
- Test pointer release, screen changes during a press, unavailable ADC, dim/wake and blocked reset as complete operator journeys.

### Sensor-backed speed and motion verification

An optional spindle encoder or rotation sensor could provide measured RPM, completed-angle validation and commanded-versus-measured motion discrepancy detection. Start with monitoring and calibration; closed-loop control needs its own hardware and stability validation. Keep open-loop operation explicitly labeled until feedback is installed and verified.

### Diagnostics and fault injection

Add a simulator scenario panel for E-STOP, driver alarm, stale ADC, I2C failure, rejected motion commands, failed NVS writes and time jumps. Export a local diagnostic bundle containing firmware/build identity, fault history, task timing and settings with no credentials. Add event drop counters and corrected stack/partition diagnostics.

### Repeatable releases

Build firmware and Windows simulator from the same tagged source. Embed build identity, validate flash layout, package only clean outputs, require license notices and run native, UI and production-core checks. Generate release information from these outputs so that documentation and downloadable files stay aligned.

## Recommended order

1. Check motion command results and define bounded STOP/fault behavior; verify physically before declaring deadlines achieved.
2. Correct stack reporting, direction preview, RPM formatting, calibration save status and storage diagnostics.
3. Add production-core tests for pulse phases, step wrap, rejected commands and persistence failures; fix event-log retry behavior.
4. Introduce shared control snapshots and the simulator fault panel incrementally.
5. Automate Windows builds and clean release packaging. Evaluate an encoder as a separate hardware enhancement.

## Coverage map

| Area | Inspection focus | Remaining validation |
|---|---|---|
| Boot, shared state, task scheduling | Initialization, task health, stack reporting, fail-closed handling | Device boot/failure injection and timing |
| Safety and inputs | ENA inhibit, reset, STOP generation, pedal freshness/release, watchdog | Electrical behavior and measured deadlines |
| Motor, speed and motion modes | API results, locking, direction, conversions, ADC, pulse/step timing | Production-core and physical motion tests |
| Persistence | Save generations, retries, settings policy, partition usage | NVS failure and power-loss tests |
| UI | Navigation, status, values, calibration, program confirmation, diagnostics | Long-text geometry and operator journey audit |
| Display/touch and USB mirror | Buffers, input release, bounded queues and transport handling | Physical display/USB faults and latency |
| Simulator | Real-screen coverage, control stubs, self-test, build dependencies | Shared-core simulation and clean Windows runner |
| Tooling, tests and CI | Modeled versus production tests, packaging, provenance, portability | Full fresh builds and automated Windows release |
| External drivers/generated fonts | Integration contracts and use sites | Not a third-party source/security audit |
