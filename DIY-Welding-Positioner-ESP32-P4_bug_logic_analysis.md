# DIY Welding Positioner ESP32-P4 — Bugg- og forbedringsanalyse

**Prosjekt:** `catorendal-a11y/DIY-Welding-Positioner-ESP32-P4`  
**Branch analysert:** `master`  
**Dato:** 2026-04-25  
**Type analyse:** Statisk gjennomgang av kode, logikk, safety, UI, storage og dokumentasjon.  
**Merk:** Jeg har ikke kjørt `pio test` eller testet på faktisk ESP32-P4-hardware i denne runden. Funnene er basert på repo-gjennomgang.

---

## Kort konklusjon

Prosjektet er langt på vei godt strukturert: egen motor-modul, safety-modul, storage-modul, LVGL UI, atomics for mye cross-core state og en tydelig FreeRTOS-arkitektur. Det er likevel noen logikk-feil som kan gi rar oppførsel på ekte maskin:

1. **Digital fotpedal virker trolig ikke uten ADS1115**, selv om dokumentasjonen sier pedal switch på GPIO33.
2. **State machine kan gå til RUNNING/PULSE/JOG selv om motoren aldri faktisk startet.**
3. **Step mode starter `move()` før driver enable**, som kan miste første step-pulser.
4. **Driver alarm latch er ikke atomic**, selv om den leses/skrive på tvers av tasks.
5. **Preset-felter må ha tydelig runtime-semantikk**: `step_repeats`, `step_dwell_sec`, `cont_soft_start`, `timer_ms`, `timer_auto_stop`.
6. **Pulse-tider har forskjellige grenser i storage, UI og control.**
7. **Microstep UI mangler 1/4 selv om backend støtter det.**
8. **Display/LVGL init kan feile og likevel la systemet fortsette.**
9. **Direction i presets kan bli ignorert hvis fysisk DIR switch er aktiv.**
10. **Flere UI-verdier viser beregninger som ikke matcher faktisk motor-kinematikk.**

---

# 1. Kritiske bugs / høy prioritet

## 1.1 Digital fotpedal er låst bak ADS1115

**Filer:**

- `src/main.cpp`
- `src/motor/speed.cpp`
- `src/ui/screens/screen_main.cpp`
- `README.md`

**Problem:**

README beskriver:

- digital start switch på GPIO33
- analog speed via ADS1115 når aktivert

Men `motorTask()` sjekker pedal switch kun når `speed_pedal_connected()` er true. I `speed.cpp` betyr det:

```cpp
return ads1115Connected && pedalEnabled.load(...);
```

Resultat: **GPIO33-pedal fungerer ikke hvis ADS1115 ikke er montert**.

Dette er ikke bare en dokumentasjonsfeil. Det gjør en enkel on/off fotpedal ubrukelig uten analog pedal.

**Anbefalt design:**

Skill mellom:

- digital pedal switch enabled
- analog pedal ADC available

Forslag:

```cpp
bool speed_pedal_switch_enabled() {
  return pedalEnabled.load(std::memory_order_acquire);
}

bool speed_pedal_analog_available() {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected && pedalEnabled.load(std::memory_order_acquire);
#else
  return false;
#endif
}
```

I `motorTask()`:

```cpp
if (speed_pedal_switch_enabled()) {
  bool swPressed = (digitalRead(PIN_PEDAL_SW) == LOW);

  if (swPressed && !pedalSwWasPressed) {
    if (control_get_state() == STATE_IDLE) {
      control_start_continuous();
    }
  } else if (!swPressed && pedalSwWasPressed) {
    if (control_get_state() == STATE_RUNNING) {
      control_stop();
    }
  }

  pedalSwWasPressed = swPressed;
}
```

I `speed_apply()` bør analog hastighet bare velges hvis `speed_pedal_analog_available()` er true.

**Prioritet:** Kritisk hvis pedal skal brukes i praksis.

---

## 1.2 State kan si RUNNING selv om motoren ikke startet

**Filer:**

- `src/control/modes/continuous.cpp`
- `src/control/modes/pulse.cpp`
- `src/control/modes/jog.cpp`
- `src/motor/motor.cpp`

**Problem:**

`continuous_start()` gjør:

```cpp
motor_set_target_milli_hz(...);

if (dir == DIR_CW) {
  motor_run_cw();
} else {
  motor_run_ccw();
}

control_transition_to(STATE_RUNNING);
```

Men `motor_run_cw()` og `motor_run_ccw()` er `void`. De kan returnere tidlig ved:

- E-STOP
- driver alarm
- null stepper
- safety race etter mutex

Likevel går control-state til `STATE_RUNNING`.

**Konsekvens:**

UI og logikk tror maskinen kjører, mens ENA ikke ble satt LOW. Dette er farlig for feilsøking og kan gi låst/forvirrende brukeropplevelse.

**Fiks:**

Endre `motor_run_cw()` og `motor_run_ccw()` til å returnere `bool`.

