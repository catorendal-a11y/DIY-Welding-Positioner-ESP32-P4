# Local code review and fixes — 2026-10-07

Reviewed `master` at `07e76eb`, starting from a clean checkout. Scope: firmware control, motor modes, safety supervision, input/speed handling, storage, affected LVGL flows, simulator and packaging. The reviewed fixes are recorded with this report. No device upload was performed.

## Confirmed defects and corrections

| Area | Reproduction / consequence | Correction |
|---|---|---|
| Driver configuration | A previously settled Standard enable survived changing to DM542T; the next start emitted pulses immediately. | Driver timing changes disable ENA and re-arm settling. |
| Motor adapter | Direct run/move calls could bypass the new settle state machine. | Every run/move entry rejects an unsettled enable. |
| Step repeats | Dwell disabled ENA, then the next repeat enabled and stepped immediately. | Reassert ENA once after dwell and wait for settling without blocking. |
| Jog input loss | No further hold event during the 200 ms wait still produced a Jog start; mode entry minted a fresh lease. | Expire the hold lease during ENABLING; only UI hold events renew it. |
| Commissioning/calibration UI | Jog buttons were disabled during ENABLING, interrupting the held input. | Keep held Jog controls available during settling; simulator supplies repeated PRESSING events and asserts availability. |
| Driver alarm | A one-sample LOW disabled ENA without stopping the planner or latching a fault. Raw ALM also allowed start/reset before debounce. | First LOW latches the fault and cancels motion; admission/reset check raw ALM. |
| E-STOP reset | Reset could clear an unprocessed ISR edge when the raw input had already returned HIGH. | Reject pending edges and never clear the ISR pending flag in reset. |
| Reset prerequisites | Required restart and failed required touch did not block reset. | Add both prerequisites to the reset policy. |
| Redundant E-STOP poll | Missed-interrupt level detection waited for debounce before inhibiting ENA. | Disable ENA on the first scheduled LOW observation. |
| Configuration rollback | Restoring the entire settings snapshot overwrote unrelated concurrent edits; failed immediate hardware rollback was not retried. | Restore only motor fields and retry hardware rollback during fault cleanup. |
| Soft-start cleanup | Failed acceleration restoration cleared the retry flag and marked the fault cleaned. | Keep restoration pending and block cleanup completion until success. |
| Fatal halt | A single ENA write did not prevent another live task from subsequently enabling the motor. | Set the shared restart/motion inhibit before disabling ENA. |
| Boot fault visibility | Control initialization overwrote ESTOP after a motor initialization fault, leaving motion locked without the expected fault overlay/cleanup path. | Preserve the previously latched fault in the initial control state. |
| Storage format | STOP could be acknowledged while erase remained in progress, reopening motion admission. | Inhibit before erase; preserve the earlier inhibit state on erase failure. |

Targeted tests reproduced the control, safety and storage failures before their corrections. Existing baseline: 467 host cases and 10 packaging cases passed; a release firmware build and simulator self-test/layout audit also passed. Those checks did not cover the defects above.

## Validation

- Final host suite: 487/487 cases passed, including 12 actual safety-supervisor cases. New safety environment is included in CI.
- Packaging: 10/10 cases passed.
- Upstream FastAccelStepper RMT encoder regression: PASS for both PART_SIZE geometries (32 and 24).
- Release, debug and mirror firmware: all three builds passed locally.
- Windows LVGL simulator: self-test PASS and layout audit 0 failures; includes held Jog across driver settling in the Jog, Setup and Calibration screens.
- `git diff --check`: passed.

Commands: `pio test -e native -e native-control -e native-speed -e native-storage -e native-safety`; `pio run -e esp32p4-release -e esp32p4-debug -e esp32p4-mirror`; `python -m unittest discover -s test/tooling -v`; `python scripts/test_fas_rmt.py`; `cmake --build simulator/build --target rotator_simulator`; simulator `--self-test` and `--audit-layout`.

Local command logs are under `.pio/audit-*.log`. Host/simulator evidence verifies software behavior, not physical GPIO pulse shape, mechanical stopping time or HF immunity. No assembled-controller test was run during this review.

The clean firmware builds emitted one existing warning per variant from the bundled Arduino core (`esp32-hal-spi.c`, discarded volatile qualifier). This is outside project source; no application-source build errors or warnings were reported.

## Follow-up: five-commit review

Reviewing the five latest commits at `e86fb53` found three remaining defects. All are corrected in the follow-up changes:

- Apply motor proposals from a private `SystemSettings` copy; publish the six motor fields only after hardware acceptance. Actual control/storage regressions now prove a pending save cannot persist cancelled microsteps, that the accepted proposal waits for its own durable receipt, and that an older flash snapshot cannot overwrite the committed direction cache.
- Use `g_storageFormatting` for the temporary format inhibit. Erase failure clears only this flag; a concurrently asserted `g_restartRequired` survives. Safety admission/reset checks include both flags.
- Track direction debounce validity separately from its timestamp. Tests cover zero at wrap, a candidate spanning wrap, and restarting the stability interval after bounce.

Six targeted regressions failed before these changes. The full suite then passed **495/495**, including the new `native-control-storage` environment added to CI. All three firmware variants rebuilt successfully; simulator self-test passed, layout audit reported zero failures, 10 packaging tests passed and the two RMT encoder geometries passed. Follow-up logs are `.pio/fix-review-*.log`. This validation remains software-only; no device was flashed.
