# DIY Welding Positioner ESP32-P4 — UI, Logic og Funksjonsforbedringer

**Prosjekt:** `DIY-Welding-Positioner-ESP32-P4`  
**Fokus:** UI-design, maskinlogikk, preset/program-flyt og nye funksjoner  
**Dato:** 2026-04-25

---

## Kort oppsummering

Prosjektet har allerede et bra teknisk fundament, men det kan løftes mye ved å gjøre tre ting:

1. **UI må se mer ut som et ekte industrielt maskinpanel**, ikke bare en touch-app.
2. **Runtime-logikken må matche det UI og presets lover.**
3. **Programmer/presets bør bli en ekte sekvensbygger**, ikke bare en snarvei til én mode.

Det største forbedringspotensialet ligger i koblingen mellom:

```text
UI -> Program/Preset -> ControlTask -> Motor/Safety
```

Akkurat nå finnes det flere verdier i presets som kan lagres, men som ikke brukes fullt ut når et program faktisk kjøres.

---

# 1. UI-forbedringer

## 1.1 Hovedskjerm bør bli et ekte maskinpanel

Hovedskjermen bør tydelig vise maskinens faktiske status, ikke bare target RPM og knapper.

### Nåværende problem

Bruker kan se `RUN`, `PULSE`, `STEP`, `JOG`, `STOP`, men det er ikke alltid tydelig:

- hva som styrer hastigheten
- om motor faktisk er enabled
- om pedal, pot eller slider bestemmer RPM
- om fysisk DIR switch overstyrer preset
- hvorfor maskinen ikke starter
- om fault er E-STOP eller driver alarm

### Forslag til ny hovedskjerm

```text
┌──────────────────────────────────────────────────────────────┐
│ TIG ROTATOR                      READY        SOURCE: POT    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                  0.420                                      │
│                   RPM                                       │
│                                                              │
│ TARGET 0.420     ACTUAL 0.418     DIR CW      ENA OFF       │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ [ JOG ]    [ CW/CCW ]    [ PULSE ]     [ STEP ]             │
│                                                              │
│ [ PEDAL: SW ]         [ PROGRAMS ]       [ SETTINGS ]       │
├──────────────────────────────────────────────────────────────┤
│ [ START ]                                      [ STOP ]      │
└──────────────────────────────────────────────────────────────┘
```

### Viktige visninger

Vis alltid:

- `Target RPM`
- `Actual RPM`
- `Direction`
- `Control source`
- `Motor enable state`
- `Safety state`
- `Pedal state`
- `Active mode`

Eksempel:

```text
SOURCE: POT
SOURCE: SLIDER
SOURCE: PEDAL ANALOG
SOURCE: PRESET
SOURCE: PROGRAM
```

---

## 1.2 Statuslinjen bør ha tydelige maskin-tilstander

Lag én stor statuslinje øverst.

### Forslag til states

```text
READY
RUNNING
PULSE ON
PULSE OFF
STEP MOVING
STEP DWELL
JOG CW
JOG CCW
STOPPING
E-STOP
DRIVER ALARM
CONFIG LOCKED
```

### Fargeforslag

| State | Farge |
|---|---|
| READY | grønn / dempet |
| RUNNING | grønn |
| PULSE ON | oransje |
| PULSE OFF | blå/grå |
| STEP MOVING | oransje |
| STEP DWELL | gul |
| STOPPING | gul |
| E-STOP | rød |
| DRIVER ALARM | rød/lilla |
| CONFIG LOCKED | gul |

---

## 1.3 STOP må alltid være visuelt dominerende

STOP bør være den mest synlige knappen etter E-STOP overlay.

Forslag:

```text
[ START ]                         [ █ STOP █ ]
```

STOP bør:

- alltid være aktiv
- alltid ligge nederst til høyre
- ha rød border
- ha større høyde enn sekundærknapper
- aldri skjules av modal/keyboard

---

## 1.4 START bør være disabled når maskinen ikke kan starte

START bør ikke bare gjøre ingenting. Den bør vise hvorfor den ikke kan starte.

Eksempler:

