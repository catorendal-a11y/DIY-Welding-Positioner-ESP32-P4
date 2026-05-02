# USB UI Mirror Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a real USB-C mirror that streams live ESP32 LVGL pixels to Windows and sends PC mouse/touch input back into the same LVGL UI path.

**Architecture:** Firmware adds a gated `src/mirror` module with testable packet framing, non-blocking rectangle queuing, and LVGL pointer state. `lvgl_flush_cb()` taps logical RGB565 dirty rectangles before physical panel rotation. A Windows SDL viewer opens COM5, renders the framebuffer, sends keepalives, and injects pointer packets.

**Tech Stack:** ESP32-P4 Arduino/FreeRTOS, LVGL 9.5, USB CDC `Serial`, PlatformIO native Unity tests, CMake/SDL2 Windows viewer.

---

## File Structure

- Create `src/mirror/usb_mirror_protocol.h`: pure packet types, constants, CRC32, encode/decode helpers usable by firmware, native tests, and PC viewer.
- Create `src/mirror/usb_mirror.h`: firmware-facing API, guarded so normal builds compile when mirror is disabled.
- Create `src/mirror/usb_mirror.cpp`: firmware transport, queue, Serial read/write, keepalive, pointer state.
- Modify `src/ui/lvgl_hal.cpp`: enqueue LVGL flush rectangles and register remote pointer input.
- Modify `src/main.cpp`: include mirror header and start mirror task only for mirror builds.
- Modify `platformio.ini`: add `esp32p4-mirror` environment with `ENABLE_USB_UI_MIRROR=1`.
- Create `simulator/usb_mirror_viewer.cpp`: Windows SDL serial viewer.
- Modify `simulator/CMakeLists.txt`: build `rotator_usb_mirror.exe`.
- Modify `simulator/run.ps1`: add `-UsbMirror` and `-Baud`.
- Modify `test/test_utils/test_main.cpp`: add protocol and timeout tests.
- Modify `README.md` and `simulator/README.md`: add mirror usage and hardware notes.

---

### Task 1: Protocol Tests

**Files:**
- Modify: `test/test_utils/test_main.cpp`
- Create: `src/mirror/usb_mirror_protocol.h`

- [ ] **Step 1: Write failing protocol tests**

Add tests near the end of `test/test_utils/test_main.cpp`:

```cpp
#include "../../src/mirror/usb_mirror_protocol.h"

void test_usb_mirror_crc32_known_value() {
  const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, usb_mirror_crc32(data, sizeof(data)));
}

void test_usb_mirror_header_round_trip() {
  UsbMirrorHeader h{};
  h.magic = USB_MIRROR_MAGIC;
  h.version = USB_MIRROR_VERSION;
  h.type = USB_MIRROR_PACKET_VIDEO;
  h.flags = 0;
  h.sequence = 42;
  h.payload_len = 12;
  h.payload_crc = 0x12345678u;
  uint8_t out[USB_MIRROR_HEADER_SIZE];
  TEST_ASSERT_TRUE(usb_mirror_write_header(h, out, sizeof(out)));
  UsbMirrorHeader decoded{};
  TEST_ASSERT_TRUE(usb_mirror_read_header(out, sizeof(out), decoded));
  TEST_ASSERT_EQUAL_UINT32(h.magic, decoded.magic);
  TEST_ASSERT_EQUAL_UINT8(h.version, decoded.version);
  TEST_ASSERT_EQUAL_UINT8(h.type, decoded.type);
  TEST_ASSERT_EQUAL_UINT32(h.sequence, decoded.sequence);
  TEST_ASSERT_EQUAL_UINT32(h.payload_len, decoded.payload_len);
  TEST_ASSERT_EQUAL_UINT32(h.payload_crc, decoded.payload_crc);
}

void test_usb_mirror_rejects_bad_magic() {
  uint8_t out[USB_MIRROR_HEADER_SIZE] = {};
  UsbMirrorHeader decoded{};
  TEST_ASSERT_FALSE(usb_mirror_read_header(out, sizeof(out), decoded));
}

void test_usb_mirror_clamps_pointer() {
  UsbMirrorPointer p = usb_mirror_clamp_pointer(999, -4, USB_MIRROR_POINTER_PRESSED);
  TEST_ASSERT_EQUAL_INT16(799, p.x);
  TEST_ASSERT_EQUAL_INT16(0, p.y);
  TEST_ASSERT_EQUAL_UINT8(USB_MIRROR_POINTER_PRESSED, p.state);
}

void test_usb_mirror_keepalive_timeout() {
  TEST_ASSERT_FALSE(usb_mirror_keepalive_fresh(1000, 1601));
  TEST_ASSERT_TRUE(usb_mirror_keepalive_fresh(1000, 1499));
}
```

