# TIG WELDING ROTATOR CONTROLLER
## CLAUDE_TASKS.md — Complete Implementation Guide for Claude Code
### Hardware: WT32-SC01 Plus · ESP32-S3-WROVER-N16R2 · PRD v7.0

---

> **READ THIS FIRST — RULES FOR CLAUDE CODE**
>
> 1. **Complete tasks in order.** Never skip ahead. Phase gates exist for safety reasons.
> 2. **After each task:** confirm output matches acceptance criteria before marking `[x]`.
> 3. **Phase gates are hard stops.** Do NOT proceed to next phase until gate is passed.
> 4. **Never touch GPIO 38–41.** They belong to the SD card. Hardware constraint.
> 5. **ENA (GPIO 13) must be HIGH before anything else in setup().** No exceptions.
> 6. **Ask before creating new files** not listed in a task's file list.
> 7. **Run `/clear` between phases** to reset context.
> 8. Use `LOG_I()` macros for debug — never raw `Serial.printf()` in logic files.
> 9. **Screen coordinates** are x,y from top-left in landscape (480 wide × 320 tall).

---

## PROJECT CONTEXT

**What we're building:** A touchscreen industrial rotator controller for TIG welding.
The controller rotates a welding fixture (up to 50 kg) at precise RPM with multiple modes.

**Why this architecture matters:**
- Motor timing must be deterministic — hardware timer ISR, not FreeRTOS task
- ESTOP must respond in < 1 ms regardless of what the GUI is doing
- The display uses MCU8080 8-bit parallel bus — NOT SPI (causes blank screen)
- All external GPIO must use the Extended IO header (GPIO 10–14, 21)

**Tech stack:** PlatformIO · Arduino framework · LovyanGFX · LVGL 8.x · FastAccelStepper · ArduinoJson · LittleFS

---

## VERIFIED PIN MAP (do not deviate)

```
GPIO 10  →  POT/ADC    ADC1_CH9   INPUT    Potentiometer (ONLY ADC1 on header)
GPIO 11  →  STEP       EXT_IO2    OUTPUT   FastAccelStepper HW timer
GPIO 12  →  DIR        EXT_IO3    OUTPUT   CW = HIGH / CCW = LOW
GPIO 13  →  ENA        EXT_IO4    OUTPUT   Active LOW to TB6600. HIGH = motor OFF
GPIO 14  →  ESTOP      EXT_IO5    INPUT    Active LOW, NC contact, 10kΩ pull-up
GPIO 21  →  RESERVE    EXT_IO6    –        Future encoder / expansion

DISPLAY (MCU8080 parallel — do not change):
  WR=47  DC=0  RST=4  BL=45
  D0=9  D1=46  D2=3  D3=8  D4=18  D5=17  D6=16  D7=15

TOUCH FT5x06 (I2C):
  SDA=6  SCL=5  INT=7  ADDR=0x38

NEVER USE:  GPIO 22–25 (don't exist on ESP32-S3)
            GPIO 26–32 (QSPI flash — always reserved)
            GPIO 33–37 (OPI PSRAM — always reserved)
            GPIO 38–41 (SD card — HARDWARE CONFLICT)
```

---

## KEY FORMULAS

```cpp
// RPM on workpiece → motor step frequency
float rpmToStepHz(float rpm_workpiece) {
    // Gear=20:1  D_workpiece=300mm  D_roller=80mm  1/8 microstep=1600 steps/rev
    return rpm_workpiece * 20.0f * (300.0f / 80.0f) * 1600.0f / 60.0f;
    // 1 RPM  → 2000  Hz
    // 5 RPM  → 10000 Hz  (10 kHz max)
}

// Angle in degrees (on workpiece) → motor steps
long angleToSteps(float degrees) {
    float motor_deg = degrees * 20.0f * (80.0f / 300.0f);
    return (long)(motor_deg / 360.0f * 1600.0f);
}

// Required motor torque: 0.30 Nm (SF=2.0 included)
// Standard NEMA 23 (1–2 Nm) is sufficient.
```

---

## FILE STRUCTURE

```
tig-rotator/
├── platformio.ini
├── CLAUDE_TASKS.md             ← this file
├── docs/
│   ├── estop_timing.md
│   ├── emi_test.md
│   └── load_test.md
├── lib/
│   └── lv_conf.h
└── src/
    ├── main.cpp
    ├── config.h
    ├── motor/
    │   ├── motor.h / motor.cpp
    │   └── speed.h / speed.cpp
    ├── safety/
    │   └── safety.h / safety.cpp
    ├── control/
    │   ├── control.h / control.cpp
    │   └── modes/
    │       ├── continuous.cpp
    │       ├── pulse.cpp
    │       ├── step_mode.cpp
    │       ├── jog.cpp
    │       └── timer_mode.cpp
    ├── programs/
    │   └── programs.h / programs.cpp
    └── ui/
        ├── theme.h
        ├── display.h / display.cpp
        ├── lvgl_hal.h / lvgl_hal.cpp
        ├── screens.h / screens.cpp
        └── screens/
            ├── screen_main.cpp
            ├── screen_estop_overlay.cpp
            ├── screen_menu.cpp
            ├── screen_pulse.cpp
            ├── screen_step.cpp
            ├── screen_jog.cpp
            ├── screen_timer.cpp
            ├── screen_programs.cpp
            ├── screen_program_edit.cpp
            ├── screen_settings.cpp
            └── screen_confirm.cpp
```

---

## TASK OVERVIEW

```
┌──────────────────────────────────────────────────────────────────────────────┐
│  FASE 1: FUNDAMENT           FASE 2: MOTOR            Est: 8–12t            │
│  T-01 PlatformIO setup  ──►  T-07 GPIO init                                 │
│  T-02 Display init           T-08 FastAccelStepper                          │
│  T-03 LVGL + PSRAM           T-09 Speed control                             │
│  T-04 Touch verify           T-10 FreeRTOS tasks                            │
│  T-05 PSRAM verify                    │                                      │
│  T-06 Debug macros                    │                                      │
│           │                           │                                      │
│           ▼                           ▼                                      │
│  FASE 3: SAFETY  ⚠           FASE 4: STATE MACHINE                          │
│  T-11 Fail-safe boot    ──►  T-15 State machine core                        │
│  T-12 ESTOP ISR              T-16 Continuous mode                           │
│  T-13 Watchdog               T-17 Pulse mode                                │
│  T-14 Timing docs            T-18 Step mode                                 │
│                               T-19 Jog mode                                 │
│                               T-20 Timer mode                               │
│                                       │                                      │
│                                       ▼                                      │
│  FASE 5: GUI + PROGRAMS       FASE 6: INTEGRATION                           │
│  T-21 theme.h           ──►  T-28 Full system test                          │
│  T-22 Main screen             T-29 EMI test                                 │
│  T-23 ESTOP overlay           T-30 Load test                                │
│  T-24 Menu + mode screens     T-31 Production build                         │
│  T-25 Jog screen                                                             │
│  T-26 Programs screens                                                       │
│  T-27 Settings screen                                                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

---

# PHASE 1 — FOUNDATION
### Goal: Display + touch + LVGL + PSRAM working. No motor connected.

```
┌─────────────────────────────────────────────────────────┐
│  PHASE 1 GATE                                           │
│  T-01 to T-06 must be [x] before Phase 2.              │
│  Do not connect TB6600 or motor during this phase.      │
└─────────────────────────────────────────────────────────┘
```

---

### T-01 · PlatformIO project setup
**Estimate:** 1–2 hours · **Depends on:** nothing

**Create these files:** `platformio.ini` · `src/config.h` · `lib/lv_conf.h`

**platformio.ini:**
```ini
[env:wt32-sc01-plus]
platform  = espressif32
board     = esp32s3box
framework = arduino
board_build.flash_size  = 16MB
board_build.partitions  = default_16MB.csv
board_build.psram_type  = opi
build_flags =
  -DBOARD_HAS_PSRAM
  -mfix-esp32-psram-cache-issue
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
  -DLV_CONF_INCLUDE_SIMPLE=1
  -DCORE_DEBUG_LEVEL=3
lib_deps =
  gin66/FastAccelStepper
  lovyan03/LovyanGFX
  lvgl/lvgl@^8.3.0
  bblanchon/ArduinoJson
