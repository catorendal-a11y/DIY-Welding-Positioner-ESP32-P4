# Code review — 1 October 2026

Historical review before fixes: branch `codex/ui-v3-integration`, based on `929b1f6` (v2.0.9), including uncommitted UI integration. Locations below refer to that earlier working copy. See [implementation and final verification](IMPROVEMENTS_2026-10-01.md) for subsequent changes.

## Scope and verification

Reviewed source architecture, motor/safety flows, all screen roles/navigation, storage, USB mirror, simulator, tests and build/release configuration. Detailed reading prioritized risk and included the GT911 failure path. This was not an exhaustive third-party audit or hardware acceptance test. SVGs do not establish actual LVGL geometry.

| Check at review time | Result |
| --- | --- |
| Release build | Passed; static RAM 32,732 bytes, flash 1,038,260 bytes, not peak heap/PSRAM. |
| Native tests | 395/395 passed, three suites. |
| CMake/MinGW simulator | Built. |
| Simulator self-test | Construction/theme pass; functional flow exits 3: `label target not found: > START on MAIN`. |
| Diff check | Extra EOF blank line in `screen_main.cpp`. |
| Physical/device checks | No motor, oscilloscope or flashing tests. |

Old button text caused simulator/UI drift, not proof of a defective START callback. Later checks did not run. SDL dummy driver initially could not create a window; normal SDL produced the result above.

## Highest-priority findings

### 1. P1 — STOP overwritten by START

**Location:** `control.cpp:72,324`. Shared one-element `xQueueOverwrite` loses STOP when followed by START. Separate peek/receive permits STOP_JOG to consume normal STOP in the wrong branch.

**Fix/test:** Separate priority STOP latch; invalidate start generations; receive once. Test STOP→START, START→STOP, STOP_JOG interleaving, queue full and repeated starts against production dispatch with a fake motor.

### 2. P1 — Pedal starts at boot/after short release

**Location:** `main.cpp:166,203`. Previous state false treats an enabled held pedal as a new press. Release only stops RUNNING and leaves queued starts while IDLE.

**Fix/test:** Stable release before boot/enable/reset arming; cancel pending pedal starts on release; identify source ownership. Test held boot, short press before tick, bounce and reset while held. UI guards alone do not fix startup.

### 3. P1 — Stale pedal speed/source fallback

**Location:** `speed.cpp:105,329,392`. Old ADS1115 value has no age limit; availability reflects startup probe; values outside 100–3900 automatically choose panel input. Freshness return behavior differs from its comment.

**Fix/test:** Publish value/validity/time/error together; stop on stale active input and require rearm; explicit source selection. Test disconnect, repeated timeout, rail shorts and reconnect.

### 4. P1 — Stale touch keeps JOG pressed

**Location:** `lvgl_hal.cpp`, touch callback; GT911 driver `:223,307`. Ignored read result can return before clearing points; old coordinates remain PRESSED. JOG depends on release events, and stalled UI has no renewal timeout.

**Fix/test:** Release on read errors; require fresh periodic JOG renewal. Inject I2C failure after a press and suspend UI; measure stop deadline physically.

### 5. P1 — Motion gate omits pending/latched faults

**Location:** `safety.cpp:123`, `motor_run_cw/ccw`. Checks omit `g_estopPending` and `estopLocked`. A brief LOW edge followed by HIGH can permit ENA LOW before fault latching.

**Fix/test:** Common latched gate at every enable; explicit reset event. Repeated reads are not atomic enable. Inject faults before/between/after enable checks and measure ENA during flash writes.

### 6. P1 — Indefinite safety mutex wait

**Location:** safety force-stop; `control.cpp:176`; `motor_halt`. Initial cleanup has 10 ms timeout, but subsequent transition restores acceleration/halts with `portMAX_DELAY`. Immediate ISR ENA action does not bound later processing.

**Fix/test:** Publish fault without locks/logging; defer blocking cleanup; measure under contention.

### 7. P1 — Opposite diagram/code polarity

**Location:** original timing guide NC-to-GND diagram; `safety.cpp:109`. Diagram gives LOW healthy/HIGH open; code faults on LOW/FALLING. Documentation conflict confirmed; actual machine unexamined. ENA HIGH disable is also an assumption.

