# Kodegjennomgang – 1. oktober 2026

Gjennomgått arbeidskopi: `codex/ui-v3-integration`, basert på `929b1f6` (v2.0.9), inkludert lokale, ucommittede UI-endringer. Dette er analysen før retting. Se [implementeringsstatus og endelig verifikasjon](FORBEDRINGER_2026-10-01.md) for hva som nå er endret. Linjenumre og testresultater nedenfor beskriver den tidligere arbeidskopien.

## Omfang og verifikasjon

Gjennomgangen dekker arkitekturen i `src`, motor- og sikkerhetsflytene, alle registrerte skjermers rolle og navigasjon, lagring, USB-speiling, simulator, native-tester og bygg-/releaseoppsett. Detaljlesing er prioritert etter risiko. GT911-driverens feilbane er også undersøkt. Dette er ikke en full linje-for-linje-revisjon av alle tredjepartsbiblioteker eller en hardwaregodkjenning. Genererte SVG-er er designartefakter, ikke bevis på korrekt LVGL-layout.

Kjørt på den aktuelle arbeidskopien:

| Kontroll | Resultat |
| --- | --- |
| `platformio run -e esp32p4-release` | Bestått. RAM 32 732 byte, flash 1 038 260 byte. Dette er byggtall, ikke målt toppforbruk av heap/PSRAM. |
| `platformio test -e native` | 395 av 395 bestått, tre suiter. |
| Simulator, CMake/MinGW | Bygget. |
| Simulator `--self-test` | Skjermoppretting/temabytte består; funksjonstesten stopper med exit 3: `label target not found: > START on MAIN`. |
| `git diff --check` | Ekstra blank linje ved slutten av `screen_main.cpp`. |
| Maskintester, oscilloskop, fysisk motor | Ikke utført. Ingen flashing. |

Simulatoren forventer gamle knappenavn. Feilen beviser at testen og UI-et er ute av synk, ikke i seg selv at START-callbacken er defekt. Resten av funksjonstesten blir ikke kjørt etter dette stoppunktet. Et innledende forsøk med SDL dummy-driver kunne ikke opprette vindu; normal SDL-kjøring ga resultatet over.

## Funn som bør prioriteres først

### 1. P1 – STOP kan overskrives av en startkommando

**Sted:** `src/control/control.cpp:72`, `:324`.

Alle bevegelseskommandoer deler en kø med ett element og `xQueueOverwrite`. STOP etterfulgt av START før neste kontrollsyklus mister STOP. I tillegg er `xQueuePeek` og `xQueueReceive` separate operasjoner: en produsent kan endre innholdet etter at grenen er valgt. Eksempel: STOP_JOG peekes, vanlig STOP overskriver den, STOP konsumeres i STOP_JOG-grenen og stopper ikke kontinuerlig drift.

**Forbedring:** Gjør STOP til en separat, latchet forespørsel med prioritet over start. En stopp skal invalidere ventende starter. Konsumer kommandoen én gang før dispatch, og bruk en generasjon/ticket slik at gammel start ikke blir gyldig etter stopp/reset.

**Test:** STOP→START og START→STOP mellom kontrollsykluser, STOP_JOG/STOP-interleaving, køfull og gjentatte starttrykk. Test faktisk kontrollkode med en falsk motoradapter.

### 2. P1 – Pedal kan starte ved oppstart og fortsette etter et kort trykk

**Sted:** `src/main.cpp:166`, `:203`.

`pedalSwWasPressed` starter som false. Når pedal er aktivert i lagrede innstillinger, blir en pedal som allerede holdes nede ved oppstart tolket som et nytt trykk. Slipp sender bare STOP når tilstanden allerede er RUNNING. Slippes pedalen mens START fortsatt ligger i kø og tilstanden er IDLE, blir startforespørselen stående.

**Forbedring:** Krev observert stabilt slipp før armering ved boot, aktivering og feilreset. Pedalslipp må kansellere pedalens ventende start uansett aktuell tilstand. Identifiser kommandokilde slik at pedal ikke utilsiktet overtar en annen kjøremodus. UI-guard i den nye pedalskjermen løser ikke boot-tilfellet.

**Test:** Oppstart med pedal nede, kort trykk/slipp før kontrolltick, sprett og reset med pedal nede.

### 3. P1 – Feil på pedal-ADC kan gi gammel hastighet eller bytte til panelpotmeter

**Sted:** `src/motor/speed.cpp:105`, `:329`, `:392`.

ADS1115-feil beholder `s_adsLastValue` uten aldersgrense. Tilkoblingsstatus beskriver i praksis oppstartsproben. Pedalverdier utenfor 100–3900 velger dessuten panelpotmeteret automatisk. Dermed kan feil eller endestilling endre hastighetskilde uten et bevisst operatørvalg. Returverdien fra `ads_poll_and_start` samsvarer heller ikke med kommentaren om fersk måling.