```text
START DISABLED — E-STOP ACTIVE
START DISABLED — DRIVER ALARM
START DISABLED — MOTOR CONFIG OPEN
START DISABLED — NO STEPPER
START DISABLED — INVALID RPM
```

Dette gjør feilsøking mye enklere.

---

## 1.5 Pedal-knappen bør vise nøyaktig pedalmodus

I dag er pedal-logikken litt uklar fordi digital switch og analog ADS1115 henger sammen.

### Ny pedalstatus

```text
PEDAL OFF
PEDAL SWITCH
PEDAL ANALOG
PEDAL ANALOG + SWITCH
NO ADS1115
PEDAL FAULT
```

### Pedal panel

```text
PEDAL
Switch:  OK / PRESSED / MISSING
Analog:  ADS1115 0x48 / MISSING
Mode:    Hold-to-run
Speed:   0.000 - 3.000 RPM
```

---

## 1.6 Safety overlay bør skille mellom feilårsaker

Ikke bruk samme overlaytekst for alt.

### E-STOP overlay

```text
E-STOP ACTIVE

Physical emergency stop is pressed.
Release E-stop and press RESET.

[ RESET ]
```

### Driver alarm overlay

```text
DRIVER ALARM

DM542T alarm input is active.
Check motor wiring, overload, driver power and ALM wiring.

[ RESET WHEN CLEAR ]
```

### Display/init fault

```text
SYSTEM INIT FAILURE

Display or motor init failed.
Motor output is disabled.

Restart required.
```

---

## 1.7 Programkort bør forklare hva RUN faktisk gjør

Programlista bør ikke bare vise “CONT / PULSE / STEP”. Den bør vise faktisk sekvens.

### Continuous program

```text
P01  Pipe root Ø60.3
RUN: CONT | 0.420 RPM | CW | Soft start ON
PRE: Countdown 3s
CTRL: Pedal hold
```

### Pulse program

```text
P02  Thin tube pulse
RUN: PULSE | 0.350 RPM | ON 0.5s / OFF 0.3s
CYCLES: Infinite
DIR: CW
```

### Step program

```text
P03  Flange tack
RUN: STEP | 90° x4 | Dwell 2.0s
DIR: CW
```

Dette gjør det mye tryggere å trykke RUN.

---

## 1.8 Program Editor bør bli en sekvensbygger

Nå er Program Editor nærmere en preset-editor. Den bør bli en “program builder”.

### Forslag

```text
PROGRAM: Pipe Root Pass

[1] Countdown     3 sec
[2] Direction     CW
[3] Mode          Continuous
[4] Speed         0.420 RPM
[5] Soft start    ON
[6] Stop mode     Manual / Pedal / Timer

[ TEST ]  [ SAVE ]
```

For step:

```text
PROGRAM: Flange Tack

[1] Direction     CW
[2] Step angle    90°
[3] Repeats       4
[4] Dwell         2.0 sec
[5] Return home   OFF

[ TEST ]  [ SAVE ]
```

For pulse:

```text
PROGRAM: Pulse Weld

[1] Direction     CW
[2] Speed         0.350 RPM
[3] ON time       0.5 sec
[4] OFF time      0.3 sec
[5] Cycles        Infinite

[ TEST ]  [ SAVE ]
```

---

## 1.9 Diagnostics screen

Dette bør være en egen skjerm.

### Innhold

```text
DIAGNOSTICS

INPUTS
E-STOP       HIGH / LOW
DIR SW       CW / CCW
PEDAL SW     HIGH / LOW
DRIVER ALM   OK / FAULT
POT ADC      2810
ADS1115      0x48 / MISSING

MOTOR
ENA          HIGH / LOW
STEP RATE    1234 Hz
TARGET RPM   0.420
ACTUAL RPM   0.418
STATE        RUNNING

SYSTEM
CORE 0       OK
CORE 1       OK
HEAP         OK
PSRAM        OK
NVS          OK
```

Dette er ekstremt nyttig når hardware skal bygges og feilsøkes.

---

## 1.10 Motor Config bør bli mer teknisk og trygg

