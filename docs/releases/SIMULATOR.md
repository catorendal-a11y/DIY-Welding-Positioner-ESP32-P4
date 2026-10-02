# Windows UI Simulator

Try the V5 welding positioner interface without buying hardware or installing development tools.

## Start

1. Download the simulator Windows x64 ZIP from the v2.1.1 GitHub release.
2. Right-click the ZIP and choose Extract All. Keep all extracted files together.
3. Open the extracted folder and double-click Start Simulator.cmd (or rotator_simulator.exe).
4. Use the mouse to operate the touchscreen interface. Close the window to exit.

The window uses the same 800x480 LVGL screens and fonts as the firmware. Explore rotation modes, program editing, settings and diagnostics. No ESP32, motor, serial port, PlatformIO, Python or MSYS2 installation is needed. Runtime libraries are included.

This is an offline UI demonstration. Motor speed, inputs and fault states are simulated. It does not move a motor, connect to your controller, reproduce welding interference, or validate physical safety. Settings are simulated rather than stored on an ESP32. The USB live mirror is a separate tool.

## Self-test and troubleshooting

Run Run Self Test.cmd for the UI regression checks. Results are written to self-test.log in the extracted folder. Use a writable folder such as Downloads or Documents.

If Windows reports a missing DLL, extract the entire archive again; do not copy only the EXE. The package is for 64-bit Windows. The application is not code-signed; Windows may show a publisher or reputation warning. Obtain it from the project's official release and check the adjacent SHA-256 file when verifying the download.

Project and source: https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4

Included license notices are in LICENSES.txt. Build version and source commit are in BUILD.json.