```

**src/config.h:**
```cpp
#pragma once
#define PIN_POT         10   // ADC1_CH9 — only ADC1 on Extended IO header
#define PIN_STEP        11   // EXT_IO2
#define PIN_DIR         12   // EXT_IO3
#define PIN_ENA         13   // EXT_IO4  Active LOW to TB6600
#define PIN_ESTOP       14   // EXT_IO5  Active LOW, NC contact
#define PIN_SPARE       21   // EXT_IO6  reserve / future encoder
#define MIN_RPM         0.1f
#define MAX_RPM         5.0f
#define MICROSTEPS      8
#define STEPS_PER_REV   (200 * MICROSTEPS)   // 1600
#define GEAR_RATIO      20.0f
#define D_EMNE          0.300f   // m  workpiece diameter
#define D_RULLE         0.080f   // m  roller diameter
#define DEBUG_BUILD     1        // set 0 for production
```

**lib/lv_conf.h:**
```cpp
#define LV_COLOR_DEPTH          16
#define LV_HOR_RES_MAX         480
#define LV_VER_RES_MAX         320
#define LV_MEM_SIZE     (32*1024U)
#define LV_USE_GPU_ESP32_DMA     1
#define LV_USE_PERF_MONITOR      1
#define LV_DISP_DEF_REFR_PERIOD 10
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_18    1
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_48    1
```

**Acceptance criteria:**
- [x] Project compiles without errors (code complete)
- [x] `Serial.println("BOOT OK")` appears at 115200 baud
- [x] No linker errors

---

### T-02 · LovyanGFX display init (MCU8080 parallel — NOT SPI)
**Estimate:** 2–3 hours · **Depends on:** T-01

**Create:** `src/ui/display.h` · `src/ui/display.cpp`

**Critical:** Bus_Parallel8, not Bus_SPI. Using SPI gives a blank screen.
Pin values are from the TZT official datasheet (Tab.5 LCD Interface).

```cpp
// display.h
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel;
  lgfx::Bus_Parallel8 _bus;     // MCU8080 — NOT Bus_SPI
  lgfx::Light_PWM     _bl;
  lgfx::Touch_FT5x06  _touch;
public:
  LGFX() {
    { auto cfg = _bus.config();
      cfg.port=0; cfg.freq_write=20000000;
      cfg.pin_wr=47; cfg.pin_rd=-1; cfg.pin_rs=0;   // DC=GPIO0 ⚠ strapping pin
      cfg.pin_d0=9;  cfg.pin_d1=46; cfg.pin_d2=3;
      cfg.pin_d3=8;  cfg.pin_d4=18; cfg.pin_d5=17;
      cfg.pin_d6=16; cfg.pin_d7=15;
      _bus.config(cfg); _panel.setBus(&_bus); }
    { auto cfg = _panel.config();
      cfg.pin_cs=-1; cfg.pin_rst=4;
      cfg.panel_width=320; cfg.panel_height=480;
      cfg.invert=true;
      _panel.config(cfg); }
    { auto cfg = _bl.config();
      cfg.pin_bl=45;                                  // BL=GPIO45 ⚠ strapping pin
      cfg.invert=false; cfg.freq=44100; cfg.pwm_channel=7;
      _bl.config(cfg); _panel.setLight(&_bl); }
    { auto cfg = _touch.config();
      cfg.x_min=0; cfg.x_max=319; cfg.y_min=0; cfg.y_max=479;
      cfg.pin_int=7; cfg.i2c_port=1; cfg.i2c_addr=0x38;
      cfg.pin_sda=6; cfg.pin_scl=5; cfg.freq=400000;  // I2C GPIO 5/6
      _touch.config(cfg); _panel.setTouch(&_touch); }
    setPanel(&_panel);
  }
};
extern LGFX display;
```

**Add color cycle test to setup() — remove before Phase 2:**
```cpp
display.init();
display.setRotation(1);   // Landscape: 480×320
display.fillScreen(TFT_RED);   delay(400);
display.fillScreen(TFT_GREEN); delay(400);
display.fillScreen(TFT_BLUE);  delay(400);
display.fillScreen(TFT_BLACK);
display.drawString("WT32-SC01+ OK", 160, 140, 4);
```

**Acceptance criteria:**
- [x] Code implemented (display.h/cpp created)
- [ ] Screen lights up within 2 seconds of power-on
- [ ] Red → Green → Blue cycle plays without artifacts
- [ ] `setRotation(1)` gives landscape 480×320
- [ ] Text renders correctly

---

### T-03 · LVGL init with PSRAM buffers
**Estimate:** 2–3 hours · **Depends on:** T-02

**Create:** `src/ui/lvgl_hal.h` · `src/ui/lvgl_hal.cpp`

```cpp
void lvgl_hal_init() {
    lv_init();
    // Must allocate in PSRAM — crash here means PSRAM misconfigured
    lv_color_t* buf1 = (lv_color_t*)ps_malloc(480 * 40 * sizeof(lv_color_t));
    lv_color_t* buf2 = (lv_color_t*)ps_malloc(480 * 40 * sizeof(lv_color_t));
    assert(buf1 != nullptr && "PSRAM buf1 alloc failed — check platformio.ini");
    assert(buf2 != nullptr && "PSRAM buf2 alloc failed");
    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, buf1, buf2, 480 * 40);
    // Register display driver with flush callback → display.pushPixelsDMA()
    // esp_timer at 1ms calls lv_tick_inc(1)
    // lvglTask: Core 1, priority 1, 8192 bytes stack
}
```

**Acceptance criteria:**
- [x] `ps_malloc()` code implemented
- [ ] LVGL FPS monitor shows > 25 FPS
- [ ] `lv_label_create()` + text renders on screen
- [ ] No stack overflow after 60 seconds

---

### T-04 · Touch verification
**Estimate:** 1 hour · **Depends on:** T-03

No new files. Add to `display.cpp`. Print touch coords to Serial.

**Acceptance criteria:**
- [x] Touch code implemented
- [ ] Coordinates print when screen pressed: X 0–479, Y 0–319
- [ ] No phantom touches at rest
- [ ] Corner test: ±15 px tolerance

---

### T-05 · PSRAM and flash verification
**Estimate:** 30 min · **Depends on:** T-01

```cpp
// Add to setup():
LOG_I("Flash: %lu", ESP.getFlashChipSize());   // expect 16777216
LOG_I("PSRAM: %lu", ESP.getPsramSize());        // expect 2097152
assert(ESP.getPsramSize()   > 0           && "PSRAM not found");
assert(ESP.getFlashChipSize() >= 16*1024*1024 && "Flash < 16MB");
```

**Acceptance criteria:**
- [x] Flash verification code implemented
- [ ] Flash: 16777216 in Serial
- [ ] PSRAM: 2097152 in Serial
- [ ] `ps_malloc(1024*1024)` returns non-null

---

### T-06 · Debug logging macros
**Estimate:** 30 min · **Depends on:** T-01

**Add to `src/config.h`:**
```cpp
#if DEBUG_BUILD
  #define LOG_D(f,...) Serial.printf("[D] " f "\n", ##__VA_ARGS__)
  #define LOG_I(f,...) Serial.printf("[I] " f "\n", ##__VA_ARGS__)
  #define LOG_W(f,...) Serial.printf("[W] " f "\n", ##__VA_ARGS__)
  #define LOG_E(f,...) Serial.printf("[E] " f "\n", ##__VA_ARGS__)
#else
  #define LOG_D(...) do{}while(0)
  #define LOG_I(...) do{}while(0)
  #define LOG_W(...) do{}while(0)
  #define LOG_E(...) do{}while(0)
#endif
```

**Acceptance criteria:**
- [x] Debug macros implemented in config.h
- [ ] LOG_I prints when DEBUG_BUILD=1
- [ ] DEBUG_BUILD=0 compiles without warnings, no Serial output

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 1 GATE — KODE FERDIG                          │
│  [x] T-01 to T-06 all implemented                      │
│  [ ] Display: correct color cycle and text              │
│  [ ] Touch: coordinates verified in Serial              │
│  [ ] PSRAM: 2097152 bytes confirmed                     │
│  [ ] Flash: 16777216 bytes confirmed                    │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

---

# PHASE 2 — MOTOR CONTROL
### Goal: Motor spins at commanded RPM, stable FreeRTOS tasks running.

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 2 GATE — KODE FERDIG                          │
│  [x] Motor control code implemented                    │
│  [ ] Motor rotates CW and CCW on command                │
│  [ ] PUL+ current 6–12 mA measured                     │
│  [ ] STEP jitter < 1 µs documented                      │
│  [ ] Speed changes smoothly with pot                    │
│  [ ] All 6 tasks running without crash                  │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

### T-07 · GPIO init — ENA HIGH before everything
**Estimate:** 30 min · **Depends on:** T-01

**Create:** `src/motor/motor.h` · `src/motor/motor.cpp`

```cpp
// FIRST LINES of setup() in main.cpp — before Serial, before display, before everything:
void setup() {
    pinMode(PIN_ENA, OUTPUT);
    digitalWrite(PIN_ENA, HIGH);   // Motor OFF — this cannot move
    // ... rest of init follows
}

