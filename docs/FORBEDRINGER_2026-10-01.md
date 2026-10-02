# Implementerte forbedringer – 1. oktober 2026

Arbeidsgren: `codex/ui-v3-integration`, basert på `929b1f6` (v2.0.9). Endringene ble publisert på GitHub i commit `65238f3` 2. oktober 2026; v2.1.0 er release-versjonen som følger denne integrasjonen. Etter brukerens bekreftelse ble release-firmwaren lastet opp til ESP32-P4 på COM3. Esptool verifiserte datahash og sendte omstart via RTS. Ingen fysisk motortest er utført.

Gjennomgangen omfatter kontrollflyt, pedal/touch, sikkerhetsoppgaver, motorinnstillinger, lagring, USB-speiling, alle registrerte skjermbilder, simulator, tester, bygg og dokumentasjon. Tredjepartsbiblioteker er ikke fullstendig revidert.

## Status for de opprinnelige funnene

Numrene viser til [kodegjennomgangen](KODEGJENNOMGANG_2026-10-01.md).

| Funn | Implementering | Verifikasjon og begrensning |
| --- | --- | --- |
| 1. STOP overskrives | Separat stopp-latch og generasjon på startkommandoer. Én mottaksoperasjon per kommando; ingen overskriving av køen. | Produksjonens `MotionGate` testes direkte. Hele FreeRTOS-dispatcheren er ikke kjørt i en integrasjonstest. |
| 2. Pedal starter ved boot / etter slipp | Stabilt slipp i 50 ms kreves før armering. Slipp kansellerer også pedalens ventende start. | Produksjonens `PedalInterlock` testes ved boot, kort trykk, feil og deaktivering. Fysisk kontaktsprett er ikke målt. |
| 3. Gammel pedalverdi | Gyldighet og alder sjekkes. Aktiv pedalfeil låser bevegelse; ingen automatisk overgang til panelpotmeter. | Aldersgrense og tidsomslag testes. ADS1115-frakobling og elektriske feil må prøves fysisk. |
| 4. Fastlåst JOG | Touchlesefeil gir RELEASED. JOG må fornyes fra UI; kontrolloppgaven stopper ved uteblitt fornyelse i 150 ms. | Firmware bygger, UI-flyten kjører i simulator. Faktisk stopptid inkluderer oppgavescheduling og nedbremsing. |
| 5. Mangelfull sperre | Sperren inkluderer ventende/låst nødstopp, oppgaveberedskap og pedalhelse. Sjekkes også ved motoraktivering. | Kodekontroll og bygg. Dette gjør ikke GPIO-aktivering og ISR atomiske; korte feilpulser ved aktivering/reset krever særskilt måling. |
| 6. Blokkerende sikkerhetsopprydding | ESTOP publiseres uten logging/motorlås i tilstandsovergangen. Potensielt blokkerende opprydding flyttes til kontrolloppgaven. | Kodekontroll og bygg. Låsstrid og tidsgrenser må måles på enheten. |
| 7. Motsatt nødstoppolaritet | Dokumentasjonen beskriver nå HIGH=frisk, LOW=feil og at direkte NC-til-GND er inkompatibelt. | Dokumentasjonsfeilen er rettet. Faktisk krets, kabelbruddrespons og ENA-polaritet er ikke bekreftet. Eldre koblingsillustrasjoner må vurderes mot denne kontrakten. |
| 8. Ubekreftet oppstart | Returverdier fra oppgave-/watchdogoppretting sjekkes. Start krever beredskap fra fire kritiske oppgaver. Initialiseringsfeil deaktiverer driveren og stopper oppstarten. | Alle firmwarevarianter bygges. Feilinjeksjon på enheten gjenstår. |
| 9. Utkast forsvinner | Programutkast bevares ved retur fra modusredigering. Periodisk UI-oppdatering overskriver ikke CONT/PULSE-endringer. | Simulator tester retur, redigering, oppdatering og lagring. |
| 10. Programhastighet overskrives | Programmet sendes som et snapshot til kontrolloppgaven. Programhastighet beholder prioritet; statisk RPM-avvik brukes ikke til å overta fra UI. | Bygg og kodekontroll. Hastighetsrespons med ekte potmeter må prøves. |
| 11. Lagring mister endringer | `SaveRequest` beholder endringer ved feil og under pågående skriving, med gradvis lengre retry-intervall. UI viser ventende lagring/feil. Ugyldig lagring ved boot blokkerer oppstart. | Produksjonspolicy testes for feil/retry og ny forespørsel under skriving. Fysisk strømbrudd/NVS-feil er ikke injisert. |
| 12. Dimming 300 blir 44 | Timeout har 16-bit bredde. Legacy-verdi 44 migreres til 300. | Direkte test av produksjonspolicy. |
| 13. UI og tester ute av synk | Felles visuelt system integrert i LVGL, skjermnavigasjon oppdatert og stabile handlings-ID-er på hovedkontrollene. | Faktisk LVGL-simulator består selvtesten. 22 skjermbilder eksportert og visuelt gjennomgått. |
| 14. Puls OFF inkluderer bremsing | OFF-timeren starter etter at motorbiblioteket rapporterer stopp. | Bygg og kodekontroll. Mekanisk stillstand er ikke målt av en encoder. |
| 15. Testene kopierer kode | Åtte nye native-tester bruker produksjonens stopp-, pedal-, ferskhets-, dimme- og lagringspolicy. Simulatoren tester faktiske skjerm-callbacks. | Forbedret, men ikke fullført dekning: simulatorens motor/lagring er stubber. Full kontroll-/FreeRTOS-integrasjon og hardwarefeilinjeksjon gjenstår. |
| 16. Ubegrenset USB-sending | Sendeløkke har tidsfrist, korte driver-timeouts og begrensede skriveblokker. Feil frigir peker og fjernkontroll; tapte bilder utløser ny opptegning. Delvis initialisering ryddes opp. | Mirror-firmware bygger. Frakobling, baktrykk og gjentatt tilkobling må prøves over ekte USB. |

