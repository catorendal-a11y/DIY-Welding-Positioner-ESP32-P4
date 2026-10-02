# Implemented improvements — 1 October 2026

Branch `codex/ui-v3-integration`, based on `929b1f6` (v2.0.9). Integration was published as `65238f3` on 2 October 2026; v2.1.0 is the subsequent release. After owner authorization, integration firmware was uploaded on COM3 with data-hash verification and RTS reset. No physical motor test was performed.

Scope covers control, pedal/touch, safety tasks, motor settings, storage, USB mirror, all registered screens, simulator, tests, builds and documentation. Third-party libraries were not exhaustively audited.

## Original findings and implementation

Numbers correspond to the [historical review](CODE_REVIEW_2026-10-01.md).

| Finding | Implementation | Verification and limits |
| --- | --- | --- |
| 1. STOP overwritten | Separate STOP latch/start generations; one receive per command. | Direct production `MotionGate` tests; complete FreeRTOS dispatcher integration untested. |
| 2. Pedal starts at boot/after release | 50 ms stable release before arming; release cancels queued pedal start. | Direct `PedalInterlock` boot/short-press/fault/disable tests; physical bounce unmeasured. |
| 3. Stale pedal value | Validity/age checks; active input failure blocks motion without panel fallback. | Freshness/time-wrap tests; electrical/disconnect testing pending. |
| 4. Stuck JOG | Touch error releases input; JOG must renew within 150 ms. | Builds/simulator pass; physical stop includes scheduling/deceleration. |
| 5. Incomplete gate | Pending/latched faults, task readiness and pedal health included at motor enable. | Source/build checks; GPIO/ISR activation is not atomic and needs bench measurement. |
| 6. Blocking cleanup | ESTOP publication avoids logging/motor locks; controlTask performs cleanup. | Source/build checks; driver alarm retains a bounded initial cleanup attempt. Timing/lock contention unmeasured. |
| 7. Wrong documented polarity | HIGH healthy/LOW fault stated; bare NC-to-GND marked incompatible. | Documentation corrected; actual ENA/interface/cable-break response unverified. Compare legacy diagrams against current contract. |
| 8. Unchecked startup | Task/watchdog results checked; four critical READY signals required. | All firmware variants build; device allocation-failure injection pending. |
| 9. Lost program drafts | Retained edit session; periodic updates preserve CONT/PULSE edits. | Simulator tests navigation, edit, update and save. |
| 10. Program speed overridden | Parameter snapshot dispatched to controlTask; static RPM difference no longer takes over. | Source/build checks; physical potentiometer response pending. |
| 11. Lost saves | `SaveRequest` retains failed/concurrent changes with backoff; UI pending/error; invalid stored data blocks boot. | Production retry/concurrent-write policy tested; no power-loss/NVS fault injection. |
| 12. 300-second dimming becomes 44 | 16-bit timeout; legacy 44 migrates to 300. | Direct production-policy test. |
| 13. UI/test drift | Shared styles, revised navigation, stable main action IDs. | Actual LVGL self-test passes; 22 runtime captures reviewed. |
| 14. Pulse OFF includes braking | Pause begins when motor library reports stopped. | Source/build checks; no encoder measurement of physical standstill. |
| 15. Tests duplicate code | Eight added tests exercise shared STOP/pedal/freshness/dimming/save policy; simulator uses actual callbacks. | Coverage improved; motor/storage stubs and full FreeRTOS/hardware tests remain. |
| 16. Unbounded USB sending | Deadline, short driver timeout/bounded chunks; failure releases remote input; redraw after dropped regions; init cleanup. | Mirror builds; actual disconnect/backpressure/reconnect testing pending. |

## Additional changes

- Motor settings and starts use control dispatch; UI shows pending application/rejection/save state.
- Main displays mode, speed source and commanded direction. Menu/pedal edits are blocked during motion; STOP remains available.
- CONT auto-stop editable; mode labels readable; microstep selector marks one value.
- Calibration scrolls with fixed motion/footer actions; held JOG keeps renewing.
- System info shows unavailable temperature instead of false zero, matching heap categories and overflow-safe load math.
- Pulse-derived speed is labelled calculated. Boot text describes initialization, not a completed machine self-test.
- Restart blocked during motion/pending or failed saves.
- Shared mode fields use atomics. Nonblocking event logging may drop entries under contention.
- Pinned dependencies and expanded CI. The [published integration run](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/actions/runs/36987291934) passed every job.

## UI artifacts

- [V3 proposals](images/ui_mockup_v3/index.html), [vector overview](images/ui_mockup_v3/all_screens.svg), [V3 runtime captures](images/ui_runtime_v3/overview.png).
- [Final V5 runtime captures](images/ui_runtime_v5/overview.png) and [V5 implementation/upload](UI_V5_DEPLOYMENT.md).

Runtime images use simulated data. Generic confirmation export lacks caller context; self-tests open the populated dialog. Calibration shows a scrollable portion. SVGs record proposals, while runtime captures show implementation.

## Verification

| Check | Result |
| --- | --- |
| Native | **403/403 passed** |
| LVGL `--self-test` | **PASS** |
| Release/debug/mirror | **All three build** |
| SVG XML | 31 parsed files: 30 views plus overview |
| Runtime export | 22 screens; final V5 also includes fault overlays |
| `git diff --check` | Passed |

[Results JSON](validation/2026-10-01/results.json), [native log](validation/2026-10-01/native-tests.log), [UI log](validation/2026-10-01/ui-self-test.log), [build log](validation/2026-10-01/firmware-build.log), [final V5 logs](validation/2026-10-01/ui-v5/). Arduino framework SPI code emits a volatile-qualifier warning; builds succeed. Physical E-STOP/ENA/cable-break, input failures, pulse timing and USB disconnection require bench testing.

## Device upload

Integration release firmware uploaded through COM3 to ESP32-P4 revision v1.0. PlatformIO reported SUCCESS and esptool verified the hash. This proves flash transfer, not physical safety behavior. [Upload log](validation/2026-10-01/firmware-upload.log).