```cpp
bool motor_run_cw() {
  if (safety_inhibit_motion()) return false;

  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);

  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }

  if (safety_inhibit_motion()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }

  digitalWrite(PIN_ENA, LOW);
  stepper->runForward();

  xSemaphoreGive(g_stepperMutex);
  return true;
}
```

Så i mode-koden:

```cpp
bool ok = (dir == DIR_CW) ? motor_run_cw() : motor_run_ccw();
if (!ok) {
  return;
}

control_transition_to(STATE_RUNNING);
```

Dette bør gjøres for:

- continuous
- pulse
- jog
- eventuelt step

**Enda bedre:** La `control_transition_to()` returnere `bool`, slik at mode-kode vet om state transition faktisk gikk gjennom.

---

## 1.3 Step mode kaller `move()` før ENA aktiveres

**Fil:** `src/control/modes/step_mode.cpp`

**Problem:**

I `step_execute()`:

```cpp
motor_apply_speed_for_rpm_locked(speed_get_target_rpm());
stepper->move(steps);
digitalWrite(PIN_ENA, LOW);
```

Stepper move sendes før driver enable. På noen drivere kan første pulser mistes hvis ENA ikke er aktiv før move begynner.

**Fiks:**

```cpp
motor_apply_speed_for_rpm_locked(speed_get_target_rpm());
digitalWrite(PIN_ENA, LOW);
stepper->move(steps);
```

Legg eventuelt inn kort ENA settle ved DM542T hvis nødvendig:

```cpp
digitalWrite(PIN_ENA, LOW);
delayMicroseconds(5);
stepper->move(steps);
```

Bruk bare delay hvis driveren faktisk trenger det.

---

## 1.4 Driver alarm latch er plain `bool`, men brukes cross-core

**Fil:** `src/safety/safety.cpp`

**Problem:**

```cpp
static bool s_driverAlarmLatched = false;
```

Den skrives i `safetyTask()` og leses fra andre tasks via:

```cpp
bool safety_is_driver_alarm_latched()
bool safety_inhibit_motion()
bool safety_can_reset_from_overlay()
```

Dette er cross-core state. En plain `bool` er ikke trygg nok her.

**Fiks:**

```cpp
static std::atomic<bool> s_driverAlarmLatched{false};
```

Bruk:

```cpp
s_driverAlarmLatched.store(true, std::memory_order_release);
s_driverAlarmLatched.load(std::memory_order_acquire);
```

**Ekstra forbedring:**

Navnet sier “latched”, men koden auto-clearer etter 50 ms HIGH:

```cpp
if (s_almHighMs >= 50) {
  s_driverAlarmLatched = false;
}
```

Enten bør den:

- hete `s_driverAlarmActive`
- eller være ekte latch som bare resettes av operatør

For welding/safety ville jeg valgt ekte latch.

---

## 1.5 E-STOP glitch kan stoppe motor uten å endre state

**Fil:** `src/safety/safety.cpp`

**Problem:**

ISR gjør direkte:

```cpp
GPIO.out1_w1ts.val = (1UL << (PIN_ENA - 32));
g_estopPending.store(true, ...);
```

Det betyr at ENA kuttes umiddelbart. Men hvis signalet er HIGH igjen før debounce på 5 ms, logger safety task “glitch” og endrer ikke state.

**Konsekvens:**

Motor driver er disabled, men `control_get_state()` kan fortsatt være `STATE_RUNNING`.

**Anbefaling:**

For maskinsikkerhet: enhver FALLING edge på E-STOP bør føre til en operatør-reset, selv hvis input senere ser HIGH ut. Alternativt må systemet minst gå til `STATE_STOPPING`/`STATE_IDLE` etter en forkastet glitch fordi ENA faktisk ble kuttet.

Fail-safe forslag:

```cpp
// Når g_estopPending er sett av ISR:
// - aldri bare ignorer glitch hvis ENA er kuttet
// - gå til STATE_ESTOP eller STATE_STOPPING
```

---

## 1.6 Display/LVGL init kan feile og systemet fortsetter

**Filer:**

- `src/ui/lvgl_hal.cpp`
- `src/ui/display.cpp`
- `src/main.cpp`

**Problem:**

`lvgl_alloc_buffers()` kan feile og returnere. `lvgl_hal_init()` returnerer da bare:

```cpp
if (buf1 == nullptr || buf2 == nullptr || rot_buf == nullptr) {
  LOG_E("LVGL buffer allocation failed - cannot continue");
  return;
}
```

Men `setup()` fortsetter og starter tasks. Da kan motor/control/safety være aktiv mens UI ikke er korrekt operativ.

I `display_init()` returneres det også ved flere init-feil, men setup fortsetter.

**Fiks:**

Bruk `fatal_halt()` ved kritisk UI/display-failure:

```cpp
if (buf1 == nullptr || buf2 == nullptr || rot_buf == nullptr) {
  fatal_halt("lvgl: buffer allocation");
}
```

Og i `display_init()` bør kritiske display-panel-feil enten returnere `bool` eller kalle `fatal_halt()`.

