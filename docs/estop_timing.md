# ESTOP Timing Verification

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware: v2.0.9 (see `FW_VERSION` in `src/config.h`)
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Hardware Configuration

- **E-STOP pin**: GPIO 34 (`INPUT_PULLUP`, active **LOW** when faulted — must match wiring in `docs/HARDWARE_SETUP.md`).
- **ENA pin**: GPIO 52 (output; **HIGH** = driver disabled for this firmware’s opto wiring).
- **Pull-up**: Firmware enables `INPUT_PULLUP` on GPIO 34. For long/noisy leads, prefer an **external** pull-up and optional RC per [EMI_MITIGATION.md](EMI_MITIGATION.md) (see also `PIN_ESTOP` notes in `src/config.h`).

**Firmware input contract (physical wiring still requires verification):**

| Condition | GPIO34 required | Firmware action |
| --- | --- | --- |
| Healthy, released | HIGH | Start allowed only after task readiness and other interlocks |
| E-STOP activated | LOW | ENA HIGH immediately, latched motion fault |
| E-STOP input cable broken | Must be mapped to LOW by external supervision | Same latched fault |
| Controller power absent | External hardware must establish the driver-safe state | Software cannot guarantee outputs |

A pull-up with a bare NC contact to GND produces LOW when healthy and HIGH
when open. **That circuit is incompatible with the retained active-LOW firmware.**
Use a verified, supervised interface that meets the table, or deliberately change
the input contract and retest every safety path. Do not infer wiring from a pin name.
ENA HIGH=disabled is an unverified assumption for the user's actual driver/opto wiring.
The former NC-to-GND example was removed because it contradicted the firmware.

**Optional RC filter** (EMI-prone environments):

```
  [GPIO 34]──[100 nF ceramic]──[GND]
```

The time constant depends on the actual pull resistance and wiring. Measure the resulting input delay and noise response on the assembled machine.

---

## Test setup

- **Oscilloscope:** Channel 1 = GPIO 34 (ESTOP sense), Channel 2 = GPIO 52 (ENA).
- **Sense:** Fault = GPIO 34 driven **LOW** (FALLING edge arms ISR after idle HIGH).
- **Target:** GPIO 34 **falling** (fault) → GPIO 52 **rising** (motor disabled / ENA de-asserted) **< 1.0 ms**.

---

## Implementation (reference)

### Layer 1: ISR (latency to be measured)

```cpp
void IRAM_ATTR estopISR() {
  GPIO.out1_w1ts.val = (1UL << (PIN_ENA - 32));  // ENA HIGH -> disabled
  g_estopPending.store(true, std::memory_order_release);
  g_wakePending.store(true, std::memory_order_release);  // wake backlight if dimmed
}
```

(No `digitalWrite`, no stepper calls, no `millis()` in ISR — matches `src/safety/safety.cpp`. Flags are declared in `src/app_state.h`.)

### Layer 2: State transition (latency requires measurement)

`safetyTask` acquire-loads `g_estopPending`, debounces it, records `g_estopTriggerMs` on the debounced edge, then publishes `STATE_ESTOP` without waiting for motor cleanup. Cleanup runs in controlTask.

### Layer 2b: Boot-time sampling

`safety_init()` samples `PIN_ESTOP` 3× with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle; requires ≥2/3 LOW to treat ESTOP as pressed. Startup sampling does not replace input conditioning or cable-break supervision.

### Layer 3: UI overlay

`lvglTask` shows the full-screen ESTOP overlay; `estop_overlay_show()` calls `dim_reset_activity()` so the operator sees the fault UI even after dim timeout.

---

## Test measurements

| Test # | GPIO 34→52 (µs) | Result | Notes |
|--------|-----------------|--------|-------|
| 1 | ___ | ___ | |
| 2 | ___ | ___ | |
| 3 | ___ | ___ | |
| 4 | ___ | ___ | |
| 5 | ___ | ___ | |
| 6 | ___ | ___ | |
| 7 | ___ | ___ | |
| 8 | ___ | ___ | |
| 9 | ___ | ___ | |
| 10 | ___ | ___ | |

### Worst-case latency

- **Measured:** ___ µs
- **Requirement:** < 1000 µs (1 ms)
- **Result:** [PASS / FAIL]

---

## Bench checklist

- [ ] ESTOP activated → motor disabled / ENA safe state
- [ ] `STATE_ESTOP` reached within ~5 ms after debounce
- [ ] No false triggers during normal operation (with EMI mitigations if needed)

---

## Notes

[Add any observations during testing]
