# ESTOP Timing Verification

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware: v2.0.8 (see `FW_VERSION` in `src/config.h`)
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Hardware Configuration

- **E-STOP pin**: GPIO 34 (`INPUT_PULLUP`, active **LOW** when faulted — must match wiring in `docs/HARDWARE_SETUP.md`).
- **ENA pin**: GPIO 52 (output; **HIGH** = driver disabled for this firmware’s opto wiring).
- **Pull-up**: Firmware enables `INPUT_PULLUP` on GPIO 34. For long/noisy leads, prefer an **external** pull-up and optional RC per [EMI_MITIGATION.md](EMI_MITIGATION.md) (see also `PIN_ESTOP` notes in `src/config.h`).

**Example topology (one valid arrangement — confirm against your NC routing):**

```
  3.3V (pull-up)──[GPIO 34]── … NC ESTOP chain … ──[GND / open on fault]
```

**Optional RC filter** (EMI-prone environments):

```
  [GPIO 34]──[100 nF ceramic]──[GND]
```

RC time constant: ~1 ms. Blocks TIG HF glitches and contact bounce.

---

## Test setup

- **Oscilloscope:** Channel 1 = GPIO 34 (ESTOP sense), Channel 2 = GPIO 52 (ENA).
- **Sense:** Fault = GPIO 34 driven **LOW** (FALLING edge arms ISR after idle HIGH).
- **Target:** GPIO 34 **falling** (fault) → GPIO 52 **rising** (motor disabled / ENA de-asserted) **< 1.0 ms**.

---

## Implementation (reference)

### Layer 1: ISR (< ~0.5 ms)

```cpp
void IRAM_ATTR estopISR() {
  GPIO.out1_w1ts.val = (1UL << (PIN_ENA - 32));  // ENA HIGH -> disabled
  g_estopPending.store(true, std::memory_order_release);
  g_wakePending.store(true, std::memory_order_release);  // wake backlight if dimmed
}
```

(No `digitalWrite`, no stepper calls, no `millis()` in ISR — matches `src/safety/safety.cpp`. Flags are declared in `src/app_state.h`.)

### Layer 2: State transition (< ~5 ms)

`safetyTask` acquire-loads `g_estopPending`, debounces it, records `g_estopTriggerMs` on the debounced edge, then calls `control_transition_to(STATE_ESTOP)` via CAS.

### Layer 2b: Boot-time sampling

`safety_init()` samples `PIN_ESTOP` 3× with 500 µs spacing after `INPUT_PULLUP` + 2 ms settle; requires ≥2/3 LOW to treat ESTOP as pressed. GPIO34 has no internal pull-up on ESP32-P4, so this avoids false power-on ESTOPs from a floating line.

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