**Hvorfor:** På en fysisk maskin bør ikke et system uten fungerende UI/display gå videre til normal drift.

---

# 2. Preset- og programlogikk

## 2.1 `step_repeats` og `step_dwell_sec` lagres, men brukes ikke når preset kjøres

**Filer:**

- `src/storage/storage.h`
- `src/ui/screens/screen_edit_step.cpp`
- `src/ui/screens/screen_programs.cpp`
- `src/control/modes/step_mode.cpp`

**Problem:**

`Preset` har:

```cpp
uint16_t step_repeats;
float step_dwell_sec;
```

Edit Step-skjermen lar bruker sette repeats og dwell. Men når preset kjøres:

```cpp
control_start_step(p.step_angle);
```

Det ignorerer:

- `p.step_repeats`
- `p.step_dwell_sec`

**Konsekvens:**

UI lover mer enn runtime gjør.

**Fiks:**

Lag en step sequence request:

```cpp
void control_start_step_sequence(float angle_deg, uint16_t repeats, float dwell_sec);
```

Control task bør håndtere:

1. start step
2. vent til ferdig
3. dwell
4. neste repeat
5. ferdig -> idle

Eller lag en `ProgramExecutor` som kjører preset-logikk.

---

## 2.2 `cont_soft_start` lagres, men brukes ikke

**Filer:**

- `src/storage/storage.h`
- `src/ui/screens/screen_edit_cont.cpp`
- `src/control/modes/continuous.cpp`

**Problem:**

Continuous editor har soft start ON/OFF. Feltet lagres i preset:

```cpp
p->cont_soft_start = softStartEnabled ? 1 : 0;
```

Men `continuous_start()` bruker ikke dette feltet.

**Mulige løsninger:**

1. Fjern soft start fra UI til det faktisk er implementert.
2. Implementer soft start som lavere start-RPM og ramp opp.
3. Bruk eksisterende acceleration-system, men la preset velge egen acceleration profile.

---

## 2.3 Timer-feltene finnes i `Preset`, men timer er ikke en egentlig preset-mode

**Filer:**

- `src/storage/storage.h`
- `src/ui/screens/screen_program_edit.cpp`
- `src/ui/screens/screen_timer.cpp`

**Problem:**

`Preset` har:

```cpp
uint32_t timer_ms;
uint8_t timer_auto_stop;
```

Men Program Edit viser bare:

- CONT
- PULSE
- STEP

Timer er egen skjerm som bare gjør countdown -> continuous. Det er ingen `STATE_TIMER`, og presets kan ikke faktisk kjøre timer som program-mode.

**Anbefaling:**

Velg én av disse:

### A. Timer er bare UI-hjelper
Da bør `timer_ms` og `timer_auto_stop` fjernes fra `Preset` eller markeres deprecated.

### B. Timer skal være program-mode
Da trenger du:

```cpp
STATE_TIMER
control_start_timer(...)
Preset mode_mask for TIMER
Program editor toggle for TIMER
```

### C. Timer er “pre-start delay”
Da bør feltet hete noe som:

```cpp
uint8_t prestart_delay_sec;
```

og kunne brukes av alle modes.

---

## 2.4 Direction i presets kan bli ignorert når fysisk DIR switch er aktiv

**Filer:**

- `src/ui/screens/screen_programs.cpp`
- `src/motor/speed.cpp`
- `src/storage/storage.h`

**Problem:**

Preset loader gjør:

```cpp
speed_set_direction((Direction)p.direction);
```

Men `speed_get_direction()` gjør:

```cpp
if (g_dir_switch_cache.load(...)) {
  dir = digitalRead(PIN_DIR_SWITCH) ? DIR_CW : DIR_CCW;
} else {
  dir = (Direction)currentDir.load(...);
}
```

Hvis DIR switch er enabled, overstyrer fysisk switch direction fra preset.

**Konsekvens:**

Bruker lagrer program med CCW, men maskinen kan kjøre CW hvis fysisk bryter står CW.

**Fiks-alternativer:**

1. I UI: vis tydelig “Direction controlled by hardware switch”.
2. Når preset kjøres: hvis preset har direction, midlertidig ignorer hardware switch.
3. Lag setting: `preset_direction_policy = hardware | preset | ask`.

Min anbefaling: **hardware switch bør vinne for manuell drift, men presets bør ha tydelig policy.**

---

## 2.5 Program New-knapp ser disabled ut når full, men er fortsatt klikkbar

**Fil:** `src/ui/screens/screen_programs.cpp`

Når `MAX_PRESETS` er nådd, styles `newBtn` som disabled, men det fjernes ikke klikkbar flagg.

Fiks:

```cpp
if (isFull) {
  lv_obj_remove_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
} else {
  lv_obj_add_flag(newBtn, LV_OBJ_FLAG_CLICKABLE);
}
```

---

# 3. Pulse-logikk og grenser

## 3.1 Pulse max er inkonsistent

**Filer:**