Register these in `main()` with `RUN_TEST(...)`.

- [ ] **Step 2: Run tests and verify RED**

Run:

```powershell
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" test -e native -f test_utils
```

Expected: compile fails because `src/mirror/usb_mirror_protocol.h` does not exist.

- [ ] **Step 3: Add minimal protocol header**

Create `src/mirror/usb_mirror_protocol.h` with fixed-size structs, little-endian header helpers, CRC32, pointer clamp, and keepalive helper. Keep it Arduino-free so native tests can include it directly.

- [ ] **Step 4: Run tests and verify GREEN**

Run the same native test command. Expected: `test_utils` passes.

---

### Task 2: Firmware Mirror Transport

**Files:**
- Create: `src/mirror/usb_mirror.h`
- Create: `src/mirror/usb_mirror.cpp`
- Modify: `src/main.cpp`
- Modify: `platformio.ini`

- [ ] **Step 1: Write failing compile check**

Add `#include "mirror/usb_mirror.h"` to `src/main.cpp` and add calls:

```cpp
#if ENABLE_USB_UI_MIRROR
  usb_mirror_begin();
#endif
```

after `Serial.begin(...)`, and:

```cpp
#if ENABLE_USB_UI_MIRROR
  xTaskCreatePinnedToCore(usbMirrorTask, "usbMirror", 8192, nullptr, 1, nullptr, 1);
#endif
```

after `storageTask` creation.

Run:

```powershell
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror
```

Expected: build fails because mirror env/header/task are missing.

- [ ] **Step 2: Add mirror env**

Add `[env:esp32p4-mirror]` to `platformio.ini` with `DEBUG_BUILD=0` and `ENABLE_USB_UI_MIRROR=1`. Add a default `#ifndef ENABLE_USB_UI_MIRROR` guard in `usb_mirror.h`.

- [ ] **Step 3: Add transport skeleton**

Implement:

```cpp
void usb_mirror_begin();
void usbMirrorTask(void* pvParameters);
bool usb_mirror_enqueue_rect(const lv_area_t* area, const uint8_t* px_map);
void usb_mirror_read_pointer(lv_indev_data_t* data);
bool usb_mirror_is_armed();
void usb_mirror_set_armed(bool armed);
```

Disabled builds return false/released and do not create tasks.

- [ ] **Step 4: Add safe task behavior**

In `usbMirrorTask`, parse hello/keepalive/pointer packets from `Serial`, write queued video chunks, and fail closed on timeout. Never call control or motor APIs.

- [ ] **Step 5: Verify firmware build**

Run:

```powershell
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run
```

Expected: both builds succeed; release build has mirror disabled.

---

### Task 3: LVGL Flush Tap and Remote Pointer

**Files:**
- Modify: `src/ui/lvgl_hal.cpp`
- Modify: `src/ui/lvgl_hal.h`
- Modify: `src/mirror/usb_mirror.cpp`

- [ ] **Step 1: Add mirror flush call**

In `lvgl_flush_cb()`, after `uint16_t *src = (uint16_t*)px_map;`, call:

```cpp
#if ENABLE_USB_UI_MIRROR
  usb_mirror_enqueue_rect(area, px_map);
#endif
```

This copies or drops data immediately and returns before panel rotation continues.

