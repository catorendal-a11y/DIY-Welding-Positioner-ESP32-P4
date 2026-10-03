# Code and logic review — 2026-10-03

Reviewed checkout: `d5d5d1036d4d5124bdd37c2a02b8d05d6d8f6ffd` (`master`).
The findings below describe that baseline. The corrections listed here are implemented in the current **unreleased working tree**. No firmware was flashed during this work.

## Implemented corrections

| Finding | Resolution | Permanent regression coverage |
| --- | --- | --- |
| F01 | A countdown retains its original control generation. STOP, any fault, stale control and screen departure cancel it; reset cannot renew it. | Production dispatcher; actual LVGL countdown, including fault/reset without a UI tick and fault near zero. |
| F02 | Jog re-clamps at execution and publishes the effective value after configuration changes. | Real dispatcher/configuration and Jog mode with a lowered cap. |
| F03 | Checked wide pulse arithmetic before any signed 32-bit cast; 64-bit repeated-move previews; finite input validation; a visibly blocked Step action for impossible moves. | Actual geometry/calibration, signed boundaries, NaN/infinity and LVGL boundary geometry. |
| F04 | Independent Pulse-deceleration and finite-Step deadlines, derived from command rate and configured acceleration. Completed moves clear their deadline. | Stuck driver injection into actual modes, completion cancellation and existing wrap tests. |
| F05 | Retain 20 Hz; derive/display minimum RPM rounded up to the screen's 0.001 RPM precision. Driver conversion rejects lower/invalid rates. No feasible range yields a blocked START. | Actual geometry at microstep 4, small parts and caps below the minimum. |
| F06 | Store requested direction; resolve inversion once for editor/review/runtime. Existing saved direction semantics remain unchanged. | Production direction policy and actual New Program creation. |
| F07 | Capture the active analog input baseline and rebase when its source changes. | Actual ADS-enabled arbitration with stationary and moved pedal samples. |
| F08 | Preserve valid legacy UTF-8 bytes. Validate new names as printable ASCII supported by the current font; reject unsupported/invalid input rather than silently changing accepted names. | Real storage round trip, malformed UTF-8/name boundaries and LVGL input validation. |
| F09 | Share fine RPM increments across Step/Jog and apply the effective minimum. | Actual LVGL Step adjustment and production speed limits. |
| F10 | Replace unsupported LVGL CLIB monitoring with supported ESP-IDF largest/minimum internal heap statistics. | Debug firmware compilation and configured allocator/source inspection. |

Additional corrections: exact serialization/NVS counts, allocation-overflow rejection, bounded blobs and list sizes, object validation, normalized program IDs, validated legacy migration with rollback on write failure, checked maintenance erase, program/Display save receipts, retained dim drafts, explicit stale motion markers, timing/sequence review before program START, bounded USB RX and partial frames, viewer release on focus loss and bounded/failed-write disconnects. CI now runs the real speed and storage fixtures.

The original report is retained below as evidence of what was reproduced. Recommended architecture extensions are not claims that hardware behavior or upstream dependencies were exhaustively proven.

## Validation after corrections

| Check | Result |
| --- | --- |
| Native modeled logic + production control/speed/storage | **459/459 passed** (407 modeled, 37 control/motor/modes, 6 geometry/ADS, 9 real storage/migration tests) |
| Packaging/tooling regressions | **10/10 passed** |
| LVGL simulator self-test | **PASS**, including cancellation, direction, fine speed, program save failure/retry and prevention of edits during pending writes, calibration and commissioning |
| 800×480 layout audit | **0 failures** |
| Actual pinned FastAccelStepper RMT encoder test | **PASS** |
| Release, debug and USB mirror firmware | **All three compile successfully** |
| Physical unit | Not flashed; pulse measurements, stop timing, E-STOP/ALM polarity, TIG HF immunity and power-loss recovery remain bench checks |

Only Windows host/simulator checks were rerun for these source fixes; CI's Linux execution will be checked when the changes are published. The framework still emits its existing SPI volatile-qualifier warning. No project compile error remains.

The storage fixture compiles actual `storage.cpp` with ArduinoJson 7.4.3; only Preferences/LittleFS/partition services are substituted. It injects short reads/writes and an allocation-rejecting ArduinoJson allocator. Host migration rollback is not proof of atomicity across two NVS keys during real power loss. Legacy UTF-8 storage is preserved, but non-ASCII display glyphs remain limited: rename such programs with supported English characters for a fully readable label.

The mechanical 108:1 reduction and 80 mm roller model are retained. A direct-chuck profile needs confirmation of the actual mechanism; no unverified mechanical change is introduced.