- `src/control/modes/pulse.cpp`
- `src/ui/screens/screen_pulse.cpp`
- `src/ui/screens/screen_edit_pulse.cpp`
- `src/ui/screens/screen_main.cpp`
- `src/storage/storage.cpp`

Forskjellige steder bruker forskjellige grenser:

| Sted | Grense |
|---|---:|
| `pulse_start()` | 100–5000 ms |
| `screen_pulse.cpp` hint | 0.1–10.0 s |
| `screen_pulse.cpp` faktisk clamp | max 5000 ms |
| `screen_edit_pulse.cpp` | max 5000 ms |
| `screen_main_set_program_pulse_times()` | max 10000 ms |
| `storage_parse_presets_buffer()` | 10–60000 ms |

**Konsekvens:**

Preset kan lagre 60 sekunder, men runtime kjører maks 5 sekunder. UI kan antyde 10 sekunder.

**Fiks:**

Lag én felles konstant i `config.h`:

```cpp
#define PULSE_MS_MIN 100u
#define PULSE_MS_MAX 10000u
```

Bruk denne i:

- storage clamp
- editor UI
- pulse screen
- main quick pulse
- `pulse_start()`

---

## 3.2 Pulse cycle count starter litt ulogisk

**Fil:** `src/control/modes/pulse.cpp`

`pulseCycleCount` incrementes når ny ON-fase starter etter OFF:

```cpp
if (pulseIsOn) {
  pulseCycleCount++;
  if (pulseCycleLimit > 0 && pulseCycleCount >= pulseCycleLimit) {
    control_transition_to(STATE_STOPPING);
    return;
  }
}
```

Det betyr at første ON-fase ikke telles ved start. For `cycles=1` får du:

1. Første ON
2. OFF
3. Når neste ON skulle starte: count blir 1 og stopper før ny ON

Det kan være funksjonelt riktig, men navnet `pulseCycleCount` er forvirrende. Det teller fullførte sykluser, ikke aktive sykluser.

**Anbefaling:**

Gjør det eksplisitt:

```cpp
static uint16_t completedCycles = 0;
```

Og tell når OFF-fasen er ferdig, eller når ON+OFF er fullført. UI bør vise “completed / limit”.

---

# 4. Step mode og kinematikk

## 4.1 Edit Step viser feil “computed steps”

**Fil:** `src/ui/screens/screen_edit_step.cpp`

Edit Step beregner steps slik:

```cpp
long totalSteps = (long)(editAngle * editRepeats * ((float)microstep_get_steps_per_rev() * GEAR_RATIO / 360.0f));
```

Men faktisk runtime bruker:

```cpp
angleToSteps()
```

som inkluderer:

- `D_EMNE / D_RULLE`
- workpiece OD override
- calibration factor

**Konsekvens:**

UI kan vise et annet step-antall enn motor faktisk kjører.

**Fiks:**

Bruk samme funksjon som runtime:

```cpp
long totalSteps = labs(angleToSteps(editAngle)) * editRepeats;
```

Da matcher UI og motor.

---

## 4.2 `step_execute()` tillater 3600°, storage/preset begrenser til 360°

**Filer:**

- `src/control/modes/step_mode.cpp`
- `src/storage/storage.cpp`
- `src/ui/screens/screen_step.cpp`
- `src/ui/screens/screen_edit_step.cpp`

Runtime:

```cpp
if (angle_deg <= 0.0f || angle_deg > 3600.0f)
```

Storage clamp:

```cpp
p.step_angle = constrain(p.step_angle, 0.1f, 360.0f);
```

Program edit:

```cpp
if (editAngle > 360.0f) editAngle = 360.0f;
```

Step screen custom kan ha høyere verdi.

**Anbefaling:**

Lag felles:

```cpp
#define STEP_ANGLE_MIN_DEG 0.1f
#define STEP_ANGLE_MAX_DEG 3600.0f
#define STEP_PRESET_ANGLE_MAX_DEG 360.0f
```

Eller bestem at maks skal være 360 for alt.

---

## 4.3 Step sequence mangler

Det finnes UI for repeats/dwell, men control-laget kjører bare én step. Dette bør løses som egen sekvenslogikk.

Forslag til state:

```cpp
struct StepSequence {
  float angle_deg;
  uint16_t repeats_total;
  uint16_t repeats_done;
  uint32_t dwell_ms;
  uint32_t dwell_until_ms;
  bool dwell_active;
};
```

I `controlTask()`:

```cpp
case STATE_STEP:
  step_update();
  step_sequence_update();
  break;
```

---

# 5. UI-state og event-logikk

## 5.1 `screen_edit_cont_create()` initierer ikke direction/soft start fra preset før styling

**Fil:** `src/ui/screens/screen_edit_cont.cpp`

I create:

```cpp
editRpm = p ? p->rpm : 1.0f;
```

Men `directionCW` og `softStartEnabled` settes ikke fra preset før:

```cpp
restyle_direction();
restyle_soft_start();
```

De settes senere i `screen_edit_cont_update()`.

**Konsekvens:**

Skjermen kan først vise gamle/statiske toggles. Hvis bruker trykker SAVE raskt, kan feil direction/soft start lagres.

