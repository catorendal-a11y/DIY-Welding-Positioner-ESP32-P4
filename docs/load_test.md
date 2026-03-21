# Load Test — Thermal Validation

## Hardware: WT32-SC01 Plus (ESP32-S3-WROVER-N16R2) PRD v7.0
## Test Date: [FILL IN AFTER HARDWARE TEST]

---

## Test Setup

- **Workpiece:** 50 kg steel pipe
- **Diameter:** 300 mm
- **RPM:** 5.0 (maximum)
- **Duration:** 60 minutes continuous
- **Ambient Temperature:** ___ °C
- **Motor:** NEMA 23 (1.8 Nm rated)
- **Driver:** TB6600 (24V supply)

---

## Temperature Measurements

| Component | Start (°C) | 30 min (°C) | 60 min (°C) | Max Rating | Result |
|-----------|------------|-------------|-------------|------------|--------|
| Motor | ___ | ___ | ___ | 80°C | [ ] |
| TB6600 | ___ | ___ | ___ | 80°C | [ ] |
| WT32-SC01+ | ___ | ___ | ___ | 60°C | [ ] |

---

## Step Accuracy Check

| Time (min) | Expected Steps | Actual Steps | Error | Result |
|------------|----------------|--------------|-------|--------|
| 10 | ___ | ___ | ___ | [ ] |
| 20 | ___ | ___ | ___ | [ ] |
| 30 | ___ | ___ | ___ | [ ] |
| 40 | ___ | ___ | ___ | [ ] |
| 50 | ___ | ___ | ___ | [ ] |
| 60 | ___ | ___ | ___ | [ ] |

---

## Current Draw

| Component | Idle (mA) | Running (mA) | Peak (mA) |
|-----------|-----------|--------------|------------|
| 5V (logic) | ___ | ___ | ___ |
| 24V (motor) | ___ | ___ | ___ |

---

## Phase 6 Gate

- [ ] docs/load_test.md — PASS
- [ ] No missed steps over 60 minutes
- [ ] Motor < 70°C (with margin)
- [ ] TB6600 < 80°C
- [ ] WT32-SC01+ < 60°C

---

## Notes

[Add observations during testing]
