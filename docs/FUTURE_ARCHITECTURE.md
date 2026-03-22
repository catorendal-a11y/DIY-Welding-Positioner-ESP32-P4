# 🚀 Future Architecture: The Next Generation

This document outlines the architectural blueprints for the three major upcoming features in the TIG Rotator Controller project. These features elevate the system from a precision DIY controller to a fully-fledged industrial product.

---

## 1. Closed-Loop Encoder Feedback

In an open-loop system, if physical resistance exceeds the stepper motor's torque (e.g., a heavy pipe binding), the motor skips steps. The firmware has no way of knowing this, resulting in an inaccurate final weld angle.

### Hardware Pipeline
- **Sensor:** A magnetic absolute encoder (e.g., **AS5600** via I2C) or a high-resolution optical quadrature encoder attached to the main drive shaft or motor shaft.
- **Connection:** Wired to the ESP32-P4 using dedicated GPIO pins with hardware pull-ups.

### Software Architecture (ESP-IDF PCNT)
- **Zero-CPU Counting:** Instead of using software interrupts (which steal CPU time and can crash the LVGL UI), we will utilize the ESP32-P4's **Pulse Counter (PCNT)** hardware peripheral.
- **Background Task:** A new `encoderTask` running on Core 0 (alongside `motorTask`) will periodically read the PCNT hardware registry.
- **Validation Logic:** 
  The task compares the expected steps (from `FastAccelStepper`) against the physical rotation (from the PCNT).
  ```cpp
  int32_t expected_pos = stepper->getCurrentPosition();
  int16_t actual_pos = pcnt_get_count(unit);
  
  if (abs(expected_pos - actual_pos) > TOLERANCE_STEPS) {
      control_transition_to(STATE_FAULT);
      ui_show_error_dialog("POSITION LOSS: MECHANICAL BINDING DETECTED");
  }
  ```

---

## 2. Wi-Fi / Web Panel Remote Control (ESP32-C6)

The ESP32-P4 lacks integrated Wi-Fi and Bluetooth to dedicate silicon entirely to processing power and high-speed I/O. To add wireless control, we use an affordable **ESP32-C6** as a Network Co-Processor (NCP).

### Hardware Pipeline
- **Component:** A standard ESP32-C6 SuperMini or DevKit.
- **Interconnect:** High-speed Hardware UART (e.g., `UART2` on P4 connected to `UART1` on C6) running at a baud rate of `921600` or higher, or a dedicated SPI bus.

### Software Architecture
- **ESP32-C6 Role (Web Server):**
  - Connects to the local shop Wi-Fi (or broadcasts its own Access Point).
  - Runs an `AsyncWebServer`.
  - Hosts a modern single-page web app (React, Vue, or clean HTML/JS) compiled into a compressed gzip file stored in the C6's flash.
  - Maintains a full-duplex **WebSocket** connection with the operator's phone/tablet.
- **Protocol (ArduinoJson):**
  When the operator changes the RPM slider on their phone, the C6 receives the WebSocket message and forwards a JSON packet over UART to the P4:
  `{"cmd": "set_rpm", "val": 12.5}`
- **ESP32-P4 Role:**
  A new `networkTask` (Core 1) constantly listens to the UART buffer. Upon receiving a valid JSON object, it calls the existing, thread-safe API:
  `speed_set_target_rpm(doc["val"]);`
  This enables bidirectional state syncing — the LVGL display on the console and the phone screen will perfectly mirror each other in real-time.

---

## 3. Program Preset Storage (LittleFS + ArduinoJson)

Operators frequently perform repetitive welds. Setting the exact RPM, pulse timing, and step angles manually each time is inefficient.

### Memory Architecture
- **Partition Table:** Modify the PlatformIO `partitions.csv` to reserve 2MB of the ESP32-P4's onboard flash memory for **LittleFS** (a flash-wear leveling file system infinitely superior to SPIFFS).
- **Data Structure:** Presets will be serialized as JSON using the `ArduinoJson` library, allowing for massive flexibility if we add new parameters in the future without breaking old save files.

### Implementation Flow
1. **The Save File (`/littlefs/presets.json`):**
   ```json
   [
     {
       "id": 1,
       "name": "3-inch SS Pulse",
       "mode": "PULSE",
       "rpm": 5.5,
       "pulse_on_ms": 800,
       "pulse_off_ms": 500,
       "dir": "CW"
     }
   ]
   ```
2. **Boot Sequence:** 
   During `setup()`, the ESP32 mounts LittleFS, reads the JSON file into a C++ `std::vector<Preset>`, and injects the array into `screen_programs.cpp` to populate the LVGL roller/list UI.
3. **Saving a Program:** 
   When the user edits a program and taps "Save" on the LVGL touchscreen, the firmware updates the C++ struct, calls `serializeJson()`, overwrites `/littlefs/presets.json`, and flashes a minimal "Program Saved" toast notification.

---

### Implementation Phasing
When we are ready to code these, they should be implemented completely independently in this logical order:
1. **Phase 1: LittleFS Presets** (Pure software update, highly visible UI improvement).
2. **Phase 2: Closed-Loop PCNT** (Requires mounting the encoder and routing safe shielded cables).
3. **Phase 3: C6 Web Remote** (Requires flashing a secondary MCU and writing an asynchronous web frontend).