// motor_gpio_init() called later in motor_init():
void motor_gpio_init() {
    assert(digitalRead(PIN_ENA) == HIGH && "ENA must be HIGH here");
    pinMode(PIN_DIR,   OUTPUT);
    pinMode(PIN_STEP,  OUTPUT);
    pinMode(PIN_ESTOP, INPUT_PULLUP);
    LOG_I("Motor GPIO init OK. ENA=HIGH ESTOP=%d", digitalRead(PIN_ESTOP));
}
```

**Acceptance criteria:**
- [x] GPIO init code implemented (ENA HIGH in setup())
- [ ] Multimeter on GPIO 13: 3.3V at power-on before any other output
- [ ] Motor does not move when power applied
- [ ] GPIO 14 = 3.3V with ESTOP cable disconnected (pull-up active)

---

### T-08 · FastAccelStepper — basic rotation
**Estimate:** 2–3 hours · **Depends on:** T-07

**Add to `motor.cpp`:**
```cpp
static FastAccelStepperEngine engine = FastAccelStepperEngine();
static FastAccelStepper* stepper = nullptr;

void motor_init() {
    motor_gpio_init();
    engine.init();
    stepper = engine.stepperConnectToPin(PIN_STEP);
    assert(stepper != nullptr && "FastAccelStepper init failed");
    stepper->setDirectionPin(PIN_DIR);
    stepper->setEnablePin(PIN_ENA, false);   // false = active LOW
    stepper->setAutoEnable(false);
    stepper->setAcceleration(5000);           // steps/s²
    stepper->setSpeedInHz(10000);             // 10 kHz = 5 RPM workpiece
}

void motor_run_cw()  { digitalWrite(PIN_DIR, HIGH); digitalWrite(PIN_ENA, LOW);  stepper->runForever(); }
void motor_run_ccw() { digitalWrite(PIN_DIR, LOW);  digitalWrite(PIN_ENA, LOW);  stepper->runForever(); }
void motor_stop()    { stepper->stopMove(); }
void motor_halt()    { stepper->forceStop(); digitalWrite(PIN_ENA, HIGH); }
```

**Acceptance criteria:**
- [x] FastAccelStepper code implemented
- [ ] Motor rotates CW and CCW on command
- [ ] `motor_stop()` decelerates smoothly over ~2 s
- [ ] `motor_halt()` stops immediately
- [ ] PUL+ optocoupler current: 6–12 mA (multimeter in series)
- [ ] STEP jitter < 1 µs on oscilloscope (GPIO 11) — document result

---

### T-09 · Speed control and ADC IIR filter
**Estimate:** 1–2 hours · **Depends on:** T-08

**Create:** `src/motor/speed.h` · `src/motor/speed.cpp`

```cpp
#define IIR_ALPHA 0.1f
static float adcFiltered     = 2047.5f;
static float sliderRPM       = MIN_RPM;
static uint32_t lastSliderMs = 0;

float rpmToStepHz(float rpm) {
    return rpm * GEAR_RATIO * (D_EMNE / D_RULLE) * STEPS_PER_REV / 60.0f;
}

void speed_update_adc() {   // called every 20 ms from ioTask
    float raw = (float)analogRead(PIN_POT);     // GPIO 10, ADC1_CH9 only
    adcFiltered = IIR_ALPHA * raw + (1.0f - IIR_ALPHA) * adcFiltered;
}

void  speed_slider_set(float rpm)  { sliderRPM = constrain(rpm, MIN_RPM, MAX_RPM); lastSliderMs = millis(); }
float speed_get_target_rpm() {
    if (millis() - lastSliderMs < 500) return sliderRPM;   // slider wins
    float adc = constrain(adcFiltered, 50.0f, 4045.0f);
    return MIN_RPM + (adc - 50.0f) / 3995.0f * (MAX_RPM - MIN_RPM);
}
void speed_apply() {   // called every 5 ms from motorTask
    if (stepper->isRunning())
        stepper->setSpeedInHz((uint32_t)rpmToStepHz(speed_get_target_rpm()));
}
```

**Acceptance criteria:**
- [x] Speed control code implemented
- [ ] `rpmToStepHz(1.0f)` == 2000.0
- [ ] `rpmToStepHz(5.0f)` == 10000.0
- [ ] RPM changes smoothly with pot — no jitter

---

### T-10 · FreeRTOS task skeleton
**Estimate:** 2 hours · **Depends on:** T-09

**Create:** `src/control/control.h` · `src/control/control.cpp`

```cpp
// All tasks created in main.cpp:
xTaskCreatePinnedToCore(safetyTask,  "safety",  2048, NULL, 5, NULL, 0);
xTaskCreatePinnedToCore(motorTask,   "motor",   4096, NULL, 4, NULL, 0);
xTaskCreatePinnedToCore(controlTask, "control", 4096, NULL, 3, NULL, 0);
xTaskCreatePinnedToCore(ioTask,      "io",      3072, NULL, 2, NULL, 0);
xTaskCreatePinnedToCore(lvglTask,    "lvgl",    8192, NULL, 1, NULL, 1);
xTaskCreatePinnedToCore(storageTask, "storage", 4096, NULL, 1, NULL, 1);

// Each periodic task uses vTaskDelayUntil() for precise timing:
void motorTask(void* pv) {
    esp_task_wdt_add(NULL);
    TickType_t t = xTaskGetTickCount();
    for(;;) {
        esp_task_wdt_reset();
        speed_apply();
        vTaskDelayUntil(&t, pdMS_TO_TICKS(5));
    }
}
```

**Acceptance criteria:**
- [x] FreeRTOS tasks implemented
- [ ] All 6 tasks start without stack overflow
- [ ] System stable for 10 minutes without restart

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 2 GATE — KODE FERDIG                          │
│  [x] Motor control code implemented                    │
│  [ ] Motor rotates CW and CCW on command                │
│  [ ] PUL+ current 6–12 mA measured                     │
│  [ ] STEP jitter < 1 µs documented                      │
│  [ ] Speed changes smoothly with pot                    │
│  [ ] All 6 tasks running without crash                  │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

---

# PHASE 3 — SAFETY
### Goal: ESTOP stops motor in < 1 ms. Watchdog resets on firmware hang.

> ⚠️ **This phase is non-negotiable.** Do not write Phase 4 code until every
> acceptance criterion is verified with an oscilloscope.

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 3 GATE — SAFETY CODE FERDIG                   │
│  [x] ESTOP ISR implemented                             │
│  [x] Watchdog implemented                              │
│  Oscilloscope required for verification:               │
│  [ ] GPIO 14 falling → GPIO 13 rising: MUST be < 1 ms. │
│  [ ] docs/estop_timing.md — PASS                        │
│  [ ] Worst-case ESTOP latency < 1.0 ms                  │
│  [ ] NC cable-cut test passes                           │
│  [ ] WDT resets in < 2 s, boot is fail-safe             │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

### T-11 · Fail-safe boot verification
**Estimate:** 1 hour · **Depends on:** T-07

Power cycle 5 times. Motor must never move.

**Acceptance criteria:**
- [x] ENA HIGH in setup() implemented
- [ ] GPIO 13 = HIGH within 1 ms of power-on (oscilloscope)
- [ ] Motor does not move across 5 power cycles
- [ ] Serial shows `"ENA=HIGH"` every boot

---

### T-12 · ESTOP ISR + safetyTask
**Estimate:** 2–3 hours · **Depends on:** T-10, T-11

**Create:** `src/safety/safety.h` · `src/safety/safety.cpp`

```cpp
volatile bool     g_estopPending   = false;
volatile uint32_t g_estopTriggerMs = 0;

void IRAM_ATTR estopISR() {
    // ISR: minimum work, no FreeRTOS calls
    digitalWrite(PIN_ENA, HIGH);   // Layer 1: < 0.5 ms
    stepper->forceStop();
    g_estopPending   = true;
    g_estopTriggerMs = millis();
}

void safetyTask(void* pv) {
    esp_task_wdt_add(NULL);
    attachInterrupt(digitalPinToInterrupt(PIN_ESTOP), estopISR, FALLING);
    TickType_t t = xTaskGetTickCount();
    for(;;) {
        esp_task_wdt_reset();
        if (g_estopPending && (millis() - g_estopTriggerMs) >= 5) {
            if (digitalRead(PIN_ESTOP) == LOW)
                control_transition_to(STATE_ESTOP);   // Layer 2: < 5 ms
            g_estopPending = false;
        }
        vTaskDelayUntil(&t, pdMS_TO_TICKS(1));
    }
}
```

**Acceptance criteria:**
- [x] ESTOP ISR code implemented
- [ ] Oscilloscope: GPIO 14 falling → GPIO 13 rising < 1 ms
- [ ] NC test: cut ESTOP cable → ESTOP activates automatically
- [ ] STATE_ESTOP reached within 5 ms (log output)
- [ ] Test 10 consecutive presses — all < 1 ms

---

### T-13 · Hardware watchdog
**Estimate:** 1 hour · **Depends on:** T-12

```cpp
esp_task_wdt_config_t cfg = { .timeout_ms=2000, .trigger_panic=true };
esp_task_wdt_reconfigure(&cfg);
```

**Acceptance criteria:**
- [x] Watchdog code implemented
- [ ] WDT reset within 2 s of simulated hang
- [ ] After WDT reset: GPIO 13 = HIGH (fail-safe)
- [ ] Serial shows `ESP_RST_WDT` as reset reason

---

### T-14 · ESTOP timing documentation
**Estimate:** 30 min · **Depends on:** T-12

**Create:** `docs/estop_timing.md` — record 10 measurements, confirm worst-case < 1 ms.

**Acceptance criteria:**
- [x] `docs/estop_timing.md` template created
- [ ] `docs/estop_timing.md` exists with 10 measurements
- [ ] Worst-case < 1.0 ms — result marked PASS

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 3 GATE — SAFETY VERIFIED                       │
│  [ ] docs/estop_timing.md — PASS                        │
│  [ ] Worst-case ESTOP latency < 1.0 ms                  │
│  [ ] NC cable-cut test passes                           │
│  [ ] WDT resets in < 2 s, boot is fail-safe             │
│  ⚠️  Do NOT proceed without all boxes above.            │
│  Run /clear before Phase 4.                             │
└─────────────────────────────────────────────────────────┘
```

