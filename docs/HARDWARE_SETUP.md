# TIG Welding Positioner — Hardware Setup Guide

This guide details the physical electrical wiring and structural assembly required to build the welding positioner safely.

## 1. Electrical Component Selection
- **ESP32-P4 Dev Board:** GUITION JC4880P443C (4.3" MIPI-DSI display, ESP32-P4 + ESP32-C6 co-processor).
- **Micro-Stepper Driver:** Common **PUL/DIR** boards and/or **DM542T** (Leadshine-class DSP). Must handle motor supply (often 20–50 V DC on `VM`/`V+`) and motor current for NEMA 23. **Logic supply for PUL/DIR/ENA is separate** (typically 3.3 V or 5 V on the `+` inputs — do not feed motor `VM` to the opto inputs).
- **Motor:** NEMA 23 Stepper Motor (approx 3.0 Nm holding torque recommended).
- **Power Supply (PSU):** **36V DC** is optimal on the stepper `VM` bus for headroom; **24V DC** also works within typical driver input range (e.g. 18–50 V). Minimum **5.0 A** — size wattage for your chosen rail (~180 W @ 36 V, ~120 W @ 24 V as a rough guide). Dedicated for the stepper driver.
- **DC-DC Converter:** Step-down buck converter (**36V** or **24V** → **5V**, 2 A) to power the ESP32-P4 if not using USB-C.
- **Enclosure:** **Metallic (Aluminum/Steel) grounded enclosure**. Required for TIG HF-start welding. Put the ESP32-P4 screen, stepper driver, and motor PSU inside the same enclosure.

## 2. DIP stepper driver configuration (basic PUL/DIR boards)
Before powering on, configure the DIP switches on the side of your driver board.

| Setting | Value | Required for firmware |
|---------|-------|-----------------------|
| **Microsteps** | Selectable: 1/4, 1/8, 1/16, 1/32 | Must match Settings > Microstepping in UI |
| **Current** | Match your NEMA 23 rating | Typically 2.5A - 3.0A |

## 3. DM542T checklist (arrival / commissioning)

Tables below match the **STEPPERONLINE DM542T** front label (same family as many “DM542T” boards). **SW = ON** means the switch position printed **ON** on that driver (confirm on your unit).

### 3.1 Signal voltage (PWR / logic jumper)

The driver has a **5 V / 24 V** selector for the **control input** side (often labeled near **PWR** / alarm). For **ESP32-P4** (3.3 V GPIO):

- Use the **5 V** logic position and wire **PUL+/DIR+/ENA+** to **3.3 V or 5 V** as in §3.4 — **not** motor `VM` (24–36 V) on the opto inputs.
- If pulses were ignored before, wrong voltage on this selector or on **+** inputs is a common cause.

### 3.2 Motor current — SW1, SW2, SW3 (peak / RMS)

| Peak | RMS | SW1 | SW2 | SW3 |
|------|-----|-----|-----|-----|
| 1.00 A | 0.71 A | ON | ON | ON |
| 1.46 A | 1.04 A | OFF | ON | ON |
| 1.91 A | 1.36 A | ON | OFF | ON |
| 2.37 A | 1.69 A | OFF | OFF | ON |
| **2.84 A** | **2.03 A** | **ON** | **ON** | **OFF** |
| 3.31 A | 2.36 A | OFF | ON | OFF |
| 3.76 A | 2.69 A | ON | OFF | OFF |
| 4.50 A | 3.20 A | OFF | OFF | OFF |

There is **no exact 2.7 A** row. For ~2.7 A class torque use **2.84 A peak** (**SW1=ON, SW2=ON, SW3=OFF**). If the motor stalls under load, step up toward **3.31 A** (within motor/driver ratings).

**SW4 (standstill current):** **OFF** = half current when idle, **ON** = full current when idle (more hold, more heat).

### 3.3 Pulses per revolution — SW5–SW8 (must match UI microstep)

Firmware assumes **200 full steps/rev** at the motor and **microstep factor** from **Motor Config** (e.g. **16** = **1/16** → **3200** pulses per motor revolution).

| Pulses/rev | SW5 | SW6 | SW7 | SW8 |
|------------|-----|-----|-----|-----|
| 800 (1/4) | ON | OFF | ON | ON |
| 1600 (use for **1/8** in UI) | **OFF** | **OFF** | ON | ON |
| **3200** (default **1/16** in UI) | **ON** | **OFF** | **OFF** | **ON** |
| 6400 (1/32) | OFF | ON | OFF | ON |

*Some DM542T boards use **SW6 ON** for 3200; follow the row on **your** driver label that matches **3200** pulses/rev.*

Pick the row that equals **200 × (Motor Config microstep value)**. Wrong table vs UI = wrong speed, noise, or stalling (“crash”).

### 3.4 Firmware

| Item | Setting |
|------|--------|
| **Microstep** | **1/16** in **Motor Config** if DIP = **3200** pulses/rev (recommended) |
| **Driver** | **DM542T** + **SAVE & APPLY** |

**Motor power:** **+Vdc** = **+18 V … +50 V** (motor supply), **GND** = power return. Do **not** connect motor `VM` to **PUL/DIR/ENA**.

**Signal wiring (optocoupler “+” to logic VCC, “−” to GPIO — Style 2):**

| Driver terminal | Connect to |
|-----------------|------------|
| **PUL+** | **3.3 V** on ESP32-P4 header (or **5 V** via series resistor e.g. **1 kΩ** if your manual recommends it) |
| **PUL−** | **GPIO 50** (STEP) |
| **DIR+** | Same logic **3.3 V** (or 5 V as above) |
| **DIR−** | **GPIO 51** (DIR) |
| **ENA+** | Same logic **3.3 V** (or 5 V as above) |
| **ENA−** | **GPIO 52** (ENA) |

**ALM+ / ALM−:** fault output (open-drain, **active LOW** on alarm for typical DM542T wiring to ESP GND). Firmware reads **GPIO 32** (`PIN_DRIVER_ALM`, `INPUT_PULLUP`). Wire so the pin is **HIGH when OK** and **pulled LOW on driver fault** (follow your driver manual; often ALM− to signal GND and ALM+ / opto side per datasheet). **RESET** on the ESTOP overlay stays disabled until ALM is high again and the physical E-STOP is released.

- **Common GND:** Driver signal **GND** / **`COM`** / logic reference must tie to **ESP32 GND**.
- Inputs are usually **5 V tolerant**; **3.3 V** on the `+` side often works with the opto current your manual specifies.

**Pulse timing (datasheet vs firmware):**

- DM542T typically specifies **≥ 2.5 µs** minimum step pulse width.
- This project uses **FastAccelStepper** with **ESP32-P4 RMT** (`gin66/FastAccelStepper@^0.33.14`). There is **no** supported `platformio.ini` flag named `FAS_MIN_PULSE_WIDTH_TICKS` in that library version; timing is defined inside the library (`MIN_CMD_TICKS` / RMT symbol encoding). If you see missed steps, first match **DIP microstep** to the UI and **Motor Config driver** to **DM542T**, then check wiring and **VM** motor supply — do not assume ~100 ns pulses at the opto without measuring.

**RPM sweep note:** Absolute workpiece limits are **`MIN_RPM` … `MAX_RPM`** in `src/config.h` (currently **0.001 … 3.0 RPM**). The **Max RPM** value in **Motor Config** (stored in NVS) sets the pot/slider ceiling up to that cap.

## 4. Wiring Connections

### A. Stepper motor to driver (PUL/DIR / DM542T)
Identify your stepper motor pairs (use a multimeter on continuity mode if unsure).
- **A+ / A-:** Coil Pair 1
- **B+ / B-:** Coil Pair 2

### B. ESP32-P4 to stepper driver (signal wires)

Opto-isolated drivers support **two common layouts**. Your **PCB silkscreen** and **manual** decide which one applies — mixing them causes no motion or wrong enable polarity.

**Style 1 — “Minus to GND, plus to MCU”** (common on some **DIP breakout** boards; matches older wiring diagram v2 signal routing):

1. Tie **PUL−**, **DIR−**, **ENA−** together → **GND** on the ESP32-P4.
2. **PUL+** → `GPIO 50` (STEP)
3. **DIR+** → `GPIO 51` (DIR)
4. **ENA+** → `GPIO 52` (ENA)

**Style 2 — “Plus to VCC, minus to MCU”** (common on **DM542T** / Leadshine-style inputs; see **§3 DM542T checklist**):

1. **PUL+**, **DIR+**, **ENA+** → **3.3 V** (or **5 V** per manual, sometimes via **1 kΩ**).
2. **PUL−** → `GPIO 50`, **DIR−** → `GPIO 51`, **ENA−** → `GPIO 52`.
3. **GND** common between ESP32 and driver logic / `COM` as required by the manual.

Firmware: **LOW** on the ENA GPIO line = **motor enabled**, **HIGH** = disabled (after stop). If direction or enable behaves inverted, swap **Style 1 vs 2** or use **Invert direction** in Motor Config — do not change ENA safety semantics without testing.

### C. External Controls
- **Speed Potentiometer (10k Linear):**
  - Pin 1 (CCW) -> GND
  - Pin 2 (Wiper) -> `GPIO 49`
  - Pin 3 (CW) -> 3.3V
- **E-STOP (NC / active LOW on press, matches firmware):**
  - Wire so that **released (safe) = logic HIGH** and **pressed = logic LOW** on `GPIO 34` (firmware uses `INPUT_PULLUP` and a **FALLING** interrupt).
  - `src/config.h` notes that some ESP32-P4 strapping guidance applies to GPIO34 — keep leads short; add external pull-up / RC filter per [EMI_MITIGATION.md](EMI_MITIGATION.md) if the line is noisy.
- **Direction Switch (CW/CCW Toggle):**
  - Pin 1 -> `GPIO 29`
  - Pin 2 -> GND
  - Uses internal INPUT_PULLUP. Toggle via Settings > Motor Config > Direction Switch.
- **Foot Pedal (Optional):**
  - Potentiometer → **ADS1115 AIN0** on touch I2C (see `ENABLE_ADS1115_PEDAL` / project README), not the panel pot pin
  - Start switch → `GPIO 33` (INPUT_PULLUP, LOW = pressed)
  - **ADS1115 not detected (I2C scan shows 0x5D but never 0x48–0x4B):** firmware uses **GPIO7 = SDA**, **GPIO8 = SCL** (`I2C_NUM_0` in `display.cpp`) — the **same bus** as GT911 and (on JC4880P443) the **ES8311** codec (often **0x18** on scan). On the **JP1** expansion header (odd row), **pin 23 = I2C_SDA** and **pin 25 = I2C_SCL** per Guition pinout — wire ADS **D** and **S** there, **not** to pins **24 / 26** (ESP32-C6 / `C6_IO9` / `C6_CHIP_PU`). Some online notes mention “Wire1 / I2C1”; this project does **not** use a second I2C bus for touch — external ADS must join **GPIO7/8**. Verify continuity, try **swap D and S** once if still no ACK, and **V → 3.3 V**, **G → GND**.

### D. Reserved Pins (Do Not Use)
- **GPIO 28**, **GPIO 14-19**, and **GPIO 54** may be PCB-routed toward the ESP32-C6 co-processor. Treat as reserved unless the GUITION schematic confirms safe repurposing for your build. **GPIO 32 is intentionally used by this firmware for DM542T ALM**; do not reuse it for other signals.

## 5. Safety & Thermal Management
- **Heat Sinking:** Stepper drivers get hot during long welding runs. Mount the driver to the metal enclosure for thermal dissipation or add a 40mm fan.
- **Power Sequencing:** Always power the logic (USB-C or DC-DC) first, then the **motor supply** (24 V or 36 V on `VM`). The firmware holds the motor ENA pin HIGH (Disabled) on boot for safety.
- **TIG HF Noise:** Open-bench wiring is only for motion/firmware testing. For welding, install the ESP32-P4 screen, stepper driver, and motor PSU inside the same grounded metal enclosure. Without this, HF start can reset the controller, freeze the touch/I2C bus, or create false GPIO/ADC inputs.

## 6. Validated Hardware

| Component | Tested Model | Status |
|-----------|-------------|--------|
| **MCU Board** | GUITION JC4880P443C (ESP32-P4 + C6) | Tested |
| **Stepper Driver** | PUL/DIR boards; DM542T | Standard drivers tested (1/4, 1/8); DM542T field checklist §3 |
| **Stepper Motor** | NEMA 23 (3 Nm) | Tested |
| **Gearbox** | NMRV030 + spur, **1:108** total | Tested |
| **Power Supply** | 36V DC (optimal); 24V DC works | Tested |
| **Potentiometer** | 10k (LA42DWQ-22) | Tested (ADC range 0-3315) |
| **E-STOP** | NC Button | Tested |
| **Direction Switch** | SPDT toggle on GPIO 29 | Tested |
| **Foot Pedal** | Analog pot + momentary switch | Tested |
| **TIG HF welding** | Shared grounded metal enclosure for ESP32-P4 screen, driver, and PSU | Field tested |

### Known limitations (basic PUL/DIR drivers)
- Motor resonance at 100-300 motor RPM causes stalling at coarse microstepping (1/4)
- Recommended: **1/16** (3200) with DM542T for smoother low-RPM motion; 1/8 or finer is acceptable
- **DM542T:** use §3 checklist; set **Motor Config → DRIVER** to match hardware