**Fix:** Verify normal/pressed/cable-break/supply-loss truth table, then align code/diagrams. Do not change polarity from this review alone.

### 8. P1 — Unchecked critical task startup

**Location:** `main.cpp:346`, watchdog init. Task and watchdog result codes ignored; success logged regardless.

**Fix:** Gate motion on explicit READY messages; handle allocation/watchdog errors as startup failure. Successful task creation alone is insufficient.

## Functional and maintenance findings

### 9. P2 — Draft resets after mode editing

**Location:** `screens.cpp:60,98`, `screen_program_edit.cpp:359`, mode save callbacks. Rebuild plus cleared `pendingEditSlot` returns a default New Program, losing draft/identity.

**Fix/test:** Persistent edit session/ID separate from widget lifetime; load only on new/edit. Test name/RPM/mode/ID round trips.

### 10. P2 — Untouched panel overrides program speed

**Location:** program `speed_slider_set`, `speed.cpp:414`, `config.h:93`. Static panel/program difference above 0.04 triggers takeover without panel movement.

**Fix:** Define source ownership; require movement or setpoint crossing for takeover. Snapshot speed/diameter/direction together.

### 11. P2 — Failed writes lose pending changes

**Location:** `storage.cpp:375,387`, motor-config save callback. Dirty clears before write; result ignored; UI claims saved early. Failed boot loads may continue with defaults.

**Fix/test:** Changed/pending/saved/error states, retained generation, backoff, root/schema validation, distinguish first use/corruption. Test full NVS, failure, power loss, invalid root and concurrent changes.

### 12. P2 — 300-second dimming becomes 44

**Location:** `storage.h:28`, `lvgl_hal.cpp:36`, `screen_display.cpp:116`. `uint8_t` overflows in storage and HAL; local UI widening is insufficient.

**Fix/test:** Consistent 16-bit field and allowed values, documented 44→300 migration. Save/reload 0/30/60/120/300.

### 13. P2 — Incomplete UI/stale simulator expectations

**Location:** `theme.h:145,308`, `screen_timer.cpp:161`, simulator. New header conflicts with old countdown/calibration positions. Editor/config/STEP layouts still need integration; old START text blocks tests.

**Fix/test:** Shared bounds and stable action IDs. Inspect rendered LVGL with long names, themes, disabled controls, overlays/keyboards. SVG checks alone are insufficient.

### 14. P2 — Pulse OFF includes braking

**Location:** `pulse_update`. Pause begins at stop request, while deceleration continues.

**Fix/test:** Define whether OFF means stop request or standstill. Separate stopping/pause and start pause after motor-reported stop. Test short OFF at low acceleration.

### 15. P2 — Tests model rather than exercise production

**Location:** native `test_build_src=false`, control/speed helper headers. Passing 395 models does not prove actual queue, pedal, task or NVS-error behavior.

**Fix:** Shared production policies with clock/motor/GPIO/storage adapters; simulator for UI and hardware for timing/electrical behavior.

### 16. P2 — USB send loop unbounded

**Location:** `serial_write_all`. Repeated zero-byte writes prevent return to parser. Independent keepalive checking means this does not prove JOG always sticks, but mirror/reconnect can stall.

**Fix:** Send deadline, disconnect abort/chunk cleanup/new session, controlled redraw after dropped regions.

## Further improvements and implementation order

Use one motor-command owner and coherent cross-core UI snapshots; atomic state does not synchronize all later ordinary fields. Use consistent ordinary/recursive LVGL mutex APIs. Label calculated speed/angle and use feedback for slip/lost steps. Expand CI and record dependency/tool/commit provenance. Reduce warning suppression and maintain readable callbacks. Measure worst-case timing rather than treating comments such as “<1 ms” as results.

1. STOP priority, pedal arming, input faults, latched gate and production-policy tests.
2. Verify wiring/startup failure/timing on a controlled bench without welding.
3. Drafts, speed ownership, save status, dimming and pulse semantics.
4. Finish UI integration and pass all simulator flows.
5. CI and hardware I2C/UI-stall/power-loss/TIG-noise testing.

This review did not itself change firmware or publish code; it describes the pre-fix working copy.
