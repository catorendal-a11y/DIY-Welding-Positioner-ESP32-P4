# PC UI Simulator

## Download and try

[Download the portable Windows x64 ZIP](https://github.com/catorendal-a11y/DIY-Welding-Positioner-ESP32-P4/releases/download/v2.1.1/welding-positioner-v2.1.1-simulator-windows-x64.zip). Extract all files and double-click **Start Simulator.cmd**. No development tools or hardware are required. Runtime libraries and license notices are included. See the [portable guide](../docs/releases/SIMULATOR.md). The build instructions below are for source development.

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
failure. The tests include main-screen RPM +/− and both blocked and available fault-reset states. Firmware and simulator share the V5 theme and numeric font.

## Screen Screenshot Dump

```powershell
.\simulator\run.ps1 -Screenshots artifacts\sim_screens
```

This builds the simulator, creates every registered screen, and writes a BMP per
screen plus active/resettable fault overlays to the requested directory. It is intended for fast UI review before
flashing the ESP32-P4. The dump is still simulator output; hardware-only behavior
such as touch wiring, E-STOP, driver alarm, and real calibration measurement must
be checked on the device.

## USB-C Live Mirror

The live mirror is separate from the simulator. It connects to the real ESP32-P4
over USB-C, draws actual LVGL RGB565 pixels, and sends mouse/touch input back as
LVGL pointer events.

The mirror firmware uses LVGL partial flushes, so it streams dirty rectangles
instead of full 800x480 frames on every refresh. Higher baud helps the Windows
serial path, but reducing mirror traffic is the main speed improvement.

Flash mirror firmware first:

```powershell
pio run -e esp32p4-mirror --target upload
```

Start the viewer:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 4000000
```

Fallback baud:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 2000000
```

Use `921600` only as a compatibility fallback if the Windows USB serial driver is
unstable at higher rates.

On the physical screen, open **Settings > Display > USB MIRROR** and arm it
after the viewer shows a link. The viewer can display pixels before arming, but
PC clicks are ignored until armed.

PlatformIO Monitor cannot use COM5 while the viewer is connected.

## Source dependency update

v2.1.1 uses LVGL **9.6.0** with an explicit SDL software renderer. SDL2 **2.32.10** is current in the compatible SDL2 series on the local Windows toolchain; Linux CI uses its distro SDL2 package. SDL3 is a different API and is not used by the LVGL SDL2 driver. The simulated drive models asynchronous force-stop drain, and movement result codes match the typed 1.4.0 API. [Full migration report](../docs/DEPENDENCY_UPGRADE_2026-10-02.md).

## Requirements

- CMake
- Ninja
- MSYS2 MinGW toolchain with SDL2, expected at `C:\msys64\mingw64`
- PlatformIO dependencies already installed, especially LVGL under `.pio/libdeps`

If LVGL is missing, run the firmware build once first:

```powershell
pio run
```

## Safety

Simulator button presses update local fake state only. They cannot spin the real
rotator and must not be treated as a machine-control path.

USB mirror clicks are real UI input on the device. E-STOP, driver alarm, and
existing firmware safety checks still apply. USB has no direct motor command API
and disconnect releases remote touch input.

## Linux CI

Install SDL2 development headers, Ninja, CMake and Xvfb, then run:

```sh
pio pkg install -e esp32p4-release
cmake -S simulator -B simulator/build -G Ninja
cmake --build simulator/build --target rotator_simulator
xvfb-run -a simulator/build/rotator_simulator --self-test
```

## Maintenance checks and scenarios (source v2.1.1)

The source simulator now compiles the production command dispatcher, motor adapter, all four motion modes and program executor. Only hardware/input/safety/storage adapters are simulated; the save-generation policy is shared. Control runs every 5 ms, independently of 40 ms UI updates. Simulator timing and drive deceleration are models, not measurements. The v2.1.1 portable ZIP includes these additions; the historical v2.1.0 ZIP predates them.

```powershell
simulator/build/rotator_simulator.exe --audit-layout
simulator/build/rotator_simulator.exe --build-info
simulator/build/rotator_simulator.exe --scenario nvs-failure
```

Scenarios: `estop`, `driver-alarm`, `stale-adc`, `i2c-failure`, `rejected-motion`, `nvs-failure`, `stalled-control`. Rejected motion faults when START is pressed. Input faults block reset until cleared; restart the simulator to select another scenario. These scenarios never connect to hardware.

The layout audit measures actual LVGL fonts on registered screens and fault overlays at 800×480. It checks clipped text, insufficient label height and labels outside non-scrolling parents; explicit ellipsis/scrolling and compact event summaries are intentional. Operator-generated text still needs practical visual review.

CMake accepts `LVGL_DIR`, `ARDUINOJSON_DIR` and `SIMULATOR_DEPENDENCY_ROOT` overrides. Windows CI builds, tests and packages the portable EXE from the same source commit. Packaging rejects mismatched/dirty binaries and missing license files; `--allow-dirty` is only for labeled local development archives.

## Setup wizard regression and preview

`--self-test` navigates the actual wizard, motor configuration and calibration editor; observes simulated E-STOP/release/reset; checks separate start/stop; injects failed completion writes and checks retry. It also checks new/existing installation entry, cancellation and stale control/STOP supervision. Simulated physical travel is accelerated only during the commissioning regression; control timers remain unchanged.

```powershell
simulator/build/rotator_simulator.exe --commissioning-preview .pio/setup-preview
```

This runs the same regression and exports the four stages plus failed-save and completed views as BMP files. It never connects to the device.

## Calibration regression and preview

The simulator compiles the actual calibration module and session policy alongside production control. `--self-test` checks interrupted motion, draft isolation, failed/passing verification and save failure/retry, including STOP while saving. `--calibration-preview <directory>` exports eight actual LVGL states and checks their label layouts. See [the calibration guide](../docs/CALIBRATION_WORKFLOW.md). LVGL argument and widget-tree validation are enabled in the simulator; invalid calls abort the test. Diagnostics uses cached driver/DIR timing information without calling the stepper from the UI.

## Program editor regression and preview

The actual LVGL self-test checks idempotent run-mode selection, availability, invalid RPM, decimal commas, UTF-8 byte limits, a 31-character wide name, fine RPM adjustment in Continuous/Pulse/Step settings, draft preservation, save/cancel and keyboard cleanup during navigation.

```powershell
simulator/build/rotator_simulator.exe --program-preview .pio/program-preview
```

This runs the same interaction regression and exports seven actual LVGL states with label-layout checks. [Program editor guide](../docs/PROGRAM_EDITOR.md). It does not connect to hardware.
