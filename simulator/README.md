# PC UI Simulator

Runs the existing LVGL screens on Windows using SDL2. This is a UI simulator only:
no ESP32 hardware, motor driver, GPIO, ESTOP input, flash, or serial protocol is
controlled from the PC.

## Run

```powershell
.\simulator\run.ps1
```

The script configures CMake, builds `rotator_simulator.exe`, then starts the
800x480 UI window.

## Automated UI Smoke Test

```powershell
.\simulator\run.ps1 -SelfTest
```

This creates every LVGL screen, runs update loops, clicks key navigation and
machine-control UI buttons against fake simulator state, and exits non-zero on
failure.

## Requirements

- CMake
- Ninja
- MSYS2 MinGW toolchain with SDL2, expected at `C:\msys64\mingw64`
- PlatformIO dependencies already installed, especially LVGL under `.pio/libdeps`

If LVGL is missing, run the firmware build once first:

```powershell
& "C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run
```

## Safety

Simulator button presses update local fake state only. They cannot spin the real
rotator and must not be treated as a machine-control path.
