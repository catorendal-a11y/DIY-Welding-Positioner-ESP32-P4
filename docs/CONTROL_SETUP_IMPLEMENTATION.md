# Control ownership and guided setup

Source v2.1.1, unreleased. This change preserves the existing V5 design and physical E-STOP input. It has not been flashed or released as part of this implementation.

## Control and safety boundaries

- `inputTask` samples GPIO pedal at 5 ms and ADC at 20 ms. Cross-task pedal-filter data is atomic. It sends requests and never calls the stepper library.
- `controlTask` executes `control_run_cycle()` every 5 ms. It owns runtime speed/acceleration, dispatcher, modes, step position, STOPPING completion and fault cleanup. Runtime stepper calls are confined to the motor adapter; ESP32 owner checks reject calls from another task. Startup configuration occurs before ownership binding.
- The internal bounded stepper mutex remains as a defensive adapter boundary and test-injection point. It is not exposed through a raw stepper-pointer API.
- The physical E-STOP ISR still drives ENA HIGH directly. Safety-task inhibition and STOP deadline supervision do not wait for controlTask. Library `forceStop` cleanup runs in the owner task and must finish before reset is accepted.
- Existing operational STOP deadlines remain 50 ms acknowledgement and the applied-acceleration completion budget. These values are software supervision, not measured physical response times.

## Snapshots and UI

`ControlSnapshot` carries sequence/timestamp, state, target and estimated RPM, effective direction, input source, step progress/completions, pulse count and motor-running status. A try-only mailbox protects complete copies; contended readers keep their previous value. A failed publish is retried on the next cycle.

UI refresh is 40 ms. A missing/control snapshot older than 100 ms shows STATUS UNAVAILABLE and disables movement controls. Command admission independently rejects requests if the executor heartbeat is stale. Normal STOP remains unconditional and visible; the physical E-STOP remains independent. Fault status is read directly from safety, not inferred from an old snapshot. Existing command and authoritative state APIs remain compatible; operating screens use the snapshot for display.

Fault headings distinguish physical E-STOP, driver alarm, pedal input, rejected motor commands and timeouts. Reset returns to idle and never starts motion. Jog requests now resolve configured inversion consistently with other modes.

## Setup and storage compatibility

See the [operator guide and actual runtime screenshots](SETUP_WIZARD.md). The wizard reuses Motor Config and verified manual calibration. Explicit requests are required for every motion. Direction confirmation requires observations of both hold controls; flipping clears those observations. Function completion requires physical E-STOP assertion/release/reset and a separate explicit start/stop sequence, followed by user confirmation.

Settings add the optional boolean `setup_completed`. Fresh defaults are false. Missing keys in valid legacy documents migrate to true, preserving the working installation. Wrong types are rejected rather than silently misread. Existing settings and presets are retained. Motor configuration, calibration and wizard completion use specific save-generation receipts. No schema change is made to presets.

## Simulator and validation

The simulator compiles production control, motor wrapper, continuous/pulse/step/jog modes and program executor. Its FreeRTOS, drive, input, safety and persistence adapters remain host simulations; real motion response and scheduler behavior are not asserted. The commissioning regression accelerates only simulated physical travel, leaving control timers unchanged.

- 438 native cases, including 31 cases compiling actual production control/motor/modes. Concurrent mailbox copies, stale admission, STOP, direction inversion, setup sequence and migration are covered.
- Six simulator packaging regression cases.
- Firmware release/debug/USB-mirror builds.
- LVGL self-test includes the actual motor-config and calibration editors, all wizard stages, physical-input simulation, reset without restart, failed completion persistence/retry, new/existing entry and cancellation.
- Geometry/font audit covers registered screens, fault overlays and each captured wizard stage. Intentional ellipsis/scrolling is retained; screenshots are inspected separately.
- Independent stalled-control scenario verifies stale START blocking and fault escalation after a STOP request.

## Device review before deployment

1. Verify saved settings/presets survive upgrade and reboot; the existing machine should open its normal operating screen.
2. Run direction checks with the physical drive settings and configured inversion; release/lost touch must end jog.
3. Complete manual calibration and compare physical rotation with entered measurements.
4. Measure normal STOP, physical E-STOP and driver-alarm response while moving, including pedal release/input loss and NVS writes.
5. Check reset never restarts and requires a new START; repeat interruption/reboot tests during setup.
6. Monitor control/input stack watermarks and scheduling under USB mirror, display activity and extended operation. Host tests do not establish these device limits.

No encoder, electrical safety-chain modification or safety certification is included. This work is delivered through a pull request and UI preview; merging, release publication and device flashing require a separate instruction.
