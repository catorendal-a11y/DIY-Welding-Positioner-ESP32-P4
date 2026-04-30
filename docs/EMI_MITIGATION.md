# TIG High Frequency (HF) EMI Mitigation Guide

High Frequency (HF) start on TIG welders emits massive amounts of Electromagnetic Interference (EMI).
This EMI can easily reset microcontrollers, freeze I2C touch screens, or cause random stepper motor jitter.

Real TIG welding validation has been completed with this controller. The system worked during welding after the ESP32-P4 screen, stepper driver, and motor PSU were installed inside the same grounded metal enclosure. Treat that enclosure as a required part of the build, not an optional accessory.

To ensure your ESP32-P4 positioner runs reliably in an industrial welding environment, implement these hardware isolation techniques:

## 1. Grounding (The Most Critical Step)
- **Star Grounding:** Do not daisy-chain ground wires. Run individual ground lines from the stepper driver, the ESP32 power converter, and the enclosure directly to a single central grounding point.
- **Welding Table Isolation:** The positioner chassis MUST be electrically bonded to the welding table ground, but the ESP32 control logic GND must remain separated from the welding earth ground loop.
- **Metal Enclosure:** House the ESP32-P4 screen, stepper driver, and motor PSU in the same grounded **aluminum or steel enclosure**. Plastic cases and open-bench wiring provide zero protection against the high-frequency RF noise of a TIG arc.
- **Short Chassis Bond:** Bond the enclosure to protective earth / welding chassis ground with a short, low-impedance strap or wire.

## 2. Cable Shielding
- Use **shielded cables** for the STEP, DIR, and ENA signal lines between the ESP32 and the stepper driver.
- Use **shielded cables** for the stepper motor coils (A+, A-, B+, B-).
- Use shielded cable for external E-STOP, foot pedal, potentiometer, and any I2C extension that leaves the enclosure.
- Ground the shielding braided mesh on **ONE END ONLY** (typically at the controller enclosure side). Grounding both ends creates a ground loop antenna.

## 3. Optoisolation
Many stepper drivers have internal optocouplers separating the ESP32 3.3V logic circuit from the high-voltage motor supply (often **24–36 V DC** on `VM`; **36 V** is optimal for headroom where the hardware allows it).
- Ensure you do not accidentally bridge the ESP32 ground and the motor power return. They should only interact through the optocouplers inside the driver.

## 4. Ferrite Chokes
Snap-on ferrite bead chokes absorb high-frequency EMI spikes.
- Place a ferrite choke on the USB-C power cable right where it enters the ESP32.
- Place a ferrite choke on the stepper motor cable right where it exits the driver.
- Place a ferrite choke on the E-STOP logic wire to prevent the long wire from acting as an antenna and triggering false E-STOPS.
- Add ferrites on pedal, potentiometer, external signal, and PSU input cables if HF start still causes resets or touch glitches.

## Field Validation Checklist

- [x] Real TIG welding tested with the controller operating the positioner.
- [x] Reliable operation achieved with ESP32-P4 screen, driver, and PSU inside one grounded metal enclosure.
- [ ] Verify your own enclosure bond before production use.
- [ ] Repeat HF arc-start test after any cable routing or enclosure change.

## 5. E-STOP Hardware Filtering (RC Filter)
If EMI causes false E-STOP triggers on `GPIO 34`, adding a hardware RC (Resistor-Capacitor) low-pass filter to the pin is highly recommended.
- Place a `100nF` ceramic capacitor between `GPIO 34` and `GND`, physically as close to the ESP32 pin as possible.

## 6. Transient Voltage Suppression (TVS)
For maximum reliability, add **TVS Diodes** (e.g., SMF3.3) on the E-STOP and Potentiometer wiper lines. These shunt high-voltage transients to ground before they can reach the ESP32-P4's internal junction.

## 7. C6 Co-Processor Considerations
The GUITION board routes a high-speed bus between ESP32-P4 and the on-board ESP32-C6. Keep those PCB traces and any related cabling away from TIG HF noise sources; follow the vendor layout for any custom shielding.