**Fiks:**

I `screen_edit_cont_create()`:

```cpp
directionCW = p ? (p->direction == 0) : true;
softStartEnabled = p ? (p->cont_soft_start != 0) : false;
```

før `restyle_direction()` og `restyle_soft_start()`.

---

## 5.2 `screen_edit_pulse_update()` syncer ikke cycles

**Fil:** `src/ui/screens/screen_edit_pulse.cpp`

Update syncer:

```cpp
editOnMs = p->pulse_on_ms;
editOffMs = p->pulse_off_ms;
editRpm = p->rpm;
```

Men ikke:

```cpp
editCycles = p->pulse_cycles;
```

Siden skjermen ofte rebuildes er dette kanskje ikke synlig ofte, men det er en logisk inkonsistens.

---

## 5.3 `screen_edit_step_update()` syncer ikke direction/repeats/dwell

**Fil:** `src/ui/screens/screen_edit_step.cpp`

Update syncer bare:

```cpp
editAngle = p->step_angle;
editRpm = p->rpm;
```

Ikke:

- `editDir`
- `editRepeats`
- `editDwell`

Samme anbefaling: hold update komplett eller fjern update hvis skjermen alltid rebuildes.

---

## 5.4 Timer delay lagres bare ved BACK

**Fil:** `src/ui/screens/screen_timer.cpp`

Når bruker endrer seconds og trykker START, `g_settings.countdown_seconds` lagres ikke. Den lagres bare i `backPending` path.

**Fiks:**

I `start_event_cb()` eller når countdownSec endres:

```cpp
xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
g_settings.countdown_seconds = (uint8_t)countdownSec;
xSemaphoreGive(g_settings_mutex);
storage_save_settings();
```

Eventuelt debounce.

---

## 5.5 Timer går til main selv om start ikke skjer

**Fil:** `src/ui/screens/screen_timer.cpp`

Når countdown når 0:

```cpp
control_start_continuous();
screens_request_show(SCREEN_MAIN);
```

Hvis `control_start_continuous()` ikke starter pga safety, E-STOP, driver alarm eller state race, går UI likevel til main.

Når `control_start_continuous()` returnerer bool eller request-resultat kan skjermen håndtere feil:

```cpp
if (!control_start_continuous()) {
  show_error("Cannot start");
  return;
}
```

---

# 6. Speed, ADC og cross-core data

## 6.1 `adcFiltered` leses fra UI task uten atomic

**Fil:** `src/motor/speed.cpp`

`adcFiltered` skrives i `speed_update_adc()` fra `motorTask`, men leses i `speed_slider_set()` som kan kalles fra LVGL/UI task:

```cpp
lastPotAdc.store(adcFiltered, ...);
```

Dette er en cross-core data race på `float`.

**Fiks:**

Alternativ A:

```cpp
static std::atomic<float> adcFilteredAtomic{2047.5f};
```

Alternativ B: bare la motorTask eie ADC og publiser snapshot:

```cpp
static std::atomic<uint16_t> adcFilteredCounts;
```

Min anbefaling: bruk integer ADC counts som atomic. Float atomics kan være lock-free på denne plattformen, men integer er tryggere og enklere.

---

## 6.2 Flere request-parametre er separate atomics

**Fil:** `src/control/control.cpp`

Control bruker:

```cpp
pendingModeRequest
pendingPulseOnMs
pendingPulseOffMs
pendingPulseCycles
pendingStepAngle
```

Dette fungerer ofte, men det er ikke en atomisk “command”. Hvis to producers skriver raskt etter hverandre, kan request og parametre teoretisk bli blandet.

**Bedre design: FreeRTOS queue**

```cpp
struct ControlCommand {
  ModeRequest mode;
  float rpm;
  Direction dir;
  uint32_t on_ms;
  uint32_t off_ms;
  uint16_t cycles;
  float angle_deg;
};
```

Bruk:

```cpp
xQueueSend(controlQueue, &cmd, 0);
```

Da blir kommando og parametre én enhet.

Dette er spesielt relevant fordi prosjektet har både:

- touch UI
- pedal
- safety
- future program executor

---

# 7. Motor config og runtime-apply

## 7.1 Microstep UI mangler 1/4

**Filer:**

- `src/motor/microstep.h`
- `src/motor/microstep.cpp`
- `src/ui/screens/screen_motor_config.cpp`

Backend støtter:

```cpp
MICROSTEP_4
MICROSTEP_8
MICROSTEP_16
MICROSTEP_32
```

Men UI viser bare:

```cpp
MICROSTEP_8, MICROSTEP_16, MICROSTEP_32
```

Hvis settings er 1/4, vil UI velge index 0 og ved save sette til 1/8.

**Fiks:**

```cpp
static const MicrostepSetting microOptions[4] = {
  MICROSTEP_4, MICROSTEP_8, MICROSTEP_16, MICROSTEP_32
};

static const char* microStrings[4] = {
  "800", "1600", "3200", "6400"
};
```

