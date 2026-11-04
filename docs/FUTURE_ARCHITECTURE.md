# Future Architecture

This document outlines planned features and their current status.

---

## 1. Closed-Loop Encoder Feedback

> **STATUS: DEFERRED / OPTIONAL**
> Current firmware uses open-loop pulse-derived speed. It cannot detect mechanical slip or missed steps. Encoder feedback remains an optional improvement for measured speed and deviation monitoring.

---

## 2. Alternate operator / remote interfaces

> **STATUS: NOT IN SCOPE**
> Operator control is touch UI, potentiometer, direction switch, and optional foot pedal. A second MCU may exist on the dev board for factory use; this repository does not ship a wireless companion controller. Optional USB-C mirroring is implemented and requires physical-screen arming.

### Already in product
- Countdown before start (3-2-1 on screen) — implemented in current firmware

---

## 3. Program Preset Storage

> **STATUS: IMPLEMENTED**
> Up to 16 presets saved to **NVS** (JSON blob `prs`) with full CRUD from UI; legacy LittleFS files are migrated once if present.

---

## 4. DM542T Driver Support

> **STATUS: PARTIAL**
> Motor Config has a DM542T driver mode, and the firmware reads the ALM input and waits 200 ms after ENA before the first pulse. Microstepping goes up to 1/32. The speed limit is `MAX_RPM` (3.0), further capped per diameter by `MAX_STEP_HZ` and `MOTOR_RPM_LIMIT`.

### What's Needed
- Decide whether `MAX_RPM` should rise above 3.0 with DM542T
- Test anti-resonance DSP at higher microstepping
- Validate step accuracy at higher speeds

---

## 5. Enclosure & Assembly

> **STATUS: PLANNED**
> 3D-printable enclosure design files and assembly guide.

---

## 6. Higher RPM

> **STATUS: PLANNED**
> Requires field data with DM542T. The default Max RPM setting is 3.0, the absolute `MAX_RPM` ceiling.
