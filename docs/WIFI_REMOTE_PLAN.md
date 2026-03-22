# 📡 ESP32-C6 Wi-Fi Remote & Web App
*(Arkitetur- og Planleggingsdokument)*

## 1. Systemoversikt
Hovedbrikken (ESP32-P4) er fullstendig dedikert til heavy duty-oppgaver: MIPI-DSI skjermoppdatering, grafikk i LVGL, og sanntids motorstyring. P4-utgavene våre har ikke radiokretser for Wi-Fi bygget inn.
Derfor setter vi inn en syltynn og billig **ESP32-C6** som en dedikert "Network Coprocessor" (NCP). Denne maskinens eneste jobb i livet er å snakke med mobilen din og mate ferdige kommandoer over til skjerm-prosessoren.

## 2. Nettverksmodus: "Access Point" (SoftAP)
ESP32-C6 fungerer som sin egen Wi-Fi-ruter. 
- Den kringkaster et lukket, trådløst nettverk med navn **"TIG_Rotator_Control"**.
- **Ingen domener:** Du trenger aldri å betale for domener, skygge-servere eller opprette DNS ute i felten.
- **Ingen rutere:** Systemet krever *ingen* tilkobling til bedriftens/husets internett. Du lukker bare opp telefonen, kobler til "TIG_Rotator_Control", og maskinen er 100 % operativ fra dypet av den mørkeste kjeller eller ute på en øde byggeplass.

## 3. Innebygd Web App ("App in a chip")
Design og funksjon ligger lagret lokalt på selve maskinen:
- Brukergrensesnittet (HTML, Dark-theme CSS, og Javascript) lagres komprimert i flash-minnet (SPIFFS / LittleFS) på ESP32-C6 brikken. (Akkurat som vi nettopp gjorde med Program Presets).
- For å styre maskinen, åpner du nettleseren (f.eks Safari eller Chrome) på telefonen og taster inn IP-adressen `192.168.4.1`.
- Siden programmeres som en moderne **Progressiv Web App (PWA)**. Dette betyr at du bare kan trykke "Legg til på hjemskjerm" på iPhonen eller Androiden. Neste gang trykker du bare på et maskin-ikon på telefonen din, og appen starter i fullskjerm *nøyaktig* slik en app lastet ned fra App Store oppfører seg (URL-linjen fjernes fullstendig).

## 4. Kommunikasjon & Synkronisering (Teknisk Bro)
Hvis du drar i RPM-slideren på mobilen, hvordan begynner motoren å spinne *øyeblikkelig*? Det skjer via en teknisk lynrask bro oppdelt i to ledd:

### A. Telefon ↔ ESP32-C6 (Trådløst)
- Vi bruker en nettverksteknologi som heter **WebSockets**. Normale nettsider må oppdatere ("refreshe") siden for hver handling. En WebSocket er en konstant, åpen tunnel i bakgrunnen mellom telefonen og ESP-antennen.
- Straks tommelen din trykker på START-knappen, skytes et trådløst bytsignal inn i C6-brikken på et millisekund.

### B. ESP32-C6 ↔ ESP32-P4 (Kablet Hjernebark)
- De to mikrokontrollerne på printkortet/kabinettet er fysisk koblet sammen med tre helt vanlige ledninger: `TX` (Send), `RX` (Motta), og `GND` (Jord). Dette kalles Hardware UART.
- Når ESP32-C6 mottar trådløse WebSockets, pakker den komandoen lynraskt i C++ ned til et minimalt `ArduinoJson`-objekt: 
  `{"cmd": "mode", "val": "START_CONTINUOUS"}`
- Den skyter denne JSON-pakken over UART-ledningen i rasende fart (f.eks 921 600 baud).
- En egen dedikert oppgave i FreeRTOS (`networkTask`) kjører sideløpende på skjermprosessoren (ESP32-P4). Sekundet signalet fanges opp, oversetter den JSON-meldingen og trigger den nøyaktig samme C++ funksjonen (`control_start_continuous()`) som kjøres hvis man hadde trykket med pekefingeren dirkete på 4.3" skjermen!

### Oppsummert Resultat
Vi får en frittstående, pansret, offline-sentrert sveisestyring der operatøren har maskinens fulle grensesnitt rett i lomma — ingen abonnement, ingen internett-forsinkelse, med øyeblikkelig hardware synkronisasjon.
