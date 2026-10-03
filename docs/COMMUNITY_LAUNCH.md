# Community launch drafts

These are publication drafts, not evidence that any post has been published. Check each community's current rules and use the project owner's account.

## Project showcase

**Title:** I built an open-source welding positioner controller with an ESP32-P4 touchscreen

I built this controller for my DIY welding positioner and am looking for feedback from welders and other builders.

It uses a GUITION JC4880P443C 4.3-inch ESP32-P4 touchscreen board, a stepper motor and a STEP/DIR driver. The firmware includes continuous rotation, pulse, step and jog, with a countdown before starting, foot-pedal support, saved programs and fault diagnostics. Version 2.1.1 retains the dark graphite/orange touchscreen interface and adds guided setup, verified calibration and a clearer program editor.

The source, wiring guide and firmware are public. You can also try the interface using a portable Windows simulator without buying any hardware. The simulator demonstrates the UI; it does not control a motor or test the physical safety circuit.

Demo video: https://youtu.be/GygLl6XY-TM

Project, wiring and source: https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4

Windows simulator: https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/download/v2.1.1/welding-positioner-v2.1.1-simulator-windows-x64.zip

I would like to find three people interested in trying the UI or building a controller. If you already use a welding positioner, what controls do you use most, and what would make this useful in your workshop?

## Short project description

An open-source ESP32-P4 controller for a DIY stepper-driven welding positioner. Built for the GUITION JC4880P443C 4.3-inch LVGL touchscreen board, with continuous, jog, pulse and step rotation, a start countdown, foot-pedal support, saved programs and diagnostics. Source, wiring documentation, firmware and a portable Windows UI simulator are available.

## Media

- Lead with a real photograph of the assembled positioner or a clip of it operating.
- Include a current V5 UI screenshot as a second image.
- The existing demo is historical footage; do not describe it as footage of the current release unless it actually shows that release.
- Keep hardware costs and measured performance out of the post until figures are verified.

## Initial publication targets

- Hackaday.io: a project page with build photos, hardware details and a link to the source. A similar rotary welding positioner project exists there: https://hackaday.io/project/192274-rotary-weld-positioner-tables
- Instructables: a complete build tutorial using the maintained [guide](INSTRUCTABLES.md), after checking its cost claims and current UI details. Publishing entry point: https://www.instructables.com/create/
- Welding communities: adapt the showcase to each group's rules and focus on the working build and a specific feedback question. Do not assume project links or promotional posts are permitted.

## Feedback to record

Record published URLs, questions, simulator problems and confirmed external builds. Count a build only when its builder confirms it; downloads, stars and clones alone do not establish a working installation.

## HomemadeTools build thread

**Title:** DIY welding positioner with an open-source ESP32-P4 touchscreen controller

Here is the controller I built for my DIY welding positioner. It uses a NEMA 23 stepper motor, a worm reduction stage and a STEP/DIR driver, controlled by an ESP32-P4 with a 4.3-inch touchscreen.

I wanted the controls on the machine: a speed dial, direction switch, foot-pedal support and saved settings for repeat jobs. The firmware also includes continuous rotation, pulse, step and jog, with a countdown before starting. I recently updated the touchscreen layout.

Build video: https://youtu.be/GygLl6XY-TM

The source and documentation are available here, including the wiring guide and hardware photographs:
https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4

For anyone who wants to explore the controls before building, there is a portable Windows UI simulator in the [v2.1.1 release](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/tag/v2.1.1). It is an offline demonstration and does not operate hardware.

For TIG HF operation I use a grounded metal enclosure for the screen, driver and motor PSU; the project documentation covers this requirement. The electrical stop circuit and driver polarity must be checked for each build.

I would be interested to hear how other builders arrange their speed control and pedal when using a positioner. What would you change in the controls?

## MIG Welding Forum build thread

**Title:** My DIY welding positioner controller — ESP32-P4 touchscreen and stepper drive

I have been working on a controller for my DIY welding positioner and wanted to share the build with others who have made their own rotators.

The setup uses a stepper motor with reduction gearing and a STEP/DIR driver. An ESP32-P4 runs the GUITION JC4880P443C 4.3-inch touchscreen. Speed can be set with the panel dial, with a physical direction switch and optional foot-pedal input. I added saved programs, continuous, pulse, step and jog rotation, and a countdown before starting.

Here is a video of the build: https://youtu.be/GygLl6XY-TM

I have documented the wiring and controller source on GitHub:
https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4

One practical consideration has been TIG HF interference. The screen, driver and motor PSU need the grounded metal enclosure described in the build guide. The current UI is newer than the video footage.

For anyone already using a positioner: do you prefer setting the rotation speed at the panel, or varying it with a pedal? I would appreciate practical feedback on the controls and on how you handle repeat welds.

## Publication status — 2026-10-02

- Hackaday.io: creator application submitted; no project publication confirmed.
- Hackaday.com: tip submitted by the owner; receipt confirmed in the browser on 2026-10-02 (submission 1169638). Editorial coverage has not been confirmed.
- HomemadeTools.net: forum opened; login required before a build thread can be created.
- MIG Welding Forum: rules reviewed; technical build-thread draft prepared; login required. Advertising is prohibited.
- Instructables: guide prepared; no external publication confirmed.