Motor Config bør vise konsekvensene av valgene.

### Forslag

```text
MOTOR CONFIG

Driver:       DM542T
Pulse/rev:    3200
Gear ratio:   1:108
Max RPM:      3.000
Step rate:    17.280 kHz
Direction SW: Enabled
Invert DIR:   Off

Status: OK
```

### Safety rundt microstep

Når motor går:

```text
Microstep locked while motor is running.
Stop motor before changing pulse/rev.
```

Knappen bør være disabled, ikke bare ha advarsel.

---

## 1.11 Calibration Wizard

Lag en enkel wizard.

```text
CALIBRATION

1. Target rotation: 360°
2. Press RUN TEST
3. Measure actual rotation: ___ °
4. Save correction

Current factor: 1.000
New factor:     0.987
```

Formel:

```text
new_factor = old_factor * target_angle / measured_angle
```

Dette er mer brukervennlig enn manuell faktorjustering.

---

## 1.12 Dry Run / Simulation mode

Legg til en trygg testmodus.

```text
DRY RUN

Motor output disabled.
Sequence will be simulated only.

[ START SIMULATION ]
```

Den bør vise:

- hvilken mode som ville kjørt
- hvor lenge
- hvilken RPM
- hvilken direction
- repeats/dwell/cycles

Dette er veldig bra for testing av programmer uten fysisk bevegelse.

---

# 2. Logikkforbedringer

## 2.1 Lag en ekte Program Executor

### Nå

UI starter modes direkte:

```text
Program list -> control_start_continuous()
Program list -> control_start_pulse()
Program list -> control_start_step()
```

### Bedre

```text
Program list -> ProgramExecutor -> ControlTask -> Motor
```

Ny modul:

```text
src/control/program_executor.cpp
src/control/program_executor.h
```

### Ansvar

ProgramExecutor skal håndtere:

- countdown
- direction
- soft start
- continuous
- pulse cycles
- step repeats
- step dwell
- timer stop
- pedal behavior
- abort ved STOP
- abort ved E-STOP
- abort ved driver alarm

---

## 2.2 Presets bør ikke ha felt som ikke brukes

Preset har felter som:

```cpp
timer_ms
timer_auto_stop
step_repeats
step_dwell_sec
cont_soft_start
pulse_cycles
```

Disse bør enten:

1. brukes fullt ut i runtime
2. skjules/fjernes fra UI
3. markeres som “planned”

Min anbefaling: implementer dem via ProgramExecutor.

---

## 2.3 Bruk command queue for motor-kommandoer

Mange atomics er greit for flags, men motor-kommandoer bør være én samlet melding.

### Forslag

```cpp
enum class MotionType {
  StartContinuous,
  StartPulse,
  StartStep,
  StartJog,
  Stop,
  ResetFault
};

struct MotionCommand {
  MotionType type;
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
xQueueSend(controlQueue, &cmd, 0);
```

### Fordeler

- mode og parametre kommer samlet
- mindre race-condition
- lettere testing
- lettere å utvide
- pedal, UI og program executor kan bruke samme interface

---

## 2.4 State transition bør returnere `bool`

Nå bør `control_transition_to()` kunne si om overgangen faktisk skjedde.

```cpp
bool control_transition_to(SystemState next);
```

Da kan startfunksjoner gjøre:

```cpp
if (!control_transition_to(STATE_RUNNING)) {
  motor_halt();
  return false;
}
```

Det gjør control-laget tryggere.

---

## 2.5 Motor start bør returnere success/fail

`motor_run_cw()` og `motor_run_ccw()` bør returnere `bool`.

```cpp
bool motor_run_cw();
bool motor_run_ccw();
```

Da kan mode-logikken unngå å gå til RUNNING når motor ikke startet.

---

## 2.6 Skill mellom SystemState og FaultReason

`STATE_ESTOP` bør ikke alene forklare alle feil.

### Forslag

```cpp
enum class FaultReason {
  None,
  EstopPressed,
  DriverAlarm,
  MotorInitFailed,
  DisplayInitFailed,
  WatchdogReset,
  StorageCorrupt
};
```