**Forbedring:** Publiser verdi, gyldighet, måletidspunkt og feilstatus samlet. Ved foreldet aktiv pedaldata: stopp og krev ny armering. Gjør kildevalg eksplisitt; unngå automatisk overgang til en annen hastighetskilde ved feil.

**Test:** Frakobling under drift, gjentatt timeout, kortslutning til hver skinne og gjeninnkobling.

### 4. P1 – JOG kan beholde et gammelt touchtrykk etter I2C-feil

**Sted:** `src/ui/lvgl_hal.cpp`, `lvgl_touchpad_read_cb`; `lib/esp_lcd_touch_gt911/esp_lcd_touch_gt911.c:223`, `:307`.

HAL ignorerer resultatet fra `esp_lcd_touch_read_data`. Ved feil i første statuslesing returnerer GT911-driveren før `tp->data.points` nullstilles. `get_xy` returnerer deretter de gamle koordinatene og antall punkter. Et tidligere trykk kan derfor fortsatt rapporteres som PRESSED etter frakobling. JOG-stopp er avhengig av RELEASED/PRESS_LOST. Dersom hele LVGL-oppgaven stopper, finnes heller ikke en tidsbegrenset JOG-fornyelse i kontrolloppgaven.

**Forbedring:** Slipp input ved lesefeil, og innfør en tidsbegrenset JOG-tillatelse som må fornyes av ferske inputdata. Kontrolloppgaven må stoppe når fornyelsen uteblir.

**Test:** Hold JOG, injiser I2C-feil etter et gyldig trykk og suspender deretter UI-oppgaven. Begge situasjoner skal stoppe innen en definert, målt frist.

### 5. P1 – Bevegelsessperren tar ikke hensyn til ventende/låst nødstopp

**Sted:** `src/safety/safety.cpp:123`; `src/motor/motor.cpp`, `motor_run_cw/ccw`.

`safety_inhibit_motion` sjekker fysisk LOW og driveralarm, men ikke `g_estopPending` eller `estopLocked`. En kort nødstoppkant setter ENA HIGH i ISR; dersom inngangen blir HIGH før kontrollen rekker å låse systemet, kan en start passere sjekken og trekke ENA LOW igjen i dette vinduet.

**Forbedring:** Samle all latchet sikkerhetstilstand i én sperre som kontrolleres ved alle motoraktiveringer. Gjør feilreset til en eksplisitt kontrollhendelse og verifiser race mellom ISR og aktivering. Flere ordinære GPIO-lesinger alene gir ikke en atomisk aktivering.

**Test:** Injiser korte feilpulser før, mellom og etter aktiveringssjekkene. Verifiser ENA fysisk, også under flashskriving.

### 6. P1 – Sikkerhetsoppgaven kan likevel vente ubegrenset på motorlås

**Sted:** `src/safety/safety.cpp`, `safety_force_stop_stepper`; `src/control/control.cpp:176`; `src/motor/motor.cpp`, `motor_halt`.

Første forceStop-forsøk har 10 ms timeout. Overgangen til ESTOP kaller deretter akselerasjonsrestore og `motor_halt`, som kan bruke `portMAX_DELAY`. Dermed er ikke hele sikkerhetsbanen tidsbegrenset. ISR setter ENA umiddelbart etter den antatte polariteten, men det dokumenterer ikke en øvre grense for videre behandling.

**Forbedring:** Hold umiddelbar deaktivering uavhengig av mutex og logging. Flytt rydding/restore til motoroppgaven og publiser feiltilstand uten ubegrenset venting. Mål stopptid under låskonflikt.

### 7. P1 – Nødstoppdiagram og kode har motsatt logikk

**Sted:** `docs/estop_timing.md`, eksempel med pull-up, GPIO og NC-kjede til GND; `src/safety/safety.cpp:109`.

Diagrammet gir LOW med lukket NC-kjede og HIGH når den åpnes. Koden behandler LOW/FALLING som feil. Dette er en bekreftet dokumentasjonskonflikt; faktisk kobling på din maskin er ikke undersøkt. ENA HIGH=disabled er også en forutsetning som må bekreftes for den konkrete driveren og optokoblingen.

**Forbedring:** Lag én autoritativ sannhetstabell for normal drift, utløst nødstopp, kabelbrudd og spenningsbortfall. Tilpass kode og alle diagrammer til den verifiserte koblingen. Ikke velg ny polaritet ut fra dette dokumentet alene.

### 8. P1 – Oppstart fortsetter uten å bekrefte kritiske oppgaver

