# USB-C UI Mirror Design

## Goal

Add a local Windows UI mirror for the ESP32-P4 rotator over the existing USB-C CDC serial link. This must be a real mirror of the running device UI, not a simulator state clone.

The PC viewer will show actual LVGL pixels from the device and send mouse/touch events back to the device so the PC can operate the same UI logic as the physical touchscreen.

## Existing Context

- Firmware already enables native USB CDC with `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`.
- `setup()` starts `Serial` at `115200`, and PlatformIO monitor/upload also use `COM5`.
- LVGL renders logical `800x480` RGB565 landscape frames.
- `lvgl_flush_cb()` in `src/ui/lvgl_hal.cpp` rotates those pixels to the physical `480x800` portrait panel.
- The GT911 touch driver converts physical portrait touch back to logical LVGL landscape coordinates.
- `lvglTask` runs on Core 1 and must not block.
- Safety, motor, and control tasks run on Core 0 and must not receive direct USB commands.

## Chosen Approach

Implement a binary USB mirror protocol with two directions:

1. Device to PC: LVGL dirty rectangles from `lvgl_flush_cb()`.
2. PC to device: pointer press/move/release input in LVGL logical coordinates.

The PC app will be a separate SDL-based viewer target in `simulator/`, launched from PowerShell:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 4000000
```

Default test baud is `4000000`, with fallbacks to `2000000` and `921600`. Because ESP32 native USB CDC is not a real UART clock, the baud value is mostly host/driver configuration. The main throughput requirement is avoiding unnecessary full-frame traffic.

## Alternatives Considered

### Read-only pixel mirror

This is simpler and safer, but it does not meet the requirement. It cannot control the UI from the PC.

### State-based remote UI

This would send machine state to the PC and render a second UI locally. It is faster, but it is not a true mirror and risks drifting from the real LVGL UI.

### Chosen: real pixels plus LVGL pointer injection

This gives the true running UI and sends input through the same LVGL event path as the touchscreen. It keeps motor/control safety rules intact because USB does not call motor functions directly.

## Firmware Components

### `src/mirror/usb_mirror.h/.cpp`

New module behind the build flag `ENABLE_USB_UI_MIRROR=1`.

Responsibilities:

- Own the binary packet protocol.
- Detect PC handshake and keepalive.
- Stream dirty rectangles as non-blocking chunks.
- Accept pointer packets and expose latest pointer state to LVGL.
- Drop mirror output when USB is behind instead of blocking LVGL.
- Release remote touch on timeout, disconnect, bad packet, or disabled state.

### `src/ui/lvgl_hal.cpp`

Changes:

- In `lvgl_flush_cb()`, enqueue the unclipped logical LVGL `area` plus RGB565 pixels before manual panel rotation.
- Register a second LVGL pointer input device when mirror is compiled in.
- The mirror pointer read callback returns PC pointer state only when remote input is armed and keepalive is fresh.

The mirror path must not change physical display behavior. `lv_display_flush_ready()` must still be called exactly once.

### Device UI arm control

Add a simple `USB MIRROR` arm control in the device UI. This is a volatile runtime state, not saved to flash. It starts disabled after boot and returns disabled on disconnect, keepalive timeout, or reboot.

When armed, USB input is allowed to press the same LVGL buttons as the physical screen. This means PC control can start/stop motion through the normal UI, but there is still no separate hidden command channel.

### `src/main.cpp`

Changes:

- Start the mirror task only when `ENABLE_USB_UI_MIRROR=1`.
- Keep it lower priority than safety, motor, control, and LVGL.
- Do not subscribe the mirror task to WDT.

### `platformio.ini`

Add a dedicated mirror environment, for example:

```ini
[env:esp32p4-mirror]
extends = esp32_common
extra_scripts = pre:scripts/pio_pre_unique_bin.py
build_flags =
  ${esp32_common.build_flags}
  -D CORE_DEBUG_LEVEL=0
  -D DEBUG_BUILD=0
  -D ENABLE_USB_UI_MIRROR=1
```

This keeps normal release firmware quiet and avoids accidental mirror traffic in normal serial monitor use.

## PC Viewer Components

### `simulator/usb_mirror_viewer.cpp`

Windows SDL app that:

- Opens the requested COM port at the requested baud.
- Sends handshake and keepalive.
- Parses binary rectangle packets.
- Maintains an `800x480` RGB565 framebuffer.
- Draws the framebuffer to an SDL window.
- Converts mouse/touch events into LVGL logical `x,y,state` input packets.
- Shows connection state in the window title.
- Resynchronizes if normal serial log text or corrupted bytes appear before a packet.

### `simulator/run.ps1`

Add:

```powershell
.\simulator\run.ps1 -UsbMirror COM5 -Baud 4000000
```

Existing simulator, self-test, and screenshot dump behavior must keep working.

## Protocol

Use binary framing with a fixed magic header so the PC can resync after logs/noise:

- Magic: `RMR1`
- Version: `1`
- Type: hello, keepalive, video rectangle, pointer, stats, error
- Sequence number
- Payload length
- Payload CRC32

Video payload:

- Logical `x`, `y`, `w`, `h`
- Pixel format `RGB565_LE`
- Pixel bytes in row-major LVGL logical orientation

Pointer payload:

- Logical `x`, `y` clamped to `0..799`, `0..479`
- State: released, pressed
- Optional button flags for future use

## Safety Rules

- USB mirror must never call `control_start_*`, `control_stop`, motor APIs, or safety APIs directly.
- PC input is only LVGL pointer input.
- Remote input is inactive after boot.
- Remote input requires an explicit arm state on the device UI before pointer packets are accepted.
- Keepalive timeout releases pointer state and disables remote input.
- ESTOP, driver alarm, and existing control state rules remain unchanged.
- Any disconnect or parser fault must fail closed: remote released, no pending press.
- The mirror must never block Core 0 tasks.
- `lvgl_flush_cb()` must never wait on USB writes.

## Performance Rules

- Use LVGL partial render mode so normal UI updates send dirty rectangles, not full 800x480 frames.
- Do not send full frames continuously unless LVGL invalidates the full screen.
- Stream dirty rectangles/chunks only.
- If output queue is full, drop old mirror data and keep the latest UI responsive.
- Prefer small fixed buffers or preallocated PSRAM ring buffers; avoid heap allocation in `lvgl_flush_cb()`.
- Serial logs and mirror traffic share the same USB CDC port, so the mirror environment should use `DEBUG_BUILD=0`.

## Testing

Software checks:

- Native unit tests for packet encode/decode, CRC rejection, resync after junk bytes, coordinate clamp, keepalive timeout, and queue overflow/drop behavior.
- Existing simulator self-test must still pass.
- Simulator screenshot export must dump every registered screen for UI review.
- `pio test -e native` must pass.
- `pio run -e esp32p4-mirror` must build.
- Normal `pio run` release must still build and must not enable mirror by default.

Bench hardware checks:

- Flash mirror firmware to ESP32-P4 over COM5.
- Launch the PC viewer at `4000000` baud, with `2000000`/`921600` as compatibility fallbacks.
- Confirm the PC window follows real device screen changes.
- Confirm PC click/touch activates normal UI buttons.
- Confirm USB unplug releases touch and does not leave a pressed button.
- Confirm ESTOP overrides PC input and motor stays disabled during ESTOP.
- Confirm reboot starts with remote input inactive.
- Confirm normal physical touchscreen still works with mirror connected.

## Out of Scope

- Wi-Fi mirror.
- Browser-based remote UI.
- Direct command API for motor/control.
- Remote operation without explicit device-side arm.
- Internet or network access.
