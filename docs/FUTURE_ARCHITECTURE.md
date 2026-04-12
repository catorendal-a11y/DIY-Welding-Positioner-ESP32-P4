# Future Architecture

This document outlines planned features and their current status.

---

## 1. Closed-Loop Encoder Feedback

> **STATUS: CANCELLED**
> The open-loop stepper setup is precise enough for welding applications. Encoder feedback adds complexity without meaningful benefit.

---

## 2. Alternate operator / remote interfaces

> **STATUS: NOT IN SCOPE**
> Operator control is touch UI, potentiometer, direction switch, and optional foot pedal. A second MCU may exist on the dev board for factory use; this repository does not ship remote operator panels or companion apps.

### Already in product
- Countdown before start (3-2-1 on screen) — implemented in current firmware

---

## 3. Program Preset Storage

> **STATUS: IMPLEMENTED**
> Up to 16 presets saved to **NVS** (JSON blob `prs`) with full CRUD from UI; legacy LittleFS files are migrated once if present.

---

## 4. DM542T Driver Support

> **STATUS: PARTIAL**
> Motor config UI supports microstepping up to 1/32. MAX_RPM limited to 1.0 with standard driver timing by default.

### What's Needed
- Increase MAX_RPM to 5.0 when DM542T is connected
- Test anti-resonance DSP at higher microstepping
- Validate step accuracy at higher speeds

---

## 5. Enclosure & Assembly

> **STATUS: PLANNED**
> 3D-printable enclosure design files and assembly guide.

---

## 6. Higher RPM

> **STATUS: PLANNED**
> Requires DM542T driver upgrade for comfortable headroom. Default MAX_RPM keeps ~1.0 RPM workpiece speed.