Da kan UI vise riktig melding.

---

## 2.7 Direction policy for presets

Problem:

- preset kan lagre CW/CCW
- fysisk DIR switch kan overstyre direction

Lag en tydelig policy:

```cpp
enum class DirectionPolicy {
  HardwareSwitchWins,
  PresetWins,
  AskBeforeRun
};
```

### UI

```text
Direction source:
[ Hardware switch ]
[ Program preset ]
[ Ask when running program ]
```

Min anbefaling:

- manuell drift: hardware switch wins
- program drift: preset wins, men vis tydelig status

---

## 2.8 Pedal logic bør deles opp

Ikke bland digital switch og analog ADS1115.

### Del opp i tre nivåer

```cpp
bool pedal_switch_enabled();
bool pedal_switch_pressed();

bool pedal_analog_available();
float pedal_analog_rpm();

PedalMode pedal_mode();
```

### Pedal modes

```cpp
enum class PedalMode {
  Disabled,
  HoldToRun,
  ToggleRun,
  AnalogSpeedOnly,
  AnalogSpeedAndHold
};
```

---

## 2.9 Pulse-grenser må være globale

Lag én kilde til sannhet:

```cpp
#define PULSE_MS_MIN 100u
#define PULSE_MS_MAX 10000u
```

Bruk i:

- UI
- storage
- control
- presets
- docs

Ikke ha 5000 ett sted, 10000 et annet og 60000 i storage.

---

## 2.10 Step sequence må være egen state

Hvis step repeats/dwell skal fungere:

```cpp
enum class StepSequencePhase {
  Idle,
  Moving,
  Dwell,
  Complete
};
```

Data:

```cpp
struct StepSequence {
  float angle_deg;
  uint16_t repeats_total;
  uint16_t repeats_done;
  uint32_t dwell_ms;
  uint32_t dwell_start_ms;
  StepSequencePhase phase;
};
```

---

# 3. Funksjonsforbedringer

## 3.1 Job presets

Gå fra enkle presets til “jobs”.

### Job bør inneholde

```cpp
struct JobPreset {
  char name[32];
  float workpiece_od_mm;
  float rpm;
  Direction direction;
  PedalMode pedal_mode;
  uint8_t countdown_sec;
  ProgramMode mode;
  ProgramSettings settings;
};
```

Eksempler:

```text
Pipe Ø60.3 root pass
Pipe Ø88.9 cap pass
Flange 4 tack
Small tube pulse
Large ring slow rotate
```

---

## 3.2 Lagre workpiece diameter per program

Du har allerede diameter-logikk i step/speed. Den bør lagres per preset.

Legg til:

```cpp
float workpiece_od_mm;
```

i `Preset`.

Fordel:

- programkort kan vise diameter
- step/RPM blir riktig per jobb
- mindre manuell justering

---

## 3.3 Weld trigger output

Legg til valgfri output for sveiseapparat/rele.

### Funksjoner

```text
Torch trigger
Gas preflow
Arc delay
Postflow
```

Eksempel program:

```text
1. Gas preflow ON
2. Wait 0.5 sec
3. Torch trigger ON
4. Start rotation
5. Stop rotation
6. Torch trigger OFF
7. Postflow 2.0 sec
```

Dette bør være disabled som default.

---

## 3.4 Home/index sensor

Hvis du legger til sensor senere:

```text
HOME input
Index pulse
Zero position
Return to zero
```

Nye funksjoner:

- zero workpiece
- return to start
- step to next tack
- index mode

---

## 3.5 Tacho/encoder feedback

Hvis du legger til encoder:

- faktisk RPM
- missed step detection
- closed loop warning
- better calibration

UI kan vise:

```text
Target RPM: 0.420
Actual RPM: 0.417
Error: -0.7%
```

---

## 3.6 Logging / event history

Lag enkel eventlogg i RAM:

```text
12:01:03 START CONT 0.42 RPM CW
12:01:15 STOP
12:02:02 DRIVER ALARM
12:02:10 RESET
```

Dette hjelper mye ved feilsøking.

---

