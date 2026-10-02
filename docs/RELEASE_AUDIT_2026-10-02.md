# Repository and release audit — 2 October 2026

Target release: **v2.1.0**, V5 UI integration. Public text is English. This audit covers maintained project material; it is not a line-by-line audit of third-party dependencies or physical hardware acceptance.

| Area | Review and update |
| --- | --- |
| README | Original sections/demo/hardware images retained; V5 runtime captures, controls, validation, release link and actual flash layout synchronized. |
| CHANGELOG / STATUS | v2.1.0, 22 screens, current STOP/pedal/JOG/readiness behavior and measurement limits. Historical entries retain their original versions. |
| PROCESS_ANALYSIS | Current dispatch, save retries, fault cleanup, input and UI flows; earlier analysis archived as historical. |
| CONTRIBUTING / templates | English policy, supported fonts, native/firmware/simulator checks, version placeholders and release procedure. |
| SECURITY | Current supported version, conditioned E-STOP contract, software interlocks and armed USB input boundaries. |
| CODE_OF_CONDUCT / LICENSE | Conduct scope clarified. MIT license reviewed; its existing 2026 Cato Rendal copyright remains correct. |
| install.sh | Replaced unrelated Cursor Agent installer with project-local PlatformIO setup/check; syntax checked. Dependency installation is not executed during this audit. |
| PlatformIO / component/config references | Direct libraries remain pinned. Auxiliary ESP-IDF manifest and sdkconfig reference scope clarified. Partition CSV matches emitted binary layout; actual release packaging checks it again. |
| Git attributes / ignore rules | LF for portable scripts/config, binary media attributes, generated release/exports and Python cache ignored. |
| CodeRabbit | Updated STOP generation/JOG/save/English-label guidance to match production behavior. |
| CI / release workflow | Native, three firmware variants and actual LVGL navigation; setup-script checks; tagged release requires successful CI for the exact tag commit, packages those binaries and verifies version/layout before publishing checksummed, licensed bundles. |
| Hardware / safety / builder guides | Current E-STOP polarity and ENA assumptions, idle +/−, countdown and fault-reset behavior. Older diagrams are explicitly legacy references. |
| Reports / historical design | English report filenames/content and regenerated English V4 SVG/PNG/gallery; V3 download archive synchronized. |
| Wiki | Six maintained pages synchronized with v2.1.0, current UI, build, wiring and troubleshooting. Public wiki is published from these source pages. |
| Test records | Historical firmware/date retained; new release checks do not retroactively establish physical validation. |

## Verification boundaries

The earlier published V5 integration passed all five CI jobs. Version-update firmware also passed all native, simulator and release/debug/mirror jobs. Packaging was exercised against those CI binaries: version strings, exact partition layout, archive contents and checksums are checked. The final tag uses its own release workflow and records commit/run provenance in every bundle.

Firmware expects GPIO34 HIGH healthy / LOW fault and ENA HIGH driver disable. Actual electrical wiring, cable-break behavior, mechanical stop latency, power-loss handling and TIG interference remain bench checks. Historical hardware experience is retained separately from validation of new software changes.