## Flere forbedringer

- Motorinnstillinger og START går gjennom samme kontrollkø. UI viser om endringen venter, er avvist eller venter på NVS-lagring.
- Hovedskjermen viser aktiv kjøremodus, hastighetskilde og kommandert motorretning. Meny/pedalendring sperres under bevegelse, mens STOP er tilgjengelig.
- CONT-programmer kan redigere automatisk stopp. Valgt modus har lesbar tekst; mikrosteppvalg markerer bare én verdi.
- Kalibreringsinnholdet kan rulles, mens bevegelsesknapper og bunnhandlinger ligger fast. JOG beholder fornyelsen mens knappen holdes.
- Systeminformasjon viser utilgjengelig temperatur fremfor en falsk nullverdi. Heapvisningen bruker samsvarende minnekategorier; lastberegning unngår 32-bit multiplikasjonsoverløp.
- Hastighet merkes som beregnet der den er avledet fra stegpulser. Oppstartsskjermen beskriver initialisering, ikke en fullført maskinselvtest.
- Omstart sperres under bevegelse og ved ventende/feilet lagring.
- Delte modusverdier bruker atomics; hendelseslogging venter ikke på logglåsen og kan dermed miste en logghendelse ved konkurranse.
- Bibliotekversjoner er låst. CI omfatter release/debug/mirror og LVGL-simulator. Den nye GitHub Actions-flyten er ikke kjørt på GitHub i denne gjennomgangen.

## UI-artefakter

- [SVG-mockuper, 30 skjermbilder og tilstander](images/ui_mockup_v3/index.html)
- [Samlet SVG](images/ui_mockup_v3/all_screens.svg)
- [Faktiske LVGL-skjermbilder](images/ui_runtime_v3/overview.png)

Runtime-bildene bruker simulatordata. Den generelle bekreftelsesskjermen mangler kontekst i skjermbildeeksporten; selvtesten åpner og tester dialogen med innhold. Kalibrering viser et rullbart utsnitt. SVG-ene er designreferanse; runtime-bildene viser implementasjonen.

## Verifikasjon

| Kontroll | Resultat |
| --- | --- |
| Native-tester | **403 / 403 bestått** |
| LVGL-simulator `--self-test` | **PASS** |
| Release / debug / mirror | **Alle tre bygger** |
| SVG XML | 31 filer kan parses (30 mockuper + samlet oversikt) |
| Runtime-eksport | 22 skjermbilder |
| `git diff --check` | Bestått |

[Maskinlesbare resultater](validation/2026-10-01/results.json), [native-logg](validation/2026-10-01/native-tests.log), [UI-logg](validation/2026-10-01/ui-self-test.log) og [bygglogg](validation/2026-10-01/firmware-build.log). Byggene har en advarsel i Arduino-rammeverkets SPI-kode om en volatile-kvalifikator; ingen byggfeil. Native-tester og simulator erstatter ikke måling av den fysiske maskinen. Før bruk må særlig nødstopp/ENA/kabelbrudd, pedal- og touchfeil, pulstiming og USB-frakobling prøves på benk med motorutgangen kontrollert.

## Opplasting etter brukerbekreftelse

Release-firmware ble lastet opp via COM3 til ESP32-P4 revision v1.0, MAC `30:ed:a0:e2:34:35`. PlatformIO rapporterte SUCCESS og esptool bekreftet «Hash of data verified». Dette bekrefter flashoverføringen, ikke skjermfunksjon eller maskinens sikkerhetsfunksjoner. [Opplastingslogg](validation/2026-10-01/firmware-upload.log).