## 3.7 Export/import settings

Funksjon:

```text
Export programs to JSON
Import programs from JSON
Factory reset
Backup settings
```

Kan være via serial, web UI senere eller SD/USB hvis tilgjengelig.

---

## 3.8 Screensaver med live status

Når display dimmes, vis minimal status:

```text
RUNNING
0.420 RPM
CW
```

Ved fault må display våkne og vise full fault overlay.

---

# 4. UI-designretninger

## 4.1 Brutalist Industrial

Kjennetegn:

- svart/grå bakgrunn
- oransje accent
- store harde borders
- monospaced tall
- tydelig maskinpanel
- lite dekor, mye data

Passer prosjektet best.

---

## 4.2 CNC Controller Style

Kjennetegn:

- grid-layout
- DRO-lignende tall
- statusfelter
- mange små maskindata
- “RUN / HOLD / STOP”

Passer hvis du vil ha maskinverktøy-følelse.

---

## 4.3 Welding Console Style

Kjennetegn:

- mørk metall
- varme farger
- store knapper
- ampere/RPM/weld-program-inspirert layout
- tydelig job/preset-fokus

Passer hvis UI skal føles mer som sveiseutstyr.

---

## 4.4 Minimal Pro Touch HMI

Kjennetegn:

- renere layout
- færre knapper per skjerm
- store touch targets
- tydelige modaler
- mer moderne industri-HMI

Passer hvis du vil at det skal se mer kommersielt ut.

---

# 5. Prioritert plan

## Fase 1 — Gjør logikken riktig

- [x] Motor start returnerer success/fail
- [x] Control transition returnerer bool
- [x] FaultReason legges til
- [x] Pedal switch skilles fra ADS1115
- [x] Pulse-grenser samles i `config.h`
- [x] Microstep endring låses mens motor går

---

## Fase 2 — Gjør presets ekte

- [x] ProgramExecutor-modul
- [x] Step repeats
- [x] Step dwell
- [x] Pulse cycles
- [x] Soft start
- [ ] Countdown per program
- [x] Direction policy

---

## Fase 3 — Ny UI-struktur

- [ ] Ny main screen med status bar
- [ ] Ny Programs screen med tydelige programkort
- [x] Diagnostics screen
- [x] Pedal settings screen
- [ ] Calibration wizard
- [x] Fault overlay med reason

---

## Fase 4 — Ekstra funksjoner

- [x] Workpiece diameter per preset
- [ ] Dry run
- [ ] Weld trigger output
- [x] Event log
- [ ] Import/export
- [ ] Encoder/tacho support

---

# 6. Anbefalt første UI-redesign

Start med hovedskjermen.

## Ny main screen layout

```text
┌──────────────────────────────────────────────────────────────┐
│ TIG ROTATOR        READY                  SOURCE: POT        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                ┌────────────────────┐                       │
│                │       0.420        │                       │
│                │        RPM         │                       │
│                └────────────────────┘                       │
│                                                              │
│ TARGET 0.420   ACTUAL 0.418   DIR CW   ENA HIGH   ALM OK    │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ [ JOG ] [ DIR ] [ PULSE ] [ STEP ] [ PEDAL ] [ PROGRAMS ]   │
├──────────────────────────────────────────────────────────────┤
│ [ START CONTINUOUS ]                       [ STOP ]          │
└──────────────────────────────────────────────────────────────┘
```

## Hvorfor denne først?

Den gir mest verdi raskest:

- mer profesjonell
- bedre safety
- mindre forvirring
- enklere å se runtime-state
- bedre egnet til ekte maskin

---

# 7. Viktigste designregel

UI skal ikke bare se kult ut.

Det skal svare på disse spørsmålene umiddelbart:

1. **Er maskinen trygg å starte?**
2. **Hva skjer hvis jeg trykker START?**
3. **Hva styrer hastigheten akkurat nå?**
4. **Hvilken retning vil den gå?**
5. **Hvorfor stoppet den?**
6. **Hva må jeg gjøre for å resette?**

Hvis UI alltid svarer på dette, blir prosjektet mye mer profesjonelt.
