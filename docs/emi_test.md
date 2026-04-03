# EMI Test — TIG Welder HF Arc Start

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware: v2.0.0
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Test Setup

- **TIG Welder:** [Fill in model]
- **HF Arc Start:** Enabled
- **Distance:** < 50 cm from controller
- **Controller State:** Running at 0.5 RPM
- **Grounding:** Controller ground connected to welder ground

---

## Test Criteria

| # | Test | Pass Criteria | Result |
|---|------|---------------|--------|
| 1 | No system reset during HF arc start | No ESP_RST_WDT | [ ] |
| 2 | No GUI freeze during welding | Touch responsive | [ ] |
| 3 | STEP jitter < 1 us during welding | Oscilloscope verified | [ ] |
| 4 | ADC variation < +/-0.05 RPM | Stable speed | [ ] |
| 5 | WiFi/BLE remains connected | No disconnection | [ ] |

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

### SDIO Stability (WiFi/BLE)

| Condition | WiFi Status | BLE Status | Result |
|-----------|-------------|------------|--------|
| Idle (no weld) | Connected | Connected | |
| HF Arc Start | ___ | ___ | |
| During Weld | ___ | ___ | |

---

## Mitigation Applied

- [ ] Shielded cables for all motor connections
- [ ] Ferrite beads on ESTOP lines
- [ ] Proper grounding chassis to welder ground
- [ ] Metal enclosure
- [ ] RC filter on GPIO 34 (100nF)

---

## Notes

[Add observations during testing]