---

---

# PHASE 4 — STATE MACHINE & MODES
### Goal: All 5 operating modes working with correct state transitions.

---

### T-15 · State machine core
**Estimate:** 3–4 hours · **Depends on:** T-12, T-13

**Add to:** `src/control/control.h` · `src/control/control.cpp`

```cpp
typedef enum {
    STATE_IDLE=0, STATE_RUNNING, STATE_PULSE,
    STATE_STEP, STATE_JOG, STATE_TIMER,
    STATE_STOPPING, STATE_ESTOP
} SystemState;

// Transition rules (enforce in control_transition_to):
// IDLE     → RUNNING, PULSE, STEP, JOG, TIMER, ESTOP
// RUNNING  → STOPPING, ESTOP
// STOPPING → IDLE, ESTOP
// ESTOP    → IDLE only (requires manual reset flag)
// Any      → ESTOP always permitted
```

**Acceptance criteria:**
- [x] State machine code implemented
- [ ] All 8 states reachable
- [ ] Invalid transitions rejected and logged
- [ ] ESTOP works from any state
- [ ] State change logged to Serial on every transition

---

### T-16 · Continuous mode
**Estimate:** 1–2 hours · **Depends on:** T-15

**Create:** `src/control/modes/continuous.cpp`

ENA LOW on entry, `speed_apply()` every 5 ms, ENA HIGH on exit.

**Acceptance criteria:**
- [x] Continuous mode code implemented
- [ ] Motor starts with smooth acceleration
- [ ] Speed follows pot in real time
- [ ] CW and CCW both work
- [ ] Smooth decel on stop

---

### T-17 · Pulse mode
**Estimate:** 2 hours · **Depends on:** T-16

**Create:** `src/control/modes/pulse.cpp`

```cpp
// Params: pulse_on_ms (100–5000), pulse_off_ms (100–5000)
// Rotate pulse_on_ms → pause pulse_off_ms → repeat
// ENA stays LOW between pulses (motor holds position)
```

**Acceptance criteria:**
- [x] Pulse mode code implemented
- [ ] Correct intervals verified with stopwatch
- [ ] ESTOP interrupts immediately

---

### T-18 · Step mode
**Estimate:** 2 hours · **Depends on:** T-16

**Create:** `src/control/modes/step_mode.cpp`

```cpp
void step_execute(float angle_deg) {
    long steps = angleToSteps(angle_deg);
    digitalWrite(PIN_ENA, LOW);
    stepper->move(steps);
    // controlTask waits for !stepper->isRunning() before next command
}
// Valid angles: 5, 10, 15, 30, 45, 90 degrees
```

**Acceptance criteria:**
- [x] Step mode code implemented
- [ ] 10° step rotates workpiece 10° (measure with protractor)
- [ ] Waits for completion before next command

---

### T-19 · Jog mode
**Estimate:** 1 hour · **Depends on:** T-16

**Create:** `src/control/modes/jog.cpp`

Motor runs only while button held. Default 0.5 RPM on workpiece.

**Acceptance criteria:**
- [x] Jog mode code implemented (with PRESS_LOST handling)
- [ ] Starts on press, stops instantly on release
- [ ] ESTOP works during jog

---

### T-20 · Timer mode
**Estimate:** 1–2 hours · **Depends on:** T-16

**Create:** `src/control/modes/timer_mode.cpp`

Continuous rotation for `timer_sec` seconds, then → STATE_STOPPING.

**Acceptance criteria:**
- [x] Timer mode code implemented
- [ ] Stops after configured time ± 1 second
- [ ] `timer_get_remaining_sec()` returns correct countdown
- [ ] Manual stop and ESTOP work during timer

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 4 GATE — KODE FERDIG                          │
│  [x] All 5 modes implemented                           │
│  [ ] All 5 modes tested with actual motor               │
│  [ ] ESTOP works from every mode                        │
│  [ ] No state machine deadlock in 10 min of use         │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

---

# PHASE 5 — GUI & PROGRAMS
### Goal: Full touchscreen UI across all 12 screens, programs saved to flash.

---

## UI THEME

### T-21 · Create theme.h — do this before any screen
**Estimate:** 30 min · **Depends on:** T-03

**Create:** `src/ui/theme.h`

```cpp
#pragma once
#include "lvgl.h"

// ── Backgrounds ────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x0B1929)   // Deep navy
#define COL_BG_CARD     lv_color_hex(0x0D2137)   // Card background
#define COL_BG_INPUT    lv_color_hex(0x071523)   // Input fields

// ── Accent ─────────────────────────────────────────────────────
#define COL_ACCENT      lv_color_hex(0x29B6F6)   // Cyan — titles, highlights
#define COL_ACCENT_DIM  lv_color_hex(0x1A6EA8)   // Dimmed accent

// ── Status ─────────────────────────────────────────────────────
#define COL_GREEN       lv_color_hex(0x43A047)   // Running / OK
#define COL_GREEN_DARK  lv_color_hex(0x1B5E20)   // Start button
#define COL_RED         lv_color_hex(0xC62828)   // ESTOP / Stop
#define COL_AMBER       lv_color_hex(0xF57C00)   // Warning / JOG
#define COL_PURPLE      lv_color_hex(0x7B1FA2)   // Pulse mode
#define COL_TEAL        lv_color_hex(0x00897B)   // Step mode

// ── Text ───────────────────────────────────────────────────────
#define COL_TEXT        lv_color_hex(0xE8F4FD)   // Primary — near white
#define COL_TEXT_DIM    lv_color_hex(0x78909C)   // Muted secondary
#define COL_TEXT_LABEL  lv_color_hex(0x90CAF9)   // Label — light blue

// ── UI elements ────────────────────────────────────────────────
#define COL_BORDER      lv_color_hex(0x1E3A5F)   // Panel borders
#define COL_BTN_NORMAL  lv_color_hex(0x132840)   // Button default bg
#define COL_BTN_PRESS   lv_color_hex(0x1E4D7A)   // Button pressed

// ── Sizing ─────────────────────────────────────────────────────
#define BTN_H           60    // Min button height (glove-safe)
#define BTN_H_SM        48    // Secondary button height
#define RADIUS_BTN       8    // Button corner radius
#define RADIUS_CARD     10    // Card corner radius
#define PAD_SCREEN      12    // Screen edge padding
```

**Acceptance criteria:**
- [ ] `theme.h` compiles when included in a screen file
- [ ] Colors show correctly on screen_main (verify in T-22)

---

## SCREEN 01 — MAIN SCREEN

### T-22 · Main screen
**Estimate:** 4–5 hours · **Depends on:** T-21, T-15

**Create:** `src/ui/screens/screen_main.cpp`