**Sted:** `src/main.cpp:346`; `src/safety/safety.cpp`, watchdog-init.

Returverdiene fra opprettelse av safety/motor/control/LVGL-oppgaver sjekkes ikke. «All FreeRTOS tasks started» logges uansett. Watchdog-konfigurasjon og registrering ignorerer også feilkoder.

**Forbedring:** Hold bevegelse sperret til alle kritiske oppgaver har meldt klar, og håndter allokerings-/watchdog-feil som oppstartsfeil. En vellykket task-opprettelse bør følges av en klar-melding fra oppgaven selv.

## Funksjonsfeil og vedlikehold

### 9. P2 – Programutkast nullstilles ved retur fra modusinnstillinger

**Sted:** `src/ui/screens.cpp:60`, `:98`; `src/ui/screens/screen_program_edit.cpp:359`; save-callbackene i `screen_edit_cont/pulse/step.cpp`.

PROGRAM_EDIT gjenbygges ved hver åpning. `pendingEditSlot` nullstilles til -1 etter første oppretting. Underskjermen endrer utkastet, men retur med `screens_show(SCREEN_PROGRAM_EDIT)` oppretter deretter «New Program» med standardverdier. Også eksisterende redigeringsidentitet kan gå tapt.

**Forbedring:** Skill en vedvarende EditSession med utkast og program-ID fra LVGL-widgetenes levetid. Last bare data ved eksplisitt «ny/rediger», og behold utkast ved retur. Test navn, RPM, modusverdier og ID gjennom hele tur-retur-flyten.

### 10. P2 – Potmeter kan overstyre programhastighet uten å flyttes

**Sted:** `src/control/program_executor.cpp`, `speed_slider_set(run.rpm)`; `src/motor/speed.cpp:414`; `src/config.h:93`.

UI/program-hastighet oppheves når enten ADC-endringen er stor eller forskjellen mellom potmeterets RPM og program-RPM er større enn 0,04. Det siste kan være sant umiddelbart, selv med et helt urørt potmeter. Programhastigheten er derfor ikke nødvendigvis hastigheten som blir brukt.

**Forbedring:** Definer eierskap til hastighet per kjøremodus. For manuell overtakelse: krev faktisk potmeterbevegelse eller kryssing av gjeldende settpunkt. Inkluder RPM, diameter og retning i samme startkommando slik at programstart blir et samlet snapshot.

### 11. P2 – Mislykket lagring mister ventende endringer

**Sted:** `src/storage/storage.cpp:375`, `:387`; `src/ui/screens/screen_motor_config.cpp`, `save_apply_cb`.

Dirty-flagget tømmes før skriving, og resultatet fra intern lagring ignoreres. Ved NVS-feil blir det ingen retry med mindre en ny endring skjer. UI viser samtidig «Settings saved!» før varig lagring er bekreftet. Oppstart ignorerer også false fra lasting av settings/presets og kan fortsette med standarder etter korrupte data.

**Forbedring:** Skill «endret», «venter», «lagret» og «feil». Behold generasjon/dirty ved feil, bruk begrenset retry med backoff og vis feil. Valider JSON-rottype og skjema. Skill manglende førstegangsdata fra korrupte eksisterende data.

**Test:** NVS full, simulert write-failure, strømbrudd, ugyldig JSON/rotype og ny endring mens en lagring pågår.

### 12. P2 – Fem minutters dimming blir 44 sekunder

**Sted:** `src/storage/storage.h:28`; `src/ui/lvgl_hal.cpp:36`; `src/ui/screens/screen_display.cpp:116`.

300 sekunder lagres i `uint8_t` og blir 44. HAL leser også inn i `uint8_t`. Den påbegynte UI-endringen til uint16_t lokalt retter ikke lagringsfeltet eller HAL.

**Forbedring:** Bruk `uint16_t` konsekvent og valider tillatte verdier. Eksisterende lagret 44 er tvetydig; migrer dokumentert siden 44 ikke er et tilbudt UI-valg. Test 0, 30, 60, 120 og 300 gjennom lagring og reload.

### 13. P2 – Ny UI-layout er uferdig og simulatorens forventninger er gamle

**Sted:** `src/ui/theme.h:145`, `:308`; `src/ui/screens/screen_timer.cpp:161`; `simulator/main.cpp`.

Den nye headeren er 70 px høy, mens COUNTDOWN fortsatt plasserer kort på y=58/60. Kalibreringslayouten bruker fortsatt `CAL_TOP_Y=53`. Programeditor, kalibrering, motoroppsett, STEP og COUNTDOWN er ikke ferdig tilpasset det nye skjermsettet. Dette er arbeid som gjenstår i min påbegynte UI-integrasjon. Simulatorens gamle START-etikett stopper funksjonstesten.

