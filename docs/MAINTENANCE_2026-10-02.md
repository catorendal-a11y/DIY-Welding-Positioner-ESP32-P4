# v2.1.1 maintenance implementation

This source update implements the software corrections from [the code review](CODE_REVIEW_2026-10-02.md). The published v2.1.0 assets are unchanged; v2.1.1 has not been published or flashed to the device during this task.

**Subsequent source work:** [single-owner control and Setup Wizard](CONTROL_SETUP_IMPLEMENTATION.md) supersedes the architectural follow-up items in this historical maintenance report.

## Implemented changes

| Review item | Implementation |
|---|---|
| 1. Unbounded STOP access | Runtime stepper locks are bounded to 2 ms. Failure disables ENA and latches a motor timeout. A safety-task supervisor watches STOP acknowledgement and STOPPING completion independently of the control task. Emergency cleanup inhibits ENA before obtaining the lock and retries until successful. |
| 2. Ignored command errors | Movement, speed and acceleration results are checked. Rejected commands latch a fault; rejected steps cannot increment completion counters. Reset cannot complete while motor cleanup remains pending. |
| 3. Stack units | Removed the incorrect ×4 conversion from task diagnostics and warning thresholds. |
| 4. Direction preview | Program confirmation resolves direction with the same inversion policy as execution. The simulator also applies inversion consistently. |
| 5. RPM precision | Shared adaptive formatter across main, countdown, jog, pulse, step and program editors/list; minimum 0.001 RPM remains visible. |
| 6. Save confirmation | Generation-specific settings receipts distinguish queued, failed and committed calibration saves. Requests arriving during a write remain pending. |
| 7. NVS diagnostics | Partition size comes from the actual partition table. Payload bytes and NVS entry statistics are separate APIs. |
| 8. I2C units | I2C timeout arguments remain milliseconds. ADS1115 runtime reads also check conversion-ready status and use a conversion deadline rather than refreshing old data. |
| 9. Position wrap | Step progress uses modular 32-bit distance for supported moves shorter than half the counter range. |
| 10. Pulse lifecycle | Shared `On / Decelerating / Off / Complete` timing core, completed-ON count and 32-bit saturating counter. Finite programs retain the final OFF pause. |
| 11. Event contention | Failed snapshots preserve the displayed version for retry. Dropped writes are counted and visible in diagnostics. The latched fault reason remains independently available. |
| 12. Simulator provenance | EXE reports embedded build identity. Packaging verifies source identity and dirty state, starts from temporary clean staging, validates x64 format and requires license notices. Archive records EXE hash and runtime libraries. |
| 13. Production coverage | New host suite compiles the actual command dispatcher, motor wrapper, event log and all four motion modes with injected clock/driver adapters. Simulator shares production program execution, pulse timing and save-generation policy. |
| 14. Build portability | Serial ports auto-detect; SDL/library paths are configurable. Application warnings are enabled and obsolete UI helpers removed. Windows UCRT64 CI builds, checks and packages the simulator for release alongside firmware. |

The safety cleanup probe is now nonblocking, preserving the safety task's scheduled polling opportunity. System-info counter deltas also tolerate runtime-counter wrap, and countdown text buffers no longer trigger truncation warnings.

## STOP supervision

- Runtime mutex acquisition: at most **2 ms of requested FreeRTOS lock wait**; a miss inhibits ENA and latches a fault.
- STOP request acknowledgement: a **50 ms operational timeout**, sampled by the safety task. Timestamp encoding has 1 ms resolution tolerance and survives `millis()` wrap.
- STOPPING completion: **2000 ms ramp allowance + current step rate / applied acceleration**, including reduced soft-start acceleration. Failure inhibits ENA and latches a timeout.
- These are software supervision settings, **not measured physical stop times**. Scheduler stalls, electrical drive behavior and mechanical stopping still require device measurements. The direct physical E-STOP inhibit remains in place.

The existing motor/control tasks still share a bounded stepper adapter. Converting every motor call to a single-owner executor and publishing one comprehensive immutable control snapshot remain larger architectural options; they are not required to claim the bounded-access corrections above.

## Validation

- **419/419** native and production-control test cases passed.
- **6/6** Python simulator-packaging regression cases passed.
- **Release, debug and USB-mirror firmware builds passed** with the pinned ESP32-P4 toolchain. A pre-existing Arduino SPI dependency warning remains; the maintenance application build produced no compiler warnings.
- LVGL simulator self-test passed, including failed persistence/retry and rejected-motion scenarios.
- Layout audit passed with **0 failures** across registered screens at 800×480 and E-STOP/driver/ADC/I2C overlays, including minimum main-screen RPM. Explicit ellipsis/scrolling and compact event summaries are intentional; this is not an exhaustive audit of every possible operator input.
- A local v2.1.1 development simulator ZIP was packaged and all 11 archived files passed checksum verification. Its `BUILD.json` explicitly records a dirty development build. Portable self-test was run with only Windows system paths on PATH.
- GitHub Actions passed native tests, all three firmware builds and Linux UI checks. The first Windows/UCRT64 run passed simulator self-test and layout audit but exposed changed MSYS2 runtime-license directories. Packaging now supports both the legacy and split runtime packages, with regression coverage; the corrected cloud package is being verified.

## Simulator scenarios

Source builds accept `--scenario estop`, `driver-alarm`, `stale-adc`, `i2c-failure`, `rejected-motion` or `nvs-failure`. `--audit-layout` runs font/label geometry checks; `--build-info` prints embedded identity. Scenarios run offline and do not control the real machine.

## Remaining hardware work

Measure normal STOP, physical E-STOP and driver-alarm response on the actual controller and drive. Test pedal/I2C loss under motion and NVS writes, direction inversion, low-RPM accuracy and long-running operation. No physical safety certification or motion-accuracy result is asserted here.

An encoder/rotation sensor, closed-loop motion control and electrical safety-chain changes require additional hardware and bench validation. They remain optional hardware enhancements, not software fixes delivered by this update.

Windows CI toolchain discovery follows the [setup-msys2 action's documented installation-location output](https://github.com/msys2/setup-msys2#outputs), avoiding assumptions about runner drive letters.