## Baseline findings (before correction)

### F01 · P1 · Countdown can start rotation after an E-STOP reset

**Sources:** [screen_timer.cpp:275–305](../src/ui/screens/screen_timer.cpp), [screen_estop_overlay.cpp](../src/ui/screens/screen_estop_overlay.cpp).

Start a three-second countdown, trigger E-STOP, release the switch and reset before zero. The countdown survives the fault. At zero, the UI creates a new continuous-start command because the current state is idle again. The dispatcher cannot reject it as an old command: its ticket is created after reset.

The actual countdown screen and production dispatcher reproduced `IDLE` after reset, then `RUNNING` at zero without another START. The simulator supplies the fault/reset inputs; this is not a measurement of the physical switch.

**Change:** Give pending countdowns a control-owned cancellation generation. Cancel on STOP, any fault, stale control, or leaving the screen. Reset must leave the countdown cancelled. Retain the physical E-STOP and existing UI style.

**Regression:** Fault and reset at several points during the countdown, including immediately before zero; assert no start until a new deliberate request. Also test fault while the UI is temporarily stalled.

### F02 · P1 · Jog can exceed a newly lowered maximum RPM

**Sources:** [jog.cpp:14–20, 39–50](../src/control/modes/jog.cpp), [control.cpp:392–403](../src/control/control.cpp).

Set jog to 1.000 RPM, return to idle, apply a 0.100 RPM maximum, then hold JOG. `jog_set_speed()` clamps when the value is edited, but configuration changes do not clamp the stored jog value. `jog_start()` reuses that value directly.

The real configuration dispatcher and jog mode reproduced a 0.100 RPM cap with a stored/commanded 1.000 RPM jog. The diagnostic fixture replaces the hardware driver and speed conversion; its maximum-speed getter reads the applied settings.

**Change:** Validate against the current cap at command execution and on configuration changes. Publish the effective jog speed so the label and command agree. Apply the same validation boundary to every motion mode.

**Regression:** Lower the cap after setting jog, including lowering below its boot default; test both directions and renewed holds.

### F03 · P1 · Valid geometry can overflow a signed 32-bit move

**Sources:** [speed.cpp:218–223](../src/motor/speed.cpp), [calibration.cpp:45–46](../src/motor/calibration.cpp), [step_mode.cpp:46–66](../src/control/modes/step_mode.cpp).

The custom Step screen allows 3600 degrees; diameter allows 20000 mm; microstepping allows 32; calibration allows 1.5. Together they require **2,592,000,000 pulses**, exceeding `INT32_MAX` (2,147,483,647). The calculation casts a float into `long` without checking. On ESP32 and Windows, `long` is 32 bits; FastAccelStepper's relative move parameter is `int32_t`.

The actual conversion functions on Windows produced `-2147483648` for that combination. An out-of-range float-to-integer conversion has no portable result; the ESP32 result was not measured. Negation and `labs()` of the minimum signed value are additional hazards. The Step editor also multiplies a signed `long` pulse count by up to 99 repeats for its preview.

**Change:** Compute in checked wide arithmetic, require finite inputs, and validate before narrowing. Reject infeasible moves with a clear explanation, or deliberately split them into bounded segments. Use 64-bit totals for previews. Validate frequency feasibility too: a valid diameter alone does not imply a valid driver command.

**Regression:** Exact range boundaries, large diameter/angle/factor combinations, both directions, repeated moves, NaN and infinity at the public API boundary.