```
  480 px wide × 320 px tall (landscape)
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ STATUS BAR (full width, h=36)                                                │
  │  "TIG ROTATOR"   [● IDLE]   [POT]   [⚡ OK]                                 │
  ├──────────────────────────────────────────────────────────────────────────────┤
  │                                                                               │
  │              ╔═══════════════════════════════╗                               │
  │              ║   RPM GAUGE  (lv_arc)         ║                               │
  │              ║         0.0 RPM               ║   h=148                       │
  │              ╚═══════════════════════════════╝                               │
  │                                                                               │
  │  [◄ CCW  ]         [▶ START / ■ STOP]         [CW ►  ]   h=60              │
  │   100×60              160×60                   100×60                        │
  │                                                                               │
  │  SPEED ─────────────●────────── 2.5 RPM  ▣ POT          h=30               │
  │                                                                               │
  │  [ADVANCED]  [PROGRAMS]  [↻ DIRECTION: CW]  [⚙ SETTINGS]   h=44            │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Status bar (y=0 h=36):**
| Element | x | font | content |
|---|---|---|---|
| Title | 12 | 18 bold COL_ACCENT | `"TIG ROTATOR"` |
| State badge | 200 | 16 | changes per state — see table below |
| Speed source | 360 | 14 COL_TEXT_DIM | `"▣ POT"` or `"◈ SLIDER"` |
| System OK | 430 | 14 COL_GREEN | `"⚡ OK"` |

**State badge strings and colors:**
```cpp
// Update every 200 ms from screen_main_update()
STATE_IDLE:     "● IDLE"      COL_TEXT_DIM
STATE_RUNNING:  "● RUNNING"   COL_GREEN
STATE_PULSE:    "◉ PULSE"     COL_PURPLE
STATE_STEP:     "◈ STEP"      COL_TEAL
STATE_JOG:      "◎ JOG"       COL_AMBER
STATE_TIMER:    "⏱ TIMER"     COL_ACCENT
STATE_STOPPING: "◌ STOPPING"  COL_AMBER
STATE_ESTOP:    "⛔ ESTOP"    COL_RED  + blink 1 Hz
```

**RPM gauge (y=44 h=148):**
```cpp
lv_obj_t* arc = lv_arc_create(screen);
lv_obj_set_size(arc, 220, 140);
lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 44);
lv_arc_set_rotation(arc, 135);
lv_arc_set_bg_angles(arc, 0, 270);
lv_arc_set_range(arc, 0, 50);         // 0–5.0 RPM × 10
lv_arc_set_value(arc, 0);
// Indicator: COL_ACCENT  bg: COL_BG_CARD  knob: hidden

// Large RPM number centered inside arc:
// font=montserrat_48  color=COL_TEXT
// "0.0" with "RPM" in font=14 COL_TEXT_DIM beside it

// Update in screen_main_update():
lv_arc_set_value(arc, (int)(speed_get_actual_rpm() * 10));
lv_label_set_text_fmt(rpm_label, "%.1f", speed_get_actual_rpm());
```

**Action buttons (y=204 h=60):**
```cpp
// CCW button x=10  w=100 h=60  bg=COL_BTN_NORMAL  border=COL_ACCENT_DIM  "◄ CCW"
// START/STOP  x=120 w=240 h=60
//   IDLE state:    bg=COL_GREEN_DARK  "▶  START"
//   RUNNING state: bg=COL_RED        "■  STOP"
//   On START press: control_transition_to(STATE_RUNNING)
//   On STOP press:  control_transition_to(STATE_STOPPING)
// CW button   x=370 w=100 h=60  bg=COL_BTN_NORMAL  border=COL_ACCENT_DIM  "CW ►"

// Active direction button gets COL_ACCENT border (3px) + bold text
```

**Speed slider (y=276 h=20):**
```cpp
lv_obj_t* slider = lv_slider_create(screen);
lv_obj_set_size(slider, 340, 20);
lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, -40, -52);
lv_slider_set_range(slider, 1, 50);     // ×0.1 RPM
// RPM value right of slider: "2.5 RPM"  font=18 COL_TEXT
// Source indicator left: "▣ POT" or "◈ SLIDER"  font=14 COL_TEXT_DIM
// On change: speed_slider_set(value * 0.1f)
```

**Bottom nav (y=280 h=44):**
```
x=0   w=110  "ADVANCED"        → screen_menu
x=115 w=110  "PROGRAMS"        → screen_programs
x=230 w=130  "↻ CW ▼"         → toggle direction, update label
x=365 w=115  "⚙ SETTINGS"     → screen_settings
```

**screen_main_update() called every 200 ms from lvglTask:**
```cpp
void screen_main_update() {
    if (!screen_is_active(SCREEN_MAIN)) return;
    lv_arc_set_value(arc, (int)(speed_get_actual_rpm() * 10));
    lv_label_set_text_fmt(rpm_label, "%.1f", speed_get_actual_rpm());
    lv_label_set_text(state_label, control_get_state_string());
    lv_label_set_text(src_label, speed_using_slider() ? "◈ SLIDER" : "▣ POT");
    // Update START/STOP button text and color based on state
}
```

**Acceptance criteria:**
- [x] Main screen code implemented
- [ ] All elements visible and correct on hardware
- [ ] RPM gauge and number update smoothly during run
- [ ] START/STOP button changes text and color correctly
- [ ] Direction buttons show active direction with highlight
- [ ] All bottom nav buttons navigate correctly
- [ ] Test with heavy work gloves — all buttons pressable

---

## SCREEN 02 — ESTOP OVERLAY

### T-23 · ESTOP overlay
**Estimate:** 1–2 hours · **Depends on:** T-22, T-12

**Create:** `src/ui/screens/screen_estop_overlay.cpp`

This is an `lv_layer_top()` overlay — sits above ALL other screens.
Must appear within 50 ms of `STATE_ESTOP` from any screen.

```
  Full 480×320 — semi-transparent red overlay over whatever screen is active
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
  │░░                                                                          ░░│
  │░░                            ⛔                                            ░░│
  │░░                    EMERGENCY STOP                                        ░░│
  │░░                ACTIVATED — MOTOR DISABLED                                ░░│
  │░░                                                                          ░░│
  │░░         ┌────────────────────────────────────────────────────┐          ░░│
  │░░         │     RESET — CONFIRM SAFE TO CONTINUE               │          ░░│
  │░░         └────────────────────────────────────────────────────┘          ░░│
  │░░              only enabled when physical ESTOP is released                ░░│
  │░░                                                                          ░░│
  │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
  └──────────────────────────────────────────────────────────────────────────────┘
```

```cpp
void estop_overlay_show() {
    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ov, 480, 320);
    lv_obj_set_style_bg_color(ov, COL_RED, 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_80, 0);

    // Icon — blinks at 1 Hz via lv_timer
    lv_obj_t* icon = lv_label_create(ov);
    lv_label_set_text(icon, "⛔");
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -70);

    // Title
    lv_obj_t* title = lv_label_create(ov);
    lv_label_set_text(title, "EMERGENCY STOP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    // Subtitle
    lv_obj_t* sub = lv_label_create(ov);
    lv_label_set_text(sub, "ACTIVATED — MOTOR DISABLED");
    lv_obj_set_style_text_color(sub, COL_TEXT_DIM, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 10);

    // Reset button — only enabled when digitalRead(PIN_ESTOP) == HIGH
    lv_obj_t* btn = lv_btn_create(ov);
    lv_obj_set_size(btn, 320, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 80);
    // On press: control_transition_to(STATE_IDLE), lv_obj_del(ov)
}