Bruk dynamisk array length i loops.

---

## 7.2 Motor Config kan endre timing mens motor går

**Filer:**

- `src/ui/screens/screen_motor_config.cpp`
- `src/motor/motor.cpp`

Motor Config viser advarsel:

```text
Stop motor before microstep change
```

Men `save_apply_cb()` stopper ikke bruker fra å lagre mens motor går.

`motor_apply_stepper_dir_timing()` kan forceStoppe motor hvis dir delay endres:

```cpp
if (s_dir_timing_applied && want != s_applied_dir_delay_us && stepper->isRunning()) {
  stepper->forceStop();
  digitalWrite(PIN_ENA, HIGH);
}
```

Men control-state oppdateres ikke tilsvarende.

**Fiks:**

I UI:

```cpp
if (control_get_state() != STATE_IDLE) {
  lv_label_set_text(saveFeedbackLabel, "Stop motor first");
  lv_obj_set_style_text_color(saveFeedbackLabel, COL_RED, 0);
  return;
}
```

Eller mer presist: bare blokker endringer som påvirker driver timing/microstep.

---

## 7.3 Default driver-kind er inkonsistent

**Fil:** `src/storage/storage.cpp`

Global default:

```cpp
SystemSettings g_settings = { ..., STEPPER_DRIVER_DM542T, ... };
```

Men ved eldre NVS uten felt:

```cpp
g_settings.stepper_driver = constrain(doc["stepper_driver"] | (int)STEPPER_DRIVER_STANDARD, 0, 1);
```

Dette gjør at gamle settings kan få standard timing selv om prosjektet ellers er DM542T-fokusert.

**Fiks:**

```cpp
g_settings.stepper_driver =
  constrain(doc["stepper_driver"] | (int)STEPPER_DRIVER_DM542T, 0, 1);
```

Eller bruk eksplisitt settings migration:

```cpp
if (settings_version < 2 && !doc["stepper_driver"].is<int>()) {
  g_settings.stepper_driver = STEPPER_DRIVER_DM542T;
}
```

---

# 8. Storage og persistens

## 8.1 `storage_get_usage()` er hardkodet

**Fil:** `src/storage/storage.cpp`

```cpp
*total = 0x6000;
*used = u + 1536;
```

Dette er bare estimat. Hvis partition table endres, blir dette feil.

**Fiks:**

Bruk ESP-IDF NVS stats der mulig:

```cpp
nvs_stats_t stats;
nvs_get_stats(NULL, &stats);
```

Rapporter:

- used_entries
- free_entries
- namespace_count

---

## 8.2 `storage_load_settings()` returnerer false, men setup ignorerer det

**Fil:** `src/storage/storage.cpp` / `src/main.cpp`

Hvis settings JSON er korrupt:

```cpp
return false;
```

Men `storage_init()` håndterer ikke dette som fatal eller fallback med save av defaults.

**Anbefaling:**

Ved parse-feil:

1. logg feilen
2. behold defaults
3. skriv defaults tilbake til NVS eller marker “needs format”
4. vis UI warning

For feltmaskin er fallback bedre enn boot loop, men brukeren bør vite at config ble reset.

---

# 9. Display, LVGL og ytelse

## 9.1 Full-frame double buffer + rotation buffer bruker mye PSRAM

**Fil:** `src/ui/lvgl_hal.cpp`

Allokerer:

- buf1: 768 KB
- buf2: 768 KB
- rot_buf: 768 KB

Totalt ca. 2.3 MB PSRAM bare for LVGL draw/rotation.

Det er greit med 32 MB PSRAM, men flush callback roterer pixel-for-pixel i CPU:

```cpp
for (int r = 0; r < H; r++) {
  for (int c = 0; c < W; c++) {
    rot_buf[(x2 - lx) * H + r] = src[r * W + c];
  }
}
```

**Forbedring:**

- Test partial render mode for mindre flush-områder.
- Vurder DMA2D eller optimized block rotation senere.
- Legg inn statistikk for flush time i debug builds.

Eksempel debug:

```cpp
uint32_t t0 = micros();
// rotate + draw
uint32_t dt = micros() - t0;
if (dt > 20000) LOG_W("flush slow: %lu us", dt);
```

---

## 9.2 Touch coordinate transform mangler clamp

**Fil:** `src/ui/lvgl_hal.cpp`

```cpp
data->point.x = 799 - py;
data->point.y = px;
```

Hvis GT911 noen gang returnerer støy utenfor range, kan LVGL få koordinat utenfor skjerm. Legg til clamp.

```cpp
int x = 799 - py;
int y = px;
if (x < 0) x = 0;
if (x >= DISPLAY_H_RES) x = DISPLAY_H_RES - 1;
if (y < 0) y = 0;
if (y >= DISPLAY_V_RES) y = DISPLAY_V_RES - 1;
```

---

# 10. Dokumentasjon og naming

## 10.1 Board-navn er inkonsistent

Det brukes flere varianter:

- `JC4880P443C`
- `JC4880P433`
- `JC4880P433C`
- `JC4880P443`

