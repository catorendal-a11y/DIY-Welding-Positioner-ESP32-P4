# ESTOP Timing Verification

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware: v2.0.0
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Hardware Configuration

- **E-STOP Pin**: GPIO 34 (INPUT_PULLUP, NC contact)
- **ENA Pin**: GPIO 52 (Output, active LOW)
- **Internal Pull-up**: Enabled (no external resistor needed)

```
  3.3V (internal pull-up)──[GPIO 34]──[NC ESTOP contact]──[GND]
```

**Optional RC Filter** (for EMI-prone environments):
```
  [GPIO 34]──[100 nF ceramic]──[GND]
```
RC time constant: ~1 ms. Blocks TIG HF glitches and contact bounce.

---

## Test Setup

- **Oscilloscope:** Channel 1 = GPIO 34 (ESTOP signal), Channel 2 = GPIO 52 (ENA)
- **ESTOP Configuration:** Active LOW, NC contact, internal pull-up
- **Target:** GPIO 34 rising (ESTOP press) -> GPIO 52 rising (motor disabled) < 1.0 ms

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
// safetyTask (priority 5) checks g_estopPending every 1ms
// After 5ms debounce, transitions to STATE_ESTOP via CAS
```

### Layer 3: UI Overlay (< 200 ms)
```cpp
// lvglTask detects STATE_ESTOP, shows full-screen red overlay
// Reset via g_uiResetPending flag processed in controlTask
```

---

## Test Measurements

| Test # | GPIO 34->52 (us) | Result | Notes |
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
- **Measured:** ___ us
- **Requirement:** < 1000 us (1 ms)
- **Result:** [PASS / FAIL]

---

## NC Cable-Cut Test

- [ ] ESTOP cable cut -> Motor disabled
- [ ] STATE_ESTOP reached within 5 ms
- [ ] No false triggers during normal operation

---

## Notes

[Add any observations during testing]
