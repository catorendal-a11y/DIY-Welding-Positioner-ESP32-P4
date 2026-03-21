# ESTOP Timing Verification

## Hardware: WT32-SC01 Plus (ESP32-S3-WROVER-N16R2) PRD v7.0
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Required Hardware RC Filter

**IMPORTANT:** Install this RC filter on the Extended IO header before final testing.

```
  3.3V ──[10 kΩ]──[GPIO 14]──[NC ESTOP contact]──[GND]
                       └──[100 nF ceramic]──[GND]
```

**RC time constant:** 10 kΩ × 100 nF = 1 ms

**Effect:**
- Blocks TIG HF glitches (2–3 MHz) ← eliminated
- Blocks contact bounce (< 1 ms)    ← eliminated
- Passes real ESTOP press (> 5 ms)  ← confirmed by SW debounce

**Without this filter,** a TIG arc start within 30 cm of the ESTOP cable can trigger a false emergency stop.

**Components:**
- Resistor: 10 kΩ, 1/4W, 5% (carbon film or metal film)
- Capacitor: 100 nF (0.1 µF), ceramic, 50V+, X7R or better

**Installation location:** On the Extended IO header (GPIO 10-14), as close to the ESP32-S3 pins as possible.

---

## Test Setup

- **Oscilloscope:** Channel 1 = GPIO 14 (ESTOP signal), Channel 2 = GPIO 13 (ENA)
- **ESTOP Configuration:** Active LOW, NC contact, 10kΩ pull-up
- **Target:** GPIO 14 falling → GPIO 13 rising < 1.0 ms

---

## Implementation

### Layer 1: ISR Response (< 0.5 ms)
```cpp
void IRAM_ATTR estopISR() {
    digitalWrite(PIN_ENA, HIGH);   // Direct GPIO write
    stepper->forceStop();
    g_estopPending = true;
}
```

### Layer 2: State Transition (< 5 ms)
```cpp
// safetyTask checks g_estopPending every 1ms
// After 5ms debounce, transitions to STATE_ESTOP
```

---

## Test Measurements

| Test # | GPIO 14→13 (µs) | Result | Notes |
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

### Worst-Case Latency
- **Measured:** ___ µs
- **Requirement:** < 1000 µs (1 ms)
- **Result:** [PASS / FAIL]

---

## NC Cable-Cut Test

- [ ] ESTOP cable cut → Motor disabled
- [ ] STATE_ESTOP reached within 5 ms
- [ ] No false triggers during normal operation

---

## Phase 3 Gate

- [ ] docs/estop_timing.md — PASS
- [ ] Worst-case ESTOP latency < 1.0 ms
- [ ] NC cable-cut test passes
- [ ] WDT resets in < 2 s, boot is fail-safe

---

## Notes

[Add any observations during testing]