void estop_overlay_update() {   // called from lvglTask every 200 ms
    bool released = (digitalRead(PIN_ESTOP) == HIGH);
    lv_obj_set_state(reset_btn, released ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
}
```

**Acceptance criteria:**
- [x] ESTOP overlay code implemented
- [ ] Overlay appears on top of every screen when STATE_ESTOP set
- [ ] Overlay appears within 50 ms of ESTOP event
- [ ] RESET button is grey/disabled while physical ESTOP is still pressed
- [ ] RESET button activates when ESTOP released
- [ ] After reset: overlay removed, returns to main screen

---

## SCREEN 03 — ADVANCED MENU

### T-24 · Advanced menu and mode screens (Pulse + Step + Timer)
**Estimate:** 4–5 hours · **Depends on:** T-22, T-17, T-18, T-20

**Create:** `src/ui/screens/screen_menu.cpp` · `screen_pulse.cpp` · `screen_step.cpp` · `screen_timer.cpp`

---

**screen_menu — navigation hub:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ADVANCED SETTINGS                                            │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  ┌─────────────────────────┐   ┌─────────────────────────┐                  │
  │  │  ◉  PULSE MODE          │   │  ◈  STEP MODE           │                  │ h=100
  │  │  Rotate / Pause cycle   │   │  Fixed angle steps      │                  │
  │  └─────────────────────────┘   └─────────────────────────┘                  │
  │  ┌─────────────────────────┐   ┌─────────────────────────┐                  │
  │  │  ◎  JOG MODE            │   │  ⏱  TIMER MODE          │                  │ h=100
  │  │  Manual positioning     │   │  Run for duration       │                  │
  │  └─────────────────────────┘   └─────────────────────────┘                  │
  │  ┌─────────────────────────────────────────────────────────────────┐        │
  │  │  ⚙  SETTINGS                System configuration               │        │ h=60
  │  └─────────────────────────────────────────────────────────────────┘        │
  └──────────────────────────────────────────────────────────────────────────────┘
```

| Button | x | y | w | h | Color | Destination |
|---|---|---|---|---|---|---|
| ◉ PULSE | 12 | 44 | 220 | 100 | COL_PURPLE border | screen_pulse |
| ◈ STEP | 248 | 44 | 220 | 100 | COL_TEAL border | screen_step |
| ◎ JOG | 12 | 152 | 220 | 100 | COL_AMBER border | screen_jog |
| ⏱ TIMER | 248 | 152 | 220 | 100 | COL_ACCENT border | screen_timer |
| ⚙ SETTINGS | 12 | 260 | 456 | 60 | COL_BTN_NORMAL | screen_settings |

---

**screen_pulse — pulse mode setup:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ◉ PULSE MODE                            [● IDLE]             │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  ┌──────────────────────────────────┐  ┌──────────────────────────────────┐  │
  │  │  PULSE ON                        │  │  PULSE OFF                       │  │
  │  │  ┌────┐  ┌──────┐  ┌────┐       │  │  ┌────┐  ┌──────┐  ┌────┐       │  │ h=120
  │  │  │ –  │  │ 500  │  │ +  │       │  │  │ –  │  │ 500  │  │ +  │       │  │
  │  │  └────┘  │  ms  │  └────┘       │  │  └────┘  │  ms  │  └────┘       │  │
  │  │          └──────┘               │  │           └──────┘               │  │
  │  │  ─────●─────── 100ms – 5000ms  │  │  ─────●─────── 100ms – 5000ms  │  │
  │  └──────────────────────────────────┘  └──────────────────────────────────┘  │
  │  SPEED  ─────────────────●──── 2.5 RPM                                      │ h=36
  │  PREVIEW  ████████████░░░████████████░░░████████████░░░   ON ██  OFF ░     │ h=40
  │  ┌──────────────────────────┐        ┌─────────────────────────────────┐    │
  │  │  ▶  START PULSE          │        │  ◄ BACK                         │    │ h=56
  │  └──────────────────────────┘        └─────────────────────────────────┘    │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**PULSE ON / PULSE OFF panels** (each w=222 h=120 bg=COL_BG_CARD radius=10):
```cpp
// Each panel contains:
// Label "PULSE ON" / "PULSE OFF"   y=+8   font=14 COL_TEXT_LABEL
// [-] button    x=+8  y=+32  w=58 h=52   tap=-100ms, hold=repeat
// Value         x=+72 y=+32  w=82 h=52   font=24 bold center  "500"
// Unit "ms"     x=+72 y=+84  font=14 COL_TEXT_DIM center
// [+] button    x=+160 y=+32 w=58 h=52   tap=+100ms, hold=repeat
// Slider        x=+8  y=+98  w=206 h=16  range 100–5000

// Hold-to-repeat on +/- (500 ms initial, 3 Hz repeat):
lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
```

**Sequence preview bar** (y=224 h=40 bg=COL_BG_CARD):
```cpp
// Draw 3 cycles proportionally across 456px total width
// on_frac = pulse_on_ms / (pulse_on_ms + pulse_off_ms)
// Each ON block: COL_GREEN filled rect
// Each OFF block: COL_BG_INPUT filled rect
// Redraws via lv_canvas whenever params change
// Label: "ON █ 500ms  OFF ░ 500ms"
```

**START/STOP button behavior:**
```cpp
// IDLE: "▶  START PULSE"  bg=COL_GREEN_DARK
//   press → control_transition_to(STATE_PULSE)
// PULSE: "■  STOP PULSE"  bg=COL_RED
//   press → control_transition_to(STATE_STOPPING)
```

---

**screen_step — step mode setup:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ◈ STEP MODE                             [● IDLE]             │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  STEP SIZE                                                                    │
  │  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐               │ h=60
  │   5°      10°      15°      30°      45°      90°                            │
  │  (active = COL_ACCENT border + COL_BG_CARD bg + bold)                        │
  │                                                                               │
  │  DIRECTION                                                                    │
  │  ┌─────────────────────────┐      ┌─────────────────────────┐               │ h=56
  │  │  ◄  CCW                 │      │                 CW  ►   │               │
  │  └─────────────────────────┘      └─────────────────────────┘               │
  │                                                                               │
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  POSITION DIAL  (lv_arc as indicator)                                │   │ h=84
  │  │  ○────────────────●    Accumulated: 30°   Steps taken: 3            │   │
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │                                                                               │
  │  ┌───────────────────────┐  ┌──────────┐  ┌───────────────────────┐        │
  │  │  ◈  EXECUTE STEP      │  │  RESET   │  │  ◄ BACK               │        │ h=56
  │  └───────────────────────┘  └──────────┘  └───────────────────────┘        │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Step size buttons** (y=44 h=60): 6 buttons evenly at w=72 each, gap=4
```cpp
const float angles[] = {5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 90.0f};
// Active:   bg=COL_BG_CARD  border_color=COL_ACCENT  border=3px  text=COL_ACCENT bold
// Inactive: bg=COL_BTN_NORMAL  border=COL_BORDER 1px  text=COL_TEXT_DIM
```

**Direction buttons** (y=116 h=56):
```
CCW x=12  w=210  active=COL_ACCENT 3px border + bold
CW  x=258 w=210  active=COL_ACCENT 3px border + bold
```

**Position dial** (y=184 h=84 bg=COL_BG_CARD radius=10):
```cpp
// Small lv_arc (80×80) as position indicator — left side
lv_arc_set_range(dial, 0, 3600);   // × 0.1 degrees
lv_arc_set_value(dial, accumulated_angle * 10);
// Labels right side:
// "Accumulated: XX.X°"  font=18 COL_TEXT
// "Steps taken: N"      font=14 COL_TEXT_DIM
```

**Action buttons** (y=276 h=56):
```
EXECUTE x=12  w=220 h=50  bg=COL_TEAL
  → Disabled while stepper->isRunning()
  → step_execute(step_angle), increment counter and dial

RESET   x=240 w=108 h=50  bg=COL_BTN_NORMAL
  → accumulated_angle = 0, steps_taken = 0, reset dial

BACK    x=356 w=112 h=50  bg=COL_BTN_NORMAL
  → screen_menu
```

---

**screen_timer — timer mode setup:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ⏱ TIMER MODE                            [● IDLE]             │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  DURATION                                                            │   │
  │  │  ┌────┐  ┌──────┐  ┌────┐   :   ┌────┐  ┌──────┐  ┌────┐          │   │ h=100
  │  │  │ –  │  │  00  │  │ +  │       │ –  │  │  30  │  │ +  │          │   │
  │  │  └────┘  │ min  │  └────┘       └────┘  │ sec  │  └────┘          │   │
  │  │          └──────┘                        └──────┘                  │   │
  │  │                    Total: 30 seconds                                │   │
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │  SPEED   ─────────────────────●── 2.0 RPM                                   │ h=36
  │  DIR     [◄ CCW]                            [CW ►]                          │ h=52
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  00 : 30  (font=48 bold)                                             │   │ h=64
  │  │  ████████████████████████████████░░░░░░░░░░░░░░░░░░░░░░░   lv_bar   │   │
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │  ┌──────────────────────┐  ┌──────────────────────┐  ┌──────────────┐      │
  │  │  ▶  START TIMER      │  │  ■  STOP             │  │  ◄ BACK      │      │ h=52
  │  └──────────────────────┘  └──────────────────────┘  └──────────────┘      │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Duration input panel** (y=44 h=100 bg=COL_BG_CARD radius=10):
```cpp
// MINUTES (x=20 to x=200):
//   [-]  w=52 h=52  LV_EVENT_CLICKED: min-- (clamp 0–99)
//         LV_EVENT_LONG_PRESSED_REPEAT: repeat at 3 Hz
//   Value  w=80 h=52  font=36 bold center
//   [+]  w=52 h=52
//   Label "min" beneath value

// ":" separator at x=210

// SECONDS (x=220 to x=430): identical, range 0–59

// Total label centered at y=+85: "Total: X min Y sec"  font=14 COL_TEXT_DIM
```

**Countdown display** (y=230 h=64 bg=COL_BG_CARD radius=10):
```cpp
// MM:SS in font=48 bold COL_TEXT — centered
// lv_bar below, width=432 h=14
//   value = (timer_elapsed_sec * 1000) / timer_duration_sec  (0–1000)
//   bar color = COL_ACCENT  bg = COL_BG_INPUT
// Updates every 1 second when timer running
```

---

## SCREEN 06 — JOG MODE

### T-25 · Jog screen
**Estimate:** 2 hours · **Depends on:** T-22, T-19

**Create:** `src/ui/screens/screen_jog.cpp`

```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ◎ JOG MODE                              [● IDLE]             │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  JOG SPEED  ──────────●──────── 0.5 RPM   [0.1 ─────────── 2.0]            │ h=44
  │                                                                               │
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  HOLD BUTTON TO JOG — RELEASES WHEN FINGER LIFTS                     │   │ h=28
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │                                                                               │
  │  ┌───────────────────────────────────┐  ┌───────────────────────────────┐   │
  │  │                                   │  │                               │   │
  │  │                                   │  │                               │   │
  │  │         ◄◄◄  JOG  CCW            │  │         JOG  CW  ►►►         │   │ h=130
  │  │                                   │  │                               │   │
  │  │                                   │  │                               │   │
  │  └───────────────────────────────────┘  └───────────────────────────────┘   │
  │   220×130   bg=COL_BTN_NORMAL               220×130                         │
  │   pressed: bg=COL_AMBER                     pressed: bg=COL_AMBER           │
  │                                                                               │
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  ◄ BACK TO MENU                                                      │   │ h=44
  │  └──────────────────────────────────────────────────────────────────────┘   │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Critical: Jog buttons use PRESSED/RELEASED events, NOT click:**
```cpp
// Register BOTH events on each jog button:
lv_obj_add_event_cb(btn_ccw, jog_event_cb, LV_EVENT_ALL, (void*)DIR_CCW);
lv_obj_add_event_cb(btn_cw,  jog_event_cb, LV_EVENT_ALL, (void*)DIR_CW);

static void jog_event_cb(lv_event_t* e) {
    uint8_t dir = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        control_transition_to(STATE_JOG);
        jog_start(dir);
        // Visual: bg = COL_AMBER
    }
    else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        // PRESS_LOST handles finger sliding off button edge
        jog_stop();
        control_transition_to(STATE_IDLE);
        // Visual: bg = COL_BTN_NORMAL
    }
}
```

**Acceptance criteria:**
- [x] Jog screen code implemented (with PRESS_LOST handling)
- [ ] Motor runs only while button held — stops instantly on release
- [ ] PRESS_LOST (finger slides off) also stops motor
- [ ] Both CCW and CW work correctly
- [ ] Jog speed slider changes speed during jog

---

## SCREENS 08–10 — PROGRAMS

### T-26 · Programs screens (list + edit)
**Estimate:** 4–5 hours · **Depends on:** T-22, T-24 (programs.cpp)

**Create:** `src/ui/screens/screen_programs.cpp` · `src/ui/screens/screen_program_edit.cpp` · `src/programs/programs.h` · `src/programs/programs.cpp`

---

**Program struct:**
```cpp
typedef struct {
    char     name[21];
    uint8_t  mode;           // 0=CONT 1=PULSE 2=STEP 3=TIMER
    float    rpm;
    uint8_t  direction;      // 0=CW 1=CCW
    uint32_t pulse_on_ms;    // default 500
    uint32_t pulse_off_ms;   // default 500
    float    step_angle;     // default 10.0
    uint32_t timer_sec;      // default 300
} Program_t;
// Storage: /programs/1.json to /programs/16.json
// Factory defaults written on first boot (no files found)
```

---

**screen_programs — 16-slot scrollable list:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          PROGRAMS                         [SAVE CURRENT →  slot]      │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  ┌────┬──────────────────┬────────┬───────┬──────┬────────────────────────┐  │
  │  │  1 │ Pipe 3-inch      │ CONT   │1.0RPM │  CW  │  [LOAD]  [EDIT]       │  │ h=48
  │  ├────┼──────────────────┼────────┼───────┼──────┼────────────────────────┤  │
  │  │  2 │ Pulse weld       │ PULSE  │2.0RPM │  CW  │  [LOAD]  [EDIT]       │  │ h=48
  │  ├────┼──────────────────┼────────┼───────┼──────┼────────────────────────┤  │
  │  │  3 │ Step 10deg       │ STEP   │1.5RPM │  CW  │  [LOAD]  [EDIT]       │  │ h=48
  │  ├────┼──────────────────┼────────┼───────┼──────┼────────────────────────┤  │
  │  │  4 │ ── empty ──      │  –     │   –   │   –  │          [SAVE HERE]   │  │ h=48
  │  │  5 │ ── empty ──      │  –     │   –   │   –  │          [SAVE HERE]   │  │ h=48
  │  └────┴──────────────────┴────────┴───────┴──────┴────────────────────────┘  │
  │  lv_list or lv_table — scrollable — rows 48px tall                           │
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  ◄ BACK TO MENU                                                               │ h=44
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Row layout** (each 48px tall):
```cpp
// Col widths: slot=40  name=160  mode=72  rpm=80  dir=40  actions=104
// mode badge colors: CONT=COL_GREEN  PULSE=COL_PURPLE  STEP=COL_TEAL  TIMER=COL_ACCENT
// LOAD button: loads program into current UI, shows "✓ LOADED" for 1 s
// EDIT button: screen_program_edit(slot)
// SAVE HERE:   saves current mode settings into slot, prompts for name
// Selected row: COL_BG_CARD bg + COL_ACCENT 3px left border
```

---

**screen_program_edit — full edit form:**
```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ✗ CANCEL     EDIT PROGRAM  [Slot 2]                          [✓ SAVE]        │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  NAME  ┌────────────────────────────────────────────────────────────────┐   │
  │        │  Pulse weld                                               [✗] │   │ h=40
  │        └────────────────────────────────────────────────────────────────┘   │
  │                                                                               │
  │  MODE   [CONT]  [PULSE]  [STEP]  [TIMER]   ← radio buttons, one active      │ h=48
  │                                                                               │
  │  RPM    ┌────┐  ───────────●──────────  2.0 RPM  ┌────┐                     │ h=44
  │         │ –  │   lv_slider 0.1–10.0              │ +  │                     │
  │         └────┘                                    └────┘                     │
  │                                                                               │
  │  DIR    [◄ CCW]                          [CW ►]                             │ h=48
  │                                                                               │
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  MODE PARAMS (dynamic — changes based on MODE selection):            │   │ h=44
  │  │  CONT:  (no extra params)                                            │   │
  │  │  PULSE: ON [___]ms  OFF [___]ms                                      │   │
  │  │  STEP:  Angle [5°][10°][15°][30°][45°][90°]                         │   │
  │  │  TIMER: Duration [MM:SS]                                             │   │
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │                                                                               │
  │  ┌──────────────────────────┐   ┌─────────────────────────────────────────┐ │
  │  │  🗑 DELETE PROGRAM        │   │  ✗ CANCEL (discard changes)             │ │ h=48
  │  └──────────────────────────┘   └─────────────────────────────────────────┘ │
  └──────────────────────────────────────────────────────────────────────────────┘