**Filer:**

- `README.md`
- `platformio.ini`
- `src/config.h`
- `src/ui/display.cpp`

**Fiks:**

Velg ett navn og bruk konsekvent. Hvis Guition har både P433/P443-varianter, lag et eget avsnitt:

```text
Tested board: GUITION JC4880P443C
Also seen as: JC4880P433C in some BSP examples
```

---

## 10.2 `setup()` logger hardkodet firmware v2.0

**Fil:** `src/main.cpp`

```cpp
LOG_I("TIG Rotator Controller v2.0");
```

Men `config.h` har:

```cpp
#define FW_VERSION "v2.0.5"
```

**Fiks:**

```cpp
LOG_I("TIG Rotator Controller %s", FW_VERSION);
```

---

## 10.3 Kommentarer sa fortsatt “12 screens” (fikset)

**Fil:** `src/ui/screens.cpp` / `src/ui/screens.h`

Kommentarene sa tidligere “all 12 screens”, mens enum nå har 21 aktive roots.

Dette er kosmetisk, men dokumentasjon i kode bør være riktig.

---

# 11. Testdekning som bør legges til

## Native tests

Legg til native tests for ren logikk:

### Speed / kinematics

- `rpmToStepHz()` ved default diameter
- `angleToSteps()` med diameter override
- calibration factor
- max/min RPM clamp
- NaN/negative input

### Pulse

- clamp min/max
- cycle count semantics
- finite cycles stopper etter riktig antall

### Program executor

- step repeats
- dwell timing
- preset direction policy
- timer/prestart behavior

### State machine

- invalid transition returnerer false
- RUNNING starter ikke hvis motor_run fails
- E-STOP edge fører til locked state
- pending stop rydder pending start

### Storage

- eldre JSON uten `stepper_driver`
- corrupted JSON fallback
- pulse time limits fra NVS

---

## Hardware tests

Utvid `test/test_device_gpio/test_main.cpp` med:

1. Pedal switch fungerer uten ADS1115.
2. ENA er HIGH ved boot.
3. Step mode setter ENA LOW før move.
4. E-STOP falling edge låser system selv ved kort glitch.
5. Direction switch policy dokumenteres/testes.
6. Driver ALM low gir ESTOP og krever reset.

---

## CI / GitHub Actions

Jeg fant ikke tydelig workflow for å kjøre PlatformIO native tests automatisk. Legg til:

```yaml
name: CI

on:
  push:
  pull_request:

jobs:
  native-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/cache@v4
        with:
          path: |
            ~/.platformio
            .pio
          key: pio-${{ runner.os }}-${{ hashFiles('platformio.ini') }}
      - uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      - run: pip install platformio
      - run: pio test -e native
```

Hvis ESP32-P4 build er for tung i CI, start med native tests.

---

# 12. Foreslått ny logikkarkitektur

## 12.1 Lag én `MotionCommand`

I stedet for mange separate atomics:

```cpp
struct MotionCommand {
  enum Type {
    StartContinuous,
    StartPulse,
    StartStep,
    StartJog,
    Stop,
    EstopReset
  } type;

  float rpm;
  Direction direction;

  uint32_t pulse_on_ms;
  uint32_t pulse_off_ms;
  uint16_t pulse_cycles;

  float step_angle_deg;
  uint16_t step_repeats;
  uint32_t step_dwell_ms;
};
```

Send med FreeRTOS queue:

```cpp
QueueHandle_t controlQueue;
```

Fordeler:

- ingen blanding av request/parametre
- lettere å teste
- pedal/UI/program kan sende samme kommandoformat
- mer robust for fremtidig multi-mode program executor

---

## 12.2 La state transition returnere bool

I dag:

```cpp
void control_transition_to(SystemState newState)
```

Bør bli:

```cpp
bool control_transition_to(SystemState newState)
```

Da kan mode-koden avbryte hvis state race skjer.

Eksempel:

```cpp
if (!control_transition_to(STATE_RUNNING)) {
  motor_halt();
  return false;
}
```

---

## 12.3 Skill SafetyReason fra SystemState

`STATE_ESTOP` brukes både for E-STOP og driver ALM. Lag:

```cpp
enum SafetyFaultReason {
  FAULT_NONE,
  FAULT_ESTOP,
  FAULT_DRIVER_ALM,
  FAULT_INIT_FAILURE
};
```

Da kan overlay vise:

- E-STOP PRESSED
- DRIVER ALARM
- INIT FAILURE

Og reset-policy kan være forskjellig.

---

## 12.4 Program executor

Lag en modul:

```text
src/control/program_executor.cpp
src/control/program_executor.h
```

Ansvar:

- kjøre preset
- respektere mode_mask
- kjøre valgt RUN-mode
- step repeats/dwell
- prestart delay
- soft start
- auto stop

Ikke legg alt i UI.

---

# 13. Prioritert backlog

## P0 — Fiks først

