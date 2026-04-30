# EMI Test — TIG Welder HF Arc Start

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware: v2.0.9
## Test Date: 2026-04-30

---

## Test Setup

- **TIG Welder:** [Fill in model]
- **HF Arc Start:** Enabled
- **Distance:** < 50 cm from controller
- **Controller State:** Running at 0.5 RPM
- **Enclosure:** ESP32-P4 screen, stepper driver, and motor PSU installed inside the same metal enclosure
- **Grounding:** Metal enclosure bonded to protective earth / welding chassis ground

---

## Test Criteria

| # | Test | Pass Criteria | Result |
|---|------|---------------|--------|
| 1 | No system reset during HF arc start | No ESP_RST_WDT | PASS |
| 2 | No GUI freeze during welding | Touch responsive | PASS |
| 3 | STEP jitter < 1 us during welding | Oscilloscope verified | Not measured |
| 4 | ADC variation < +/-0.05 RPM | Stable speed | Not measured |
| 5 | No spurious I2C / touch lockups | GT911 stays responsive | PASS |

---

## Measurements

### STEP Signal Stability (GPIO 50)

| Condition | Jitter (us) | Result |
|-----------|-------------|--------|
| Idle (no weld) | ___ | |
| HF Arc Start | ___ | |
| During Weld | ___ | |

### ADC Noise (GPIO 49 - Potentiometer)

| Condition | RPM Variation | Result |
|-----------|---------------|--------|
| Idle (no weld) | +/- ___ | |
| HF Arc Start | +/- ___ | |
| During Weld | +/- ___ | |

### P4 ↔ C6 bus (board-dependent)

| Condition | Bus / co-processor stable | Result |
|-----------|---------------------------|--------|
| Idle (no weld) | ___ | |
| HF Arc Start | ___ | |
| During Weld | ___ | |

---

## Mitigation Applied

- [x] ESP32-P4 screen, stepper driver, and motor PSU inside the same metal enclosure
- [x] Proper grounding chassis to welder ground
- [x] Metal enclosure
- [ ] Shielded cables for all motor connections
- [ ] Ferrite beads on ESTOP lines
- [ ] RC filter on GPIO 34 (100nF)

---

## Notes

Initial TIG welding test showed that open / separated electronics were disrupted by TIG HF noise. Reliable operation was achieved after the power supply, stepper driver, and ESP32-P4 screen were installed in the same grounded metal enclosure.