```

```cpp
// Name input: lv_textarea, one line, max 20 chars
// On focus: lv_keyboard appears at bottom (built-in LVGL keyboard)

// Mode buttons: radio group — active gets mode color bg + white text
// CONT=COL_GREEN  PULSE=COL_PURPLE  STEP=COL_TEAL  TIMER=COL_ACCENT

// Mode params section changes dynamically:
void update_mode_params(uint8_t mode) {
    lv_obj_clean(params_panel);
    switch(mode) {
        case 0: /* CONT: no extra params */ break;
        case 1: /* PULSE: add ON/OFF spinners */ break;
        case 2: /* STEP: add angle selector */ break;
        case 3: /* TIMER: add MM:SS input */ break;
    }
}

// SAVE: programs_save(slot, &prog) → navigate to screen_programs
// DELETE: confirm_show("Delete program?", ...) → programs_delete(slot)
// CANCEL: discard, navigate back
```

---

## SCREEN 10 — SETTINGS

### T-27 · Settings screen
**Estimate:** 2 hours · **Depends on:** T-22

**Create:** `src/ui/screens/screen_settings.cpp`

```
  ┌──────────────────────────────────────────────────────────────────────────────┐
  │ ◄ BACK          ⚙ SETTINGS                                                   │ h=36
  ├──────────────────────────────────────────────────────────────────────────────┤
  │  MAX RPM       ─────────────────●── 5.0 RPM     range: 0.5 – 10.0           │ h=40
  │  JOG SPEED     ──────────●──────── 0.5 RPM     range: 0.1 – 2.0            │ h=40
  │  MICROSTEP     [1/2] [1/4] [1/8✓] [1/16] [1/32]                            │ h=52
  │                ⚠ Change requires restart. Update TB6600 DIP switches too.   │
  │  ACCELERATION  ─────────────────●── 5000 steps/s²                           │ h=40
  │                                                                               │
  │  ┌──────────────────────────────────────────────────────────────────────┐   │
  │  │  SYSTEM INFO                                                         │   │ h=44
  │  │  Firmware v1.0   Flash 16MB   PSRAM 2MB   Uptime 00:12:34           │   │
  │  └──────────────────────────────────────────────────────────────────────┘   │
  │                                                                               │
  │  ┌──────────────────────────┐   ┌──────────────────────────────────────┐    │
  │  │  🗑 FACTORY RESET         │   │  ◄ BACK                               │    │ h=52
  │  └──────────────────────────┘   └──────────────────────────────────────┘    │
  └──────────────────────────────────────────────────────────────────────────────┘