- [x] Gjør `s_driverAlarmLatched` atomic eller ekte latch.
- [x] Endre `motor_run_cw/ccw()` til bool og bare transition ved faktisk start.
- [x] Endre step mode til ENA LOW før `stepper->move()`.
- [x] Fail-safe E-STOP glitch: ikke bli i RUNNING etter ENA-kutt.
- [x] Fatal halt eller safe boot hvis display/LVGL init feiler.

## P1 — Viktige logikkfikser

- [x] Digital pedal switch må fungere uten ADS1115.
- [x] Implementer eller fjern `step_repeats`, `step_dwell_sec`, `cont_soft_start`.
- [x] Rydd timer-feltene i presets.
- [x] Én felles pulse max/min-konstant.
- [x] Direction preset policy når DIR switch er enabled.

## P2 — UI/UX og kvalitet

- [x] Microstep UI må inkludere 1/4 eller backend må fjerne 1/4.
- [x] Motor Config må blokkere timing/microstep save mens motor går.
- [x] New Program-knapp må faktisk disable når full.
- [x] Timer delay bør lagres ved start/justering.
- [x] Touch coordinate clamp.
- [x] Bruk `FW_VERSION` i boot log/about.

## P3 — Dokumentasjon/test

- [ ] Rydd board-navn.
- [x] Oppdater skjerm-antall kommentarer.
- [x] Legg til CI for native tests.
- [x] Utvid native tests for state/preset/pulse/step.
- [ ] Utvid hardware tests for pedal, E-STOP og ALM.

---

# 14. Foreslåtte konkrete patcher

## Patch A — motor run returnerer bool

**`src/motor/motor.h`**

```cpp
bool motor_run_cw();
bool motor_run_ccw();
```

**`src/motor/motor.cpp`**

```cpp
bool motor_run_cw() {
  if (safety_inhibit_motion()) return false;
  xSemaphoreTake(g_stepperMutex, portMAX_DELAY);
  if (stepper == nullptr) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  if (safety_inhibit_motion()) {
    xSemaphoreGive(g_stepperMutex);
    return false;
  }
  digitalWrite(PIN_ENA, LOW);
  stepper->runForward();
  xSemaphoreGive(g_stepperMutex);
  LOG_I("Motor: CW");
  return true;
}
```

Repeter for CCW.

---

## Patch B — continuous start validerer start

```cpp
void continuous_start() {
  if (control_get_state() != STATE_IDLE) return;
  if (safety_inhibit_motion()) return;

  motor_set_target_milli_hz(
    motor_milli_hz_for_rpm_calibrated(speed_get_target_rpm())
  );

  Direction dir = speed_get_direction();
  bool ok = (dir == DIR_CW) ? motor_run_cw() : motor_run_ccw();
  if (!ok) return;

  control_transition_to(STATE_RUNNING);
}
```

---

## Patch C — step ENA før move

```cpp
motor_apply_speed_for_rpm_locked(speed_get_target_rpm());
digitalWrite(PIN_ENA, LOW);
stepper->move(steps);
```

---

## Patch D — pedal switch uavhengig av ADS1115

**`speed.h`**

```cpp
bool speed_pedal_switch_enabled();
bool speed_pedal_analog_available();
```

**`speed.cpp`**

```cpp
bool speed_pedal_switch_enabled() {
  return pedalEnabled.load(std::memory_order_acquire);
}

bool speed_pedal_analog_available() {
#if ENABLE_ADS1115_PEDAL
  return ads1115Connected && pedalEnabled.load(std::memory_order_acquire);
#else
  return false;
#endif
}
```

---

## Patch E — atomic driver alarm

```cpp
static std::atomic<bool> s_driverAlarmLatched{false};

bool safety_is_driver_alarm_latched() {
  return s_driverAlarmLatched.load(std::memory_order_acquire);
}

bool safety_inhibit_motion() {
  return safety_is_estop_active() ||
         s_driverAlarmLatched.load(std::memory_order_acquire);
}
```

---

## Patch F — felles pulse-konstanter

**`config.h`**

```cpp
#define PULSE_MS_MIN 100u
#define PULSE_MS_MAX 10000u
```

Bruk overalt i stedet for hardkodet 5000/10000/60000.

---

## Patch G — microstep UI med 1/4

```cpp
static const MicrostepSetting microOptions[] = {
  MICROSTEP_4, MICROSTEP_8, MICROSTEP_16, MICROSTEP_32
};

static const char* microStrings[] = {
  "800", "1600", "3200", "6400"
};
```

---

# 15. Oppsummering

Dette prosjektet har et solid fundament, men de største problemene er ikke “syntax bugs”. De er **logikk-kontrakter**:

- UI sier én ting, runtime gjør noe annet.
- Storage har felter som ikke brukes.
- Safety kutter fysisk output, men state følger ikke alltid etter.
- Presets lagrer verdier som kan overstyres av hardware switch uten tydelig beskjed.
- Flere min/max-grenser er hardkodet forskjellig i ulike filer.

Hvis du fikser P0 + P1-listen, blir prosjektet mye mer robust og mer “ekte maskin”-klart.