**Forbedring:** Fullfør skjermene mot felles innholds-/footergrenser. Gi handlinger stabile test-ID-er i stedet for å finne dem bare via synlig tekst. Kontroller faktisk rendret LVGL med lange programnavn, alle temaer, deaktivert tilstand, feiloverlay og tastatur. SVG-kontrollen erstatter ikke dette.

### 14. P2 – Pulsens OFF-tid inkluderer nedbremsing

**Sted:** `src/control/modes/pulse.cpp`, `pulse_update`.

OFF-timeren starter samtidig med `motor_stop`, som gir kontrollert nedbremsing. Motoren kan derfor fortsatt rotere deler av eller hele det viste OFF-intervallet. Dette er ikke nødvendigvis feil hvis OFF betyr «stoppkommando aktiv», men UI-et må ikke love stillstand.

**Forbedring:** Avklar prosesskravet. Hvis OFF betyr stillstand, bruk separate faser for nedbremsing og faktisk pause, og start pausetimer når motoren er stoppet. Test kort OFF med lav akselerasjon.

### 15. P2 – Testene dekker ikke produksjonsflytene godt nok

**Sted:** `platformio.ini`, native `test_build_src=false`; `src/control/test_logic.h`, `src/motor/test_speed_logic.h`.

Mange native-tester kjører hjelpe-/modellkode, ikke den faktiske FreeRTOS-køen, motoroppgaven eller NVS-feilbanen. Noen hjelpefunksjoner deles med produksjon, men 395 grønne tester er ikke dokumentasjon på at kø-, pedal- og sikkerhetsflytene er testet.

**Forbedring:** Trekk deterministisk kontroll- og inputlogikk ut som produksjonsmoduler med adaptere for klokke, motor, GPIO og lagring. Test disse direkte. Behold simulatoren til UI-flyter og hardwaretestene til fysisk timing og elektriske innganger.

### 16. P2 – USB-speiling har ubegrenset sendeløkke

**Sted:** `src/mirror/usb_mirror.cpp`, `serial_write_all`.

Hvis Serial.write fortsetter å returnere 0, prøver løkken videre uten deadline og speiloppgaven kommer ikke tilbake til input/parser. LVGL-siden sjekker selv keepalive-alder, så dette er ikke bevis på at remote JOG alltid blir hengende. Feilen kan derimot blokkere speiling og ny tilkobling.

**Forbedring:** Tidsbegrens sending, avbryt ved frakobling, frigjør chunk og krev ny sesjon ved reconnect. Droppede skjermområder bør utløse en kontrollert full oppdatering, slik at klientbildet ikke blir liggende delvis gammelt.

## Videre forbedringer

- Behold én eier av motorkommandoer og publiser et konsistent status-snapshot til UI. Flere pulse/step-statusfelt er ordinære variabler lest fra UI på en annen kjerne; atomisk kontrolltilstand synkroniserer ikke alle senere feltoppdateringer.
- Gjør LVGL-låsetypen konsistent: `xSemaphoreCreateRecursiveMutex` brukes med vanlige Take/Give. Bruk enten vanlig mutex eller de rekursive API-ene dersom nesting er tilsiktet.
- Skill beregnet RPM/vinkel fra fysisk målt bevegelse. En encoder er nødvendig dersom tapt steg eller sluring skal oppdages automatisk.
- CI bør også bygge debug/mirror og kjøre simulatorflyter. Lås utgivelsesavhengigheter og lagre verktøyversjoner/commit sammen med binærfilene.
- Reduser global undertrykking av kompilatoradvarsler. Formater de nye UI-filene slik at callbacks og guards er lette å vedlikeholde.
- Dokumenter målt worst-case responstid i stedet for å presentere kommentarer som «<1 ms» som et testresultat.

## Anbefalt gjennomføringsrekkefølge

1. STOP-prioritet, pedalens slipp-før-armering, input-feil og latchet sikkerhetssperre. Legg til regresjonstester på produksjonslogikken.
2. Bekreft kobling/polaritet, oppstartsfeil og tidsgrenser på benk med motoren kontrollert og uten sveiseprosess.
3. Rett programutkast, hastighetseierskap og lagringsstatus; deretter dimming og pulssemantikk.
4. Fullfør den eksisterende UI-integrasjonen og få simulatorens komplette flyter grønne før hardwarevisning.
5. Utvid CI og gjennomfør dokumentert hardwaretest med I2C-frakobling, UI-stans, strømbrudd og relevant TIG-støy.

Rapporten legger ikke til nye funksjonsendringer i firmware. Arbeidskopien inneholder fortsatt de tidligere påbegynte UI-endringene. Ingen endringer er pushet til GitHub.