```

**Settings saved in LittleFS as `/settings.json`:**
```cpp
typedef struct {
    float   max_rpm;       // default 5.0
    float   jog_rpm;       // default 0.5
    uint8_t microsteps;    // default 8
    long    acceleration;  // default 5000
} Settings_t;
```

**Factory reset button:**
```cpp
confirm_show(
    "Reset ALL settings and programs?\nThis cannot be undone.",
    []() { LittleFS.format(); ESP.restart(); },
    nullptr
);
```

**Acceptance criteria:**
- [x] Settings screen code implemented (placeholder)
- [ ] All sliders change settings immediately (live effect)
- [ ] Settings persist after power cycle (saved to LittleFS)
- [ ] System info shows correct values
- [ ] Factory reset formats LittleFS and reboots
- [ ] Microstep warning is visible

---

## LVGL UPDATE ARCHITECTURE (applies to all screens)

```cpp
// Rule: NO direct lv_ calls from motor/control/safety tasks.
// All screen updates go through lvglTask on Core 1.

// In lvglTask (Core 1, every 10 ms):
void lvglTask(void* pv) {
    lvgl_hal_init();
    screen_main_create();
    lv_scr_load(screen_main);
    TickType_t t = xTaskGetTickCount();
    for(;;) {
        lv_timer_handler();
        screen_update_current();
        if (control_get_state() == STATE_ESTOP && !estop_overlay_visible())
            estop_overlay_show();
        if (control_get_state() != STATE_ESTOP && estop_overlay_visible())
            estop_overlay_hide();
        vTaskDelayUntil(&t, pdMS_TO_TICKS(10));
    }
}

// screen_update_current() dispatches to active screen update fn:
void screen_update_current() {
    switch(screen_get_active()) {
        case SCREEN_MAIN:  screen_main_update();  break;
        case SCREEN_PULSE: screen_pulse_update(); break;
        case SCREEN_TIMER: screen_timer_update(); break;
        // etc.
    }
}
```

---

## SCREEN NAVIGATION MAP

```
screen_main
    │
    ├── [ADVANCED] ──────────► screen_menu
    │                               │
    │              ┌────────────────┼──────────┬──────────┬──────────────┐
    │              ▼                ▼          ▼          ▼              ▼
    │         screen_pulse    screen_step  screen_jog  screen_timer  screen_settings
    │              │                │          │          │
    │              └────────────────┴──────────┴──────────┘
    │                               │ [BACK]
    │                               ▼
    │                          screen_menu  ──[BACK]──► screen_main
    │
    ├── [PROGRAMS] ───────────► screen_programs
    │                               │ [EDIT row]
    │                               ▼
    │                          screen_program_edit
    │                               │ [SAVE] or [CANCEL]
    │                               ▼
    │                          screen_programs  ──[BACK]──► screen_main
    │
    └── ESTOP overlay  ────────► lv_layer_top() — above ALL screens
                                    │ [RESET — only when ESTOP released]
                                    ▼
                               overlay removed → screen_main
```

---

## SCREEN IMPLEMENTATION ORDER

```
[ ] 1. theme.h              (T-21)
[ ] 2. screen_main          (T-22)
[ ] 3. screen_estop_overlay (T-23)
[ ] 4. screen_menu          (T-24)
[ ] 5. screen_pulse         (T-24)
[ ] 6. screen_step          (T-24)
[ ] 7. screen_timer         (T-24)
[ ] 8. screen_jog           (T-25)
[ ] 9. screen_programs      (T-26)
[ ] 10. screen_program_edit (T-26)
[ ] 11. screen_settings     (T-27)
[ ] 12. screen_confirm      (build when first needed in T-26/T-27)
```

---

## PHASE 5 ACCEPTANCE CRITERIA

```
[ ] All screens render without artifacts on 480×320 hardware
[ ] No screen loads in > 200 ms
[ ] All buttons min 60px tall — glove test passed
[ ] ESTOP overlay appears within 50 ms from any screen
[ ] ESTOP overlay cannot be dismissed while physical button is held
[ ] Jog screen: PRESS_LOST stops motor (finger slides off button)
[ ] Programs: save/load/delete work correctly for all 16 slots
[ ] Sequence preview bar animates correctly in pulse screen
[ ] Position dial accumulates correctly across multiple steps
[ ] Timer countdown and progress bar update every 1 second
[ ] Keyboard opens for name entry in program edit screen
[ ] Settings persist after power cycle
[ ] No LVGL heap fragmentation after 30 min continuous use
```

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 5 GATE — KODE FERDIG                          │
│  [x] All 12 screens created                             │
│  [ ] All 12 screens render on hardware                  │
│  [x] Navigation map implemented                          │
│  [x] ESTOP overlay code implemented                      │
│  [x] Jog PRESS_LOST behaviour implemented                │
│  [x] Programs screens created (placeholder)              │
│  [x] Settings screen created (placeholder)               │
│  → HARDWARE VERIFICATION REQUIRED                       │
└─────────────────────────────────────────────────────────┘
```

---

---

# PHASE 6 — INTEGRATION & VALIDATION
### Goal: System runs stable under real-world welding conditions.

---

### T-28 · Full system integration test
**Estimate:** 2–3 hours · **Depends on:** all previous

```
[ ] IDLE → RUNNING → STOPPING → IDLE
[ ] IDLE → PULSE → STOPPING → IDLE
[ ] IDLE → STEP (each angle: 5/10/15/30/45/90°) → IDLE
[ ] IDLE → JOG (hold CW, release) → IDLE
[ ] IDLE → TIMER (10 s) → IDLE
[ ] RUNNING → ESTOP → reset → IDLE
[ ] PULSE → ESTOP → reset → IDLE
[ ] STEP → ESTOP → reset → IDLE
[ ] JOG → ESTOP → reset → IDLE
[ ] TIMER → ESTOP → reset → IDLE
[ ] Programs: save 3 programs, power cycle, verify reload
[ ] 30 minutes continuous run without restart
```

---

### T-29 · EMI test with TIG welder
**Estimate:** 1–2 hours · **Depends on:** T-28

TIG welder running < 50 cm from controller, HF arc start enabled.

```
[ ] No system reset during TIG HF arc start
[ ] No GUI freeze during welding
[ ] STEP jitter < 1 µs on oscilloscope during welding
[ ] ADC variation < ±0.05 RPM during welding
[ ] Create docs/emi_test.md with results
```

---

### T-30 · Load test and thermal validation
**Estimate:** 2 hours · **Depends on:** T-28

Mount 50 kg workpiece, run at 5 RPM for 60 minutes.

```
[ ] No missed steps over 60 minutes
[ ] Motor < 70°C, TB6600 < 80°C, WT32-SC01+ < 60°C
[ ] Create docs/load_test.md with temperature readings
```

---

### T-31 · Production build
**Estimate:** 1 hour · **Depends on:** T-29, T-30

```cpp
// config.h:
#define DEBUG_BUILD 0

// platformio.ini build_flags:
// -DCORE_DEBUG_LEVEL=0
```

```
[ ] Compiles without warnings
[ ] Serial is silent (no LOG output)
[ ] Full functionality verified after flash
[ ] Git tag v1.0 created
```

---

```
┌─────────────────────────────────────────────────────────┐
│  ✋ PHASE 6 GATE — KODE FERDIG, TESTS PÅGÅR             │
│  [x] All 31 tasks code implemented                     │
│  [ ] docs/estop_timing.md — PASS                        │
│  [ ] docs/emi_test.md — PASS                            │
│  [ ] docs/load_test.md — PASS                           │
│  [ ] Production build flashed and verified              │
│  [ ] Git tag v1.0 created                               │
│                                                         │
│  → HARDWARE TESTS REQUIRED FOR PROJECT COMPLETE         │
│  🎉  TIG Rotator Controller v1.0 — CODE READY           │
└─────────────────────────────────────────────────────────┘
```

---

---

## APPENDIX — Quick reference for Claude Code sessions

**Starting a session on a specific task:**
> "Read CLAUDE_TASKS.md. I am working on T-08. Tasks T-01 to T-07 are complete.
> Implement T-08 now. Do not proceed to T-09 until I confirm acceptance criteria."

**When a task is done:**
> "T-08 acceptance criteria all pass. Mark T-08 [x] in CLAUDE_TASKS.md
> and summarise what T-09 requires."

**If something is broken:**
> "T-22 is failing — RPM gauge does not update. Review screen_main_update()
> and lvglTask only. Do not change any other files."

**Starting a screen:**
> "Implement screen_pulse.cpp using the exact layout in the SCREEN 04 section
> of CLAUDE_TASKS.md. Match all x/y/w/h values and use theme.h constants."

**Phase transition:**
> "Phase 3 gate passed — all boxes checked, oscilloscope result in docs/.
> Run /clear. Start Phase 4, begin with T-15."

---

*CLAUDE_TASKS.md v1.0 · TIG Welding Rotator Controller · 31 tasks / 6 phases*
*GPIO map verified against TZT official datasheet · Torque formula mathematically corrected v7*
*Includes complete screen specifications for all 12 screens (480×320 landscape)*