The pinned [FastAccelStepper 1.4.0 header](https://github.com/gin66/FastAccelStepper/blob/f24a65996bdc501a0497cdca6a636706003333e7/src/FastAccelStepper.h) confirms the signed relative-move interface.

### F04 · P2 · Pulse's internal deceleration has no completion deadline

**Sources:** [pulse.cpp:34–46](../src/control/modes/pulse.cpp), [motion_policy.h:36–48](../src/control/motion_policy.h), [control.cpp:83–93](../src/control/control.cpp).

At the end of ON, Pulse calls `motor_stop()` and stays in `STATE_PULSE`. The timeline waits for `isRunning()` to become false. The independent stopping deadline only covers an explicit STOP request or `STATE_STOPPING`.

With an injected driver that does not finish deceleration, the actual mode remained running in `STATE_PULSE` after 15 seconds with no timeout fault. This is a fault-injection result, not a claim that the real driver routinely fails to stop. The task watchdog still gets fed because the control loop is alive.

**Change:** Supervise internal pulse deceleration with the same bounded stop budget. Preserve the final OFF pause and count only the intended cycle phases. Extend bounded completion supervision to finite Step moves as a separate improvement.

### F05 · P2 · Minimum-speed flooring changes the requested workpiece RPM

**Sources:** [motor.cpp:328–341](../src/motor/motor.cpp), [config.h:82–84](../src/config.h).

Commands below 20 step Hz are silently raised to 20 Hz. With a 300 mm part, microstep 4 and calibration 1.0, 0.001 RPM needs 5.4 Hz, but the command becomes 20 Hz: approximately **0.003704 RPM**, 3.70 times the requested value. Other supported geometries create larger differences.

The actual production geometry and motor conversion functions reproduced this. The default microstep-16/300 mm case does not reveal it. The pulse-derived estimated RPM is not independent encoder feedback.

**Change:** Calculate a feasible RPM range from geometry, calibration and the selected driver. If the floor is necessary, expose the effective minimum and reject lower requests. If lowering it is supported, qualify the new setting against the pinned library and measured pulse behavior; do not simply remove the floor based on a host test.

### F06 · P2 · New Program applies direction inversion twice

**Sources:** [screen_program_edit.cpp:177](../src/ui/screens/screen_program_edit.cpp), [speed.cpp:456–473](../src/motor/speed.cpp), [control.cpp:406–409](../src/control/control.cpp).

`speed_get_direction()` already returns the effective direction after inversion. New Program stores that result as a preset's requested direction. Execution applies inversion again.

With inversion enabled and a raw CW request, current effective direction was CCW, the new draft stored CCW, and its resolved execution direction was CW. Actual UI creation reproduced this with the simulator's direction adapter. The start-review dialog resolves the preset correctly, but the draft's claimed inheritance from the current machine state is wrong.

**Change:** Distinguish requested and effective directions explicitly. Convert only at the hardware boundary and show the effective direction consistently in the editor, review and runtime screens. Preserve existing saved-program semantics through an explicit migration if they change.

### F07 · P2 · UI speed override uses the wrong ADC baseline with an analog pedal

**Sources:** [speed.cpp:366–373, 427–437](../src/motor/speed.cpp).

`speed_slider_set()` records the panel potentiometer ADC as the takeover baseline. `speed_apply()` compares that baseline with the active pedal ADC when analog pedal mode is enabled. Different stationary positions can therefore immediately cancel a UI override.

Using the actual ADS-enabled speed code with fresh injected ADC state, panel ADC 3000 and stationary pedal ADC 1000 changed a requested 0.800 RPM to approximately **2.095 RPM** on the next apply. No pedal movement was injected. The I2C transactions were stubbed; the arbitration logic was production code.

**Change:** Capture the currently active input's baseline, with its source identity and sample freshness. Rebase when sources change. A source change should not be mistaken for a physical movement. Replace the unused `POT_SLIDER_OVERRIDE_RPM_DELTA` promise with implemented policy or remove it.

### F08 · P2 · Accepted UTF-8 program names change on reload

**Sources:** [screen_program_edit.cpp:48–55](../src/ui/screens/screen_program_edit.cpp), [storage.cpp:165–167](../src/storage/storage.cpp), [config.h:134](../src/config.h).

The editor accepts names up to 31 UTF-8 bytes. Loading replaces every non-ASCII byte with `?`. For example, `Nør` becomes `N??r`. The actual sanitizer reproduced this; the NVS round trip was traced in source, not exercised on a device.

**Change:** Preserve valid UTF-8 and supply the required glyphs, or consistently validate an explicitly restricted character set in the editor. Use one shared name policy before save and after load. Never silently alter accepted names.

### F09 · P2 · Step and Jog adjustment cannot reach fine low speeds

**Sources:** [screen_step.cpp:303–312](../src/ui/screens/screen_step.cpp), [screen_jog.cpp:41–51](../src/ui/screens/screen_jog.cpp).

The Step minus button does nothing at 0.100 RPM or lower; its plus button increments by 0.100 RPM. Jog uses the same coarse behavior. This conflicts with the 0.001 RPM range and the program editor's finer increments. The actual Step screen reproduced 0.020 RPM remaining at 0.020 after pressing minus.

**Change:** Share the existing `ui_rpm_increment()` policy across screens, add precise numeric entry where useful, and clamp to the physically feasible range from F05. Keep the V5 layout.

### F10 · P3 · Debug health logging reads an unsupported LVGL memory result

**Sources:** [main.cpp:247–258](../src/main.cpp), [lib/lv_conf.h:25](../lib/lv_conf.h), local pinned LVGL `src/stdlib/clib/lv_mem_core_clib.c:78`.

The debug storage task declares an uninitialized `lv_mem_monitor_t`, calls `lv_mem_monitor_core()`, and reads `used_pct`. With the configured CLIB allocator, that function returns without filling the structure. Consequently the percentage and high-usage warning are meaningless and read uninitialized memory. This was verified against the actual configured LVGL 9.6 source, not a guessed threading problem.

**Change:** Report supported ESP-IDF heap statistics, including largest free block and minimum free heap. Zero-initializing the structure removes the invalid read, but reporting zero as measured LVGL usage would still be misleading. Use the public monitoring wrapper only where the allocator supports meaningful metrics.

## Module assessment and improvements

| Area | Verdict | Assessment and next improvement |
| --- | --- | --- |
| Control dispatcher, STOP generation, snapshots | Core Asset | Preserve single ownership and queued commands. Add command receipts for every START and a cancellation token for deferred UI actions. |
| Motor/FastAccelStepper wrapper | Core Asset | Explicit RMT selection, checked library return values and cleanup ownership are useful. Add checked pulse/rate planning and share stop supervision across all modes. |
| Speed/input/geometry | Extract & Merge | Centralize source arbitration and a pure, checked motion planner. Production code should be the same code tested on the host. |
| Calibration session | Core Asset | Draft isolation, interrupted-move rejection, a separate verification move and save receipts are useful. Add precision/uncertainty information and a bounded control-owned move deadline. |
| Setup wizard | Core Asset | Preserve observed start/stop/reset sequence and saved-stage requirements. Add explicit checkpoints for physical direction, microstep DIP settings and the actual E-STOP interface. |
| Programs/editors | Extract & Merge | Preserve root drafts and sub-editor Cancel behavior. Unify direction/name/range policies and report queued/saved/failed outcomes visibly. |
| LVGL screens/theme/display HAL | Core Asset | Preserve approved graphite/orange design and single UI owner. Use action IDs rather than label text to disable stale actions; update only changed labels/styles. Remove obsolete layout constants and API comments. |
| NVS/legacy migration | Core Asset | Debounce, generations and retry behavior are useful. Harden serialization, size/type validation and migration commit/rollback. |
| USB mirror/protocol/viewer | Core Asset | Keep bounded frames, CRC, local arming, keepalive and release on disconnect. Bound RX work per cycle; reset partial-frame parsers after inactivity; release pointer on focus loss; bound viewer writes. |
| Simulator/test adapters | Extract & Merge | Actual UI/control coverage is valuable, but speed, storage, safety and hardware are substituted. Share production policy and add a real storage-parser fixture. |
| CI/release packaging | Core Asset | Keep pinned versions, three firmware builds, binary identity, checksums and license packaging. Tie firmware metadata to build-time identity and stage all release outputs before publication. |
| Vendored panel/touch drivers | Core Asset | Review at integration boundaries; preserve provenance. Unused EK79007/RGB paths are not runtime evidence for the ST7701 MIPI board. Keep upstream upgrades separate and qualify on hardware. |
| Legacy duplicated test helpers/design generators | Deprecate selectively | Retire helpers after equivalent production tests exist. Archive obsolete design generators; retain current V5 generation/auditing. Translate the remaining Norwegian extension-repair script if it remains public. |

No entire production subsystem needs a rewrite. Refactor shared policies incrementally and protect the existing useful behavior.

### Storage hardening to implement and verify

Check `JsonDocument::overflowed()` before measuring or writing. A partially populated document can still serialize to nonzero bytes, so `written != 0` alone does not prove the complete settings/program were saved. This is a source-identified gap; allocation failure was not injected into the real storage module in this review. The [ArduinoJson overflow documentation](https://arduinojson.org/v7/api/jsondocument/overflowed/) explains this condition.

Require serialized byte count to match the measured size and `putBytes()` to return the exact requested count. Bound stored blob sizes before allocation; reject non-object preset entries; normalize or reject duplicate/out-of-range IDs. Validate legacy files before committing the NVS migration. Add program save receipts rather than relying on `storage_save_presets()` returning true when a write has only been queued.

### Kinematics improvement requiring hardware confirmation

Current conversions always use 108:1 reduction and the part-diameter/80 mm roller ratio. That is appropriate only for the represented mechanism. A direct chuck/table should use its own transmission model. Add explicit machine profiles only after confirming the mechanical drive; this review does not establish that the existing mechanism is wrong.

### UI improvements without another redesign

Show requested and effective speed clearly when a limit applies. Show effective direction consistently. Use the same adjustment precision in every mode. Keep the prominent fault overlay; cancel deferred actions behind it. Display program save progress/failure, and review angle/repeats/dwell or ON/OFF/cycles before a program starts. Use one coherent draft/save policy for Display settings so theme reconstruction does not discard an unsaved dim-time selection.

The automated 800×480 layout audit passed its current screen/scenario checks. That is not an exhaustive proof for all dynamic strings, maximum values, keyboard overlays, fault combinations or display hardware. Add boundary-value snapshots instead of changing layout based solely on default screenshots.

## Baseline verification and coverage

| Check | Result |
| --- | --- |
| `platformio test -e native -e native-control` | **438/438 passed** |
| `python -m unittest discover -s test/tooling -v` | **10/10 passed** |
| Existing LVGL simulator `--self-test` | **PASS** |
| Existing LVGL simulator `--audit-layout` | **0 failures** |
| `python scripts/test_fas_rmt.py` against the pinned library | **PASS** |
| Targeted diagnostic reproductions | **Nine problematic behaviors reproduced**, detailed below |
| Debug CLIB monitoring inspection | Unsupported core function and uninitialized caller confirmed in source |
| Physical E-STOP/ALM/STEP/DIR, TIG HF immunity, NVS power-loss behavior | Not measured in this review |

The native-control suite compiles the real dispatcher, motor wrapper, modes and calibration, but substitutes speed and storage. The UI simulator also substitutes ADC, storage and safety/hardware. Many native helpers duplicate production behavior. These distinctions explain how all baseline tests can pass while the above defects remain.

Review scope covered startup/task scheduling, cross-core state ownership, control admission/transitions, all operating modes, geometry/speed and ADS arbitration, motor configuration, calibration/setup, persistence/migration, screen registry and all screen categories, display/touch integration, USB mirror/viewer, host adapters/tests and build/release tooling. Generated font glyph data and visual asset payloads were classified as data. Vendored drivers and downloaded dependencies were assessed at the used integration/API boundaries; this is not a complete upstream-library audit or a formal concurrency proof.

Verified pins: LVGL **9.6.0**, FastAccelStepper **1.4.0** at `f24a65996bdc501a0497cdca6a636706003333e7`, ArduinoJson **7.4.3**, pioarduino `55.03.312-1`. Firmware compilation was not repeated because no firmware source was changed during this review; host regressions and targeted diagnostics were run locally.

Local diagnostic runner: `.pio/run_review_repros.py` (requires the existing Windows MinGW/SDL simulator build and Unity fixture). It compiles ignored harnesses that include actual production files or reuse the simulator's compiled objects. It does not flash the device. These diagnostic runs demonstrate current behavior; they are not yet permanent acceptance tests.

```text
JOG: cap=0.100 stored=1.000 commandHz=1000.000 state=4
PULSE STUCK STOP: elapsed=15000ms running=1 state=2 fault=0
LOW RPM: requested=0.001000 rawHz=5.400001 commandHz=20.000000 effectiveRPM=0.003704
STEP RANGE: sizeof(long)=4 expected=2592000000 converted=-2147483648 max=2147483647
PERSISTED UTF8 NAME: N+U00F8+r -> N??r
PEDAL UI OVERRIDE: stationary pedalADC=1000 panelADC=3000 requested=0.800 result=2.095 source=2
COUNTDOWN AFTER RESET: state=0
COUNTDOWN AFTER ZERO, NO NEW START: state=1 running=1
BEFORE NEW PROGRAM physicalDirection=1
NEW PROGRAM rawDirection=1 physicalDirectionWhenRun=0
STEP MINUS: before=0.020 after=0.020
```

State IDs: 0 idle, 1 running, 2 pulse, 4 jog. Direction IDs: 0 CW, 1 CCW. Input source 2 is pedal. The jog fixture maps 1 RPM to 1000 Hz for dispatch verification; that diagnostic Hz value is not the real machine conversion.

## Original recommended implementation order

1. Cancel deferred starts across STOP/fault/reset; clamp jog at execution; reject overflowing pulse counts.
2. Add bounded pulse/finite-move supervision and production regression tests for those fault paths.
3. Introduce a shared checked motion planner and active-input arbitration; fix low-speed feasibility and ADC baselines.
4. Unify program direction/names and UI precision; add save receipts and storage failure tests.
5. Improve diagnostics, clean obsolete helpers/comments, and broaden simulator snapshots.
6. Build all three firmware variants, then bench-check physical input polarity, direction timing, low-speed pulses, stop behavior and power-loss recovery before publishing corrected binaries.
