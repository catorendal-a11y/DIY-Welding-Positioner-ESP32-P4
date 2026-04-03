# TIG High Frequency (HF) EMI Mitigation Guide

High Frequency (HF) start on TIG welders emits massive amounts of Electromagnetic Interference (EMI).
This EMI can easily reset microcontrollers, freeze I2C touch screens, or cause random stepper motor jitter.

To ensure your ESP32-P4 positioner runs flawlessly in an industrial welding environment, implement these hardware isolation techniques:

## 1. Grounding (The Most Critical Step)
- **Star Grounding:** Do not daisy-chain ground wires. Run individual ground lines from the stepper driver, the ESP32 power converter, and the enclosure directly to a single central grounding point.
- **Welding Table Isolation:** The positioner chassis MUST be electrically bonded to the welding table ground, but the ESP32 control logic GND must remain separated from the welding earth ground loop.
- **Metal Enclosure:** House the entire controller in a grounded **aluminum or steel enclosure**. Plastic cases provide zero protection against the high-frequency RF noise of a TIG arc.

## 2. Cable Shielding
- Use **shielded cables** for the STEP, DIR, and ENA signal lines between the ESP32 and the TB6600.
- Use **shielded cables** for the stepper motor coils (A+, A-, B+, B-).
- Ground the shielding braided mesh on **ONE END ONLY** (typically at the controller enclosure side). Grounding both ends creates a ground loop antenna.

## 3. Optoisolation
The TB6600 stepper driver already has internal optocouplers separating the ESP32 3.3V logic circuit from the 24V motor drive circuit.
- Ensure you do not accidentally bridge the ESP32 ground and the 24V motor power ground. They should only interact through the optocouplers inside the TB6600.

## 4. Ferrite Chokes
Snap-on ferrite bead chokes absorb high-frequency EMI spikes.
- Place a ferrite choke on the USB-C power cable right where it enters the ESP32.
- Place a ferrite choke on the stepper motor cable right where it exits the TB6600.
- Place a ferrite choke on the E-STOP logic wire to prevent the long wire from acting as an antenna and triggering false E-STOPS.

## 5. E-STOP Hardware Filtering (RC Filter)
If EMI causes false E-STOP triggers on `GPIO 34`, adding a hardware RC (Resistor-Capacitor) low-pass filter to the pin is highly recommended.
- Place a `100nF` ceramic capacitor between `GPIO 34` and `GND`, physically as close to the ESP32 pin as possible.

## 6. Transient Voltage Suppression (TVS)
For maximum reliability, add **TVS Diodes** (e.g., SMF3.3) on the E-STOP and Potentiometer wiper lines. These shunt high-voltage transients to ground before they can reach the ESP32-P4's internal junction.

## 7. C6 Co-Processor Considerations
The ESP32-P4 communicates with the ESP32-C6 co-processor over SDIO for WiFi and BLE. This shared bus is sensitive to EMI:
- Keep SDIO traces (GPIO 14-19) short and away from motor power cables.
- EMI-induced SDIO errors can cause WiFi disconnections or BLE data corruption.
- If WiFi/BLE becomes unstable during welding, add additional ferrite chokes near the ESP32-P4 board.
