# UI-utkast V3

Nytt SVG-design for alle 22 registrerte skjermbilder i `src/ui/screens.h`,
pluss åtte ekstra tilstander/visninger. Totalt 30 skjermutkast, hver i 800 × 480.

- Åpne `index.html` for et galleri med navigasjon og enkeltfiler.
- Åpne `all_screens.svg` for hele oversikten som skalerbar vektorgrafikk.
- Filene `01_boot.svg` til `30_input_fault.svg` er selvstendige SVG-er.
- `manifest.json` kobler hver visning til riktig ScreenId og forklarer formålet.

## Designvalg

Mørk grafitt, varm oransje for handling/aktivitet, rødt for stopp/feil og grønt
for bekreftet tilstand. Status er alltid også skrevet i tekst. Skriftfamilien
er Bahnschrift med Segoe UI og Arial som reserve. Tekst forblir redigerbar.

Hovedskjermen prioriterer lesbar hastighet, retning og betjeningskilde. RPM fra
stegpulser er merket CALCULATED under kjøring. Sveisehastighet i mm/min vises
sammen med emnediameter. Tallene er illustrasjoner, ikke levende maskindata.

Primærknapper er normalt 56–82 piksler høye. Jog bruker to separate, store
hold-knapper. Når STOP vises på kjøreskjermene, ligger den nederst til høyre.
Skjermtastaturets nederste funksjonstaster er 48 piksler høye. Mindre tekst er
sekundær informasjon og må prøves på det fysiske 4,3-tommers panelet.

## Skjermgrupper

1. **Operate, 01–06:** oppstart, hovedskjerm klar/kjøring, meny, modusvalg, jog.
2. **Process & safety, 07–12:** puls, steg, nedtelling, nødstopp aktiv/klar for reset, bekreftelse.
3. **Programs, 13–18:** liste, redigering, kontinuerlig/puls/steg, tom liste.
4. **Setup, 19–24:** innstillinger, motor, pedal, skjerm, kalibrering og verifikasjon.
5. **Service & input, 25–30:** diagnostikk, systeminfo, om, talltastatur, teksttastatur, pedalfeil.

## Foreslått oppførsel som krever implementering

Dette er en designleveranse. Firmware og LVGL-skjermer er ikke endret.
Følgende detaljer er forbedringsforslag, ikke bekreftelser på eksisterende funksjoner:

- Faktiske READY-signaler under oppstart, i stedet for tidsstyrt fremdrift.
- Pedal må slippes før armering; inputfeil stopper pedalstyrt kjøring.
- Valg av program går til gjennomgang før start.
- Kontinuerlig program har et tydelig felt for automatisk stopp.
- Endring av motorinnstillinger håndheves bare ved stoppet motor.
- USB-visning og tillatelse til fjernbetjening skilles tydelig.
- Talltastatur og teksttastatur er forslag til samlede inndatavisninger.

Trykk på RESET skal bare gå til IDLE. Ny START må alltid være en separat
handling. En skjermindikator erstatter ikke kontroll av fysisk sikkerhetskrets.

## Regenerering

Kjør `python scripts/generate_ui_mockup_v3.py` fra prosjektet.
Generatoren bruker bare Python-standardbiblioteket og kontrollerer automatisk
at alle 22 registrerte ScreenId-er er dekket. Eksisterende SVG-forslag beholdes.
