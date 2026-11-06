# Load Test — Thermal Validation

## Hardware: GUITION JC4880P443C (ESP32-P4 + ESP32-C6)
## Firmware under test: [FILL IN version and exact commit before testing]
## Test Date: [FILL IN AFTER HARDWARE TEST]

> [!NOTE]
> This is a blank test sheet. Fill in the tables during the test. Nothing here is a measured result.

---

## Test Setup

| Item | Value |
|------|-------|
| **Workpiece** | 50 kg steel pipe |
| **Diameter** | 300 mm |
| **RPM** | 1.0 test setpoint; firmware absolute maximum is 3.0 RPM, subject to saved motor settings |
| **Duration** | 60 minutes continuous |
| **Ambient Temperature** | ___ C |
| **Motor** | NEMA 23 (3 Nm rated) |
| **Driver** | PUL/DIR (motor supply 24V or 36V; **36V** optimal) |

### Procedure

1. **Mount** the 50 kg, 300 mm steel pipe and note the ambient temperature.
2. **Record** the start temperature of the motor, the driver and the ESP32-P4 board.
3. **Run** continuous rotation at 1.0 RPM for 60 minutes.
4. **Check** step accuracy every 10 minutes.
5. **Record** the temperatures again at 30 min and 60 min.
6. **Record** idle, running and peak current draw.

---

## Temperature Measurements

| Component | Start (C) | 30 min (C) | 60 min (C) | Max Rating | Result |
|-----------|-----------|-------------|-------------|------------|--------|
| Motor | ___ | ___ | ___ | 80C | [ ] |
| Driver | ___ | ___ | ___ | 80C | [ ] |
| ESP32-P4 board | ___ | ___ | ___ | 60C | [ ] |

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
| 24–36V (motor) | ___ | ___ | ___ |

---

## Notes

[Add observations during testing]