- [ ] **Step 2: Add remote input read callback**

In `lvgl_hal.cpp`, add:

```cpp
#if ENABLE_USB_UI_MIRROR
static void lvgl_usb_mirror_read_cb(lv_indev_t*, lv_indev_data_t* data) {
  usb_mirror_read_pointer(data);
}
#endif
```

After the GT911 indev registration, create a second pointer indev with this callback.

- [ ] **Step 3: Verify LVGL constraints**

Check that no LVGL calls are added outside `lvglTask`, no physical touch behavior changes, and `lv_display_flush_ready()` remains exactly once per flush path.

- [ ] **Step 4: Build mirror firmware**

Run:

```powershell
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror
```

Expected: success.

---

### Task 4: Device-Side Arm Control

**Files:**
- Modify: `src/ui/screens/screen_display.cpp`
- Modify: `src/ui/theme.h` only if existing settings constants are insufficient

- [ ] **Step 1: Find display settings pattern**

Read `screen_display.cpp` and reuse the existing row/toggle style.

- [ ] **Step 2: Add volatile USB MIRROR toggle**

Add a display settings row labeled `USB MIRROR`. The toggle calls `usb_mirror_set_armed(...)`. It is not persisted to `g_settings`.

- [ ] **Step 3: Build simulator and firmware**

Run:

```powershell
.\simulator\run.ps1 -SelfTest
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror
```

Expected: simulator self-test and firmware build pass.

---

### Task 5: Windows SDL Mirror Viewer

**Files:**
- Create: `simulator/usb_mirror_viewer.cpp`
- Modify: `simulator/CMakeLists.txt`
- Modify: `simulator/run.ps1`

- [ ] **Step 1: Add viewer target**

Create a Windows SDL app that opens the COM port, sends hello/keepalive packets, parses `RMR1` frames, updates an `800x480` texture, and sends pointer packets on mouse events.

- [ ] **Step 2: Add script mode**

Extend `run.ps1`:

```powershell
param(
  [switch]$SelfTest,
  [string]$UsbMirror,
  [int]$Baud = 2000000
)
```

When `-UsbMirror COM5` is passed, run `rotator_usb_mirror.exe COM5 2000000`.

- [ ] **Step 3: Verify build**

Run:

```powershell
.\simulator\run.ps1 -SelfTest
```

Expected: existing simulator target still builds and self-test passes.

Run:

```powershell
cmake --build C:\Users\Rendalsniken\Documents\rotator\simulator\build --target rotator_usb_mirror
```

Expected: viewer target builds.

---

### Task 6: Documentation and Final Verification

**Files:**
- Modify: `README.md`
- Modify: `simulator/README.md`

- [ ] **Step 1: Document usage**

Add USB mirror commands:

```powershell
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror --target upload
.\simulator\run.ps1 -UsbMirror COM5 -Baud 2000000
```

Mention that physical UI must arm `USB MIRROR` first and that PlatformIO monitor cannot use COM5 while the viewer is connected.

- [ ] **Step 2: Run full software checks**

Run:

```powershell
.\simulator\run.ps1 -SelfTest
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" test -e native
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run
"C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe" run -e esp32p4-mirror
```

Expected: all pass.

- [ ] **Step 3: Commit implementation**

Commit all mirror implementation files with:

```powershell
git add src/mirror src/ui/lvgl_hal.cpp src/ui/lvgl_hal.h src/ui/screens/screen_display.cpp src/main.cpp platformio.ini simulator README.md test/test_utils/test_main.cpp docs/superpowers/plans/2026-05-02-usb-ui-mirror.md
git commit -m "feat: add USB UI mirror"
```

Do not stage the existing deleted analysis markdown files unless the user asks.

---

## Self-Review

- Spec coverage: real pixels, remote touch, keepalive, arm gate, high baud, tests, docs, and no direct motor USB API are each covered by tasks above.
- Placeholder scan: no incomplete task text remains.
- Type consistency: protocol names use the `UsbMirror*` prefix and packet constants use `USB_MIRROR_*`.
