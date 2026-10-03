# FastAccelStepper re-audit validation — 3 October 2026

Local evidence after correcting RMT allocation to run on Core 0. The real ESP32-P4 branch is compiled in all three firmware environments; host tests execute the production adapter with simulated hardware.

- [438 native/production-control cases](native.txt): pass.
- [Release/debug/mirror compilation and size](firmware.txt): pass.
- [Actual LVGL interaction self-test](simulator.txt): pass, including program, calibration and commissioning flows.
- [800×480 layout audit](layout.txt): zero failures.
- [Actual pinned upstream RMT encoder](encoder.txt): both buffer geometries pass.
- [Six packaging regressions](packaging.txt): pass.
- Release workflow YAML parses; its extracted checksum-generation code compiles and includes firmware, simulator ZIP and adjacent checksum files, excluding only the manifest itself.

These results do not execute ESP32 task creation or measure IRQ affinity, GPIO timing, physical movement, touch or E-STOP latency. The firmware has not been flashed. Publication additionally requires all six CI jobs on the exact tagged master commit; downloadable artifacts come from that run.

[Re-audit and primary sources](../../../FASTACCELSTEPPER_1_4_REAUDIT.md) · [Dependency report](../../../DEPENDENCY_UPGRADE_2026-10-02.md)
