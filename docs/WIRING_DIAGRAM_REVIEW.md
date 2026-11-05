# Wiring diagram review — 2026-10-03

The original `images/Wiring_diagram.v2.svg` is updated in place to revision 2.6,
using the V5 graphite/orange UI palette. Hardware positions and the original
common-cathode driver topology are retained. No firmware polarity is changed.

## Corrections

- Foot-pedal start switch now terminates at GPIO33, the right-hand header's
  fourth row. The previous route ended beside the 5 V row.
- Foot-pedal potentiometer supply joins the ADS1115 VDD / 3.3 V net. The
  previous route ended inside the ADS card without reaching its VDD net.
- Panel-pot wiper no longer runs along the GND bus.
- STEP/DIR/ENA routes use the gap between the header columns rather than
  crossing GPIO33/31/30 terminals and striking through their labels. Pin-label
  spacing and wire channels are adjusted for legibility.
- NC E-STOP is shown through a required input interface, specifying GPIO34
  HIGH when healthy and LOW on fault. A bare NC-to-GND contact has the opposite
  logic. The functional block does not assert a verified installed relay or
  circuit, or establish an independent hardware power cutoff.
- GPIO32 ALM input and shared GPIO7/SDA, GPIO8/SCL references are explicit.
- Right-hand JP1 rows 10/11 are C6_U0RXD/C6_U0TXD, matching `images/pinout.jpg`;
  they are not duplicate free P4 GPIO31/GPIO30 pins.
- Driver current switches are SW1–SW3; microstep switches are SW5–SW8.
  The physical driver label and UI microstep setting must agree.
- Workpiece RPM limits are 0.001–3.0, subject to the configured maximum and
  actual gearing/roller/workpiece geometry.
- The unmeasured 0.5 ms stop-time claim is removed. The ISR sets ENA HIGH;
  electrical and mechanical stop response still require measurement.
- Notes clarify driver-interface voltage/current, ENA polarity, example coil
  colors, connected dots versus crossings, Setup Wizard, and TIG HF enclosure.

## Sources and validation

Pin assignments and logic were checked against `src/config.h`,
`src/motor/motor.cpp`, `src/safety/safety.cpp` and `src/ui/display.cpp`.
Board header rows are retained from the project's board/pinout reference.
Driver interface and DIP distinctions were cross-checked with the official
[STEPPERONLINE DM542T V4.0 manual](https://omc-stepperonline.com/download/DM542T_V4.0.pdf)
and [enable-signal guidance](https://help.omc-stepperonline.com/hc/s/articles/how-to-connect-the-stepper-driver-enable-signal-ena-and-what-is-its-application).
These sources describe model-dependent interfaces; their presence does not
validate the user's assembled driver or contact wiring.

The SVG is XML-parseable. Browser checks cover text boundaries and manual
inspection at overview and 100% scale. README and the retained Builder Guide
both reference the same corrected original diagram.

Final browser geometry checks covered 190 text elements in 16 hardware,
reference and interface cards, with no text outside the sheet or its card.
All 118 local file, image and heading references in the changed documentation
resolve with exact filename case, including the shared SVG path. Signal/power,
motor-coil and ground endpoints were checked against the illustrated terminals
and the firmware pin assignments. Motor initialization comments now describe
the firmware's ENA contract rather than implying universal direct 5 V wiring.

This review is a documentation correction. No device was flashed, no wiring
was changed, and no physical E-STOP, cable-break or TIG HF test was performed.
Use [the input truth table and stop-time procedure](estop_timing.md) when
commissioning the actual controller.

## Revision 2.8 - 2026-11-05

Legibility only. The layout and every connection are unchanged: an automated
comparison of wire endpoints gives the same 19 nets as revision 2.7.
A browser measured every text box and wire path; these defects were fixed:

- 18 labels had a wire through them: field-device terminal names, GND BUS,
  the band title and the motor colour plate on top of the coil wires.
- 7 parallel runs were 6-9 px apart: STEP/DIR/ENA, ALM, and the ADS1115
  lanes. They are now at least 12 px apart.
- The level-adapter box let the signal wires run through its text.
- DIR switch text sat under the knob and past the pill edge.
- GPIO32, I2C_SDA and I2C_SCL were drawn as unused pins.
- Driver and motor terminal names sat under the wires; they now sit inside the housing.
- The USB-C ports sat across the GPIO29 wire; the 3.3 V wires were half transparent.
- The DIP switches showed a pattern that matches no documented setting.
- Card shadows, mismatched header corners and three orange headers; only the ESP32-P4 keeps the accent.
