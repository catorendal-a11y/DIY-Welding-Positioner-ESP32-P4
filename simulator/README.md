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

## USB-C Live Mirror

The live mirror is separate from the simulator. It connects to the real ESP32-P4
over USB-C, draws actual LVGL RGB565 pixels, and sends mouse/touch input back as
LVGL pointer events.

Flash mirror firmware first:

```powershell
& "C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror --target upload
```

Start the viewer:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 2000000
```

Fallback baud:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 921600
```

On the physical screen, open **Settings > Display > USB MIRROR** and arm it
after the viewer shows a link. The viewer can display pixels before arming, but
PC clicks are ignored until armed.

PlatformIO Monitor cannot use COM5 while the viewer is connected.

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

USB mirror clicks are real UI input on the device. E-STOP, driver alarm, and
existing firmware safety checks still apply. USB has no direct motor command API
and disconnect releases remote touch input.
