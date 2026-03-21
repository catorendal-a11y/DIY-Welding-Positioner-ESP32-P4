# EMI Test — TIG Welder HF Arc Start

## Hardware: WT32-SC01 Plus (ESP32-S3-WROVER-N16R2) PRD v7.0
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Test Setup

- **TIG Welder:** [Fill in model]
- **HF Arc Start:** Enabled
- **Distance:** < 50 cm from controller
- **Controller State:** Running at 3 RPM
- **Grounding:** Controller ground connected to welder ground

---

## Test Criteria

| # | Test | Pass Criteria | Result |
|---|------|---------------|--------|
| 1 | No system reset during HF arc start | No ESP_RST_WDT | [ ] |
| 2 | No GUI freeze during welding | Touch responsive | [ ] |
| 3 | STEP jitter < 1 µs during welding | Oscilloscope verified | [ ] |
| 4 | ADC variation < ±0.05 RPM | Stable speed | [ ] |

---

## Measurements

### STEP Signal Stability (GPIO 11)

| Condition | Jitter (µs) | Result |
|-----------|-------------|--------|
| Idle (no weld) | ___ | |
| HF Arc Start | ___ | |
| During Weld | ___ | |

### ADC Noise (GPIO 10 - Potentiometer)

| Condition | RPM Variation | Result |
|-----------|---------------|--------|
| Idle (no weld) | ± ___ | |
| HF Arc Start | ± ___ | |
| During Weld | ± ___ | |

---

## Mitigation Applied

- [ ] Shielded cables for all motor connections
- [ ] Ferrite beads on ESTOP lines
- [ ] Proper grounding chassis to welder ground
- [ ] PSRAM/cache fix enabled in platformio.ini

---

## Phase 6 Gate

- [ ] docs/emi_test.md — PASS
- [ ] No system reset during HF arc start
- [ ] GUI responsive during welding
- [ ] STEP jitter < 1 µs maintained

---

## Notes

[Add observations during testing]
