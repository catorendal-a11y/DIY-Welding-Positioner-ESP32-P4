# TIG Rotator Controller - Project Implementation

**Board**: GUITION JC4880P433 ESP32-P4 4.3" Touch Display Dev Board
**Display**: ST7701S 480×800 MIPI-DSI (rotated to 800×480 landscape)
**Touch**: GT911 capacitive touch controller
**Firmware**: v0.3.0-beta

---

## Table of Contents
1. [MIPI-DSI Display Fix](#1-mipi-dsi-display-fix)
2. [Navigation Fixes](#2-navigation-fixes)
3. [Programs Screen Redesign](#3-programs-screen-redesign)
4. [Custom Program Presets from UI](#4-custom-program-presets-from-ui)
5. [Theme Enhancements](#5-theme-enhancements)

---

## 1. MIPI-DSI Display Fix

### Problem Summary
Initial state: Black screen, no display output.

### Journey - Errors and Solutions

#### Error 1: Wrong GPIO Pins
**Symptom**: Display not initializing
**Cause**: Used wrong GPIO pins from RetroESP32-P4 (BL=15, RST=38)
**Solution**: Changed to JC4880P433 board pins
- `BL_GPIO = 23`
- `LCD_RST = 5`

#### Error 2: Stack Overflow
**Symptom**: `Stack protection fault` in loopTask
**Cause**: Default Arduino stack size too small for MIPI-DSI init
**Solution**: Increased stack in `platformio.ini`:
```ini
-DARDUINO_LOOP_STACK_SIZE=32768
```

#### Error 3: Wrong Display Driver
**Symptom**: Display not responding
**Cause**: Tried ILI9881C driver initially
**Solution**: Switched to ST7701S driver (confirmed by RetroESP32-P4 analysis)

#### Error 4: Wrong Pixel Format
**Symptom**: Dark/corrupted colors, `first_pixel=0x0020` in logs
**Cause**: LVGL RGB565 but display expected RGB888/ARGB8888
**Solution**: Changed to RGB565 throughout (matches BSP and ST7701S)

#### Error 5: DMA Buffer Allocation Failed
**Symptom**:
```
[E] DMA buffer alloc failed!
[E] LVGL buffer allocation failed
Load access fault (MTVAL: 0x00000022)
```
**Cause**: Tried to allocate 184KB in internal DMA-RAM (only ~80-100KB available)
**Solution**: Use internal DMA-RAM with smaller buffers (80 lines = 76800 bytes)

#### Error 6: PSRAM + DMA2D Cache Crash
**Symptom**:
```
E (1011) cache: esp_cache_msync(113): invalid addr or null pointer
Guru Meditation Error: Core 1 panic'ed (Load access fault)
```
**Cause**: DMA2D tries to sync cache on PSRAM addresses (PSRAM not cache-synlig)
**Solution**:
- Disabled DMA2D (`use_dma2d = false`)
- Use internal DMA-RAM for LVGL buffers

#### Error 7: Portrait Instead of Landscape
**Symptom**: UI visible but in portrait mode (480×800)
**Cause**: Missing LVGL rotation configuration
**Solution**:
```cpp
disp_drv.sw_rotate = 1;  // Enable software rotation
lv_disp_set_rotation(display, LV_DISP_ROT_90);
```

#### Error 8: Touch Not Working
**Symptom**: Touch input not responding
**Cause**:
1. Touch initialization skipped
2. Wrong I2C configuration
3. Wrong swap_xy setting

**Solution**: Full GT911 touch init matching BSP:
```cpp
i2c_master_bus_config_t i2c_bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_7,
    .scl_io_num = GPIO_NUM_8,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
        .enable_internal_pullup = 0,  // External pull-ups on board
    },
};
```

### Final Display Configuration

#### Display Settings (display.cpp)
```cpp
// MIPI-DSI Bus
esp_lcd_dsi_bus_config_t bus_cfg = {
    .bus_id = 0,
    .num_data_lanes = 2,
    .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
    .lane_bit_rate_mbps = 500,
};

// DPI Timing (from JC4880P433C BSP)
esp_lcd_dpi_panel_config_t dpi_cfg = {
    .virtual_channel = 0,
    .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
    .dpi_clock_freq_mhz = 34,  // BSP: 34MHz for 60Hz
    .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,  // RGB565 = 16-bit
    .num_fbs = 2,  // 2 framebuffers
    .video_timing = {
        .h_size = 480,
        .v_size = 800,
        .hsync_pulse_width = 12,
        .hsync_back_porch = 42,
        .hsync_front_porch = 42,
        .vsync_pulse_width = 2,
        .vsync_back_porch = 8,
        .vsync_front_porch = 166,
    },
    .flags = { .use_dma2d = false },  // DISABLED: caused crashes
};

// ST7701 Panel
esp_lcd_panel_dev_config_t panel_cfg = {
    .reset_gpio_num = 5,  // LCD_RST
    .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,  // RGB565
    .vendor_config = &vendor_cfg,
};
```

#### ST7701 Custom Init Sequence (from BSP)
Critical! The default driver init sequence doesn't work properly:
```cpp
static const st7701_lcd_init_cmd_t st7701_lcd_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    // ... (full sequence in display.cpp)
    {0x11, (uint8_t[]){0x00}, 1, 120},  // Sleep out
    {0x29, (uint8_t[]){0x00}, 1, 20},   // Display on
};
```

#### LVGL Configuration (lv_conf.h)
```cpp
#define LV_COLOR_DEPTH          16      // RGB565
#define LV_COLOR_16_SWAP        0       // Byte order
#define LV_HOR_RES_MAX         800
#define LV_VER_RES_MAX         480
#define LV_MEM_SIZE     (64*1024U)
```

#### LVGL Driver (lvgl_hal.cpp)
```cpp
// Buffer: Internal DMA-RAM, NOT PSRAM
buf1 = (lv_color_t*)heap_caps_malloc(buf_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

// Driver setup
disp_drv.hor_res = 480;  // Native portrait
disp_drv.ver_res = 800;
disp_drv.sw_rotate = 1;  // Enable software rotation
lv_disp_set_rotation(display, LV_DISP_ROT_90);
```

#### Touch Configuration (display.cpp)
```cpp
// I2C for GT911
#define TOUCH_I2C_SDA    7
#define TOUCH_I2C_SCL    8

// GT911 Touch Config
esp_lcd_touch_config_t tp_cfg = {
    .x_max = 480,
    .y_max = 800,
    .rst_gpio_num = GPIO_NUM_NC,
    .int_gpio_num = GPIO_NUM_NC,
    .flags = {
        .swap_xy = 0,  // Let LVGL handle rotation
        .mirror_x = 0,
        .mirror_y = 0,
    },
};
```

#### VSync Callback (CRITICAL for MIPI DSI Video Mode)
```cpp
extern "C" bool IRAM_ATTR display_lvgl_vsync_callback(
    esp_lcd_panel_handle_t panel,
    esp_lcd_dpi_panel_event_data_t *edata,
    void *user_ctx)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_ctx;
    if (disp_drv) {
        lv_disp_flush_ready(disp_drv);
    }
    return false;
}

// Register on DPI panel, NOT DBI IO
esp_lcd_dpi_panel_register_event_callbacks(display_panel, &cbs, user_ctx);
```

**Important**: `lv_disp_flush_ready()` MUST be called from vsync callback, NOT from flush_cb!

### Key Technical Learnings

1. **JC4880P433C BSP is the reference** - Match its configuration exactly
2. **RGB565 not RGB888** - ST7701S works best with RGB565
3. **DMA2D + PSRAM = Crash** - Use internal DMA-RAM for buffers
4. **Vsync callback is mandatory** - For MIPI DSI Video Mode
5. **Custom ST7701 init** - Default driver sequence incomplete
6. **Software rotation** - LVGL sw_rotate for landscape
7. **Touch swap_xy=0** - Let LVGL handle coordinate mapping

---

## 2. Navigation Fixes

### Problem
Sub-screens (Step, Jog, Timer, Programs) had no way to return to main menu without restarting device.

### Solution
Added back buttons to all sub-screens with consistent styling:

#### Implementation Pattern
```cpp
// Back button (top-left)
lv_obj_t* backBtn = lv_btn_create(screen);
lv_obj_set_size(backBtn, 80, 40);
lv_obj_set_pos(backBtn, 16, 12);
lv_obj_set_style_bg_color(backBtn, COL_BTN_NORMAL, 0);
lv_obj_set_style_radius(backBtn, 6, 0);
lv_obj_set_style_border_width(backBtn, 1, 0);
lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

lv_obj_t* backLabel = lv_label_create(backBtn);
lv_label_set_text(backLabel, "BACK");
lv_obj_set_style_text_font(backLabel, &lv_font_montserrat_14, 0);
lv_obj_set_style_text_color(backLabel, COL_TEXT_DIM, 0);
lv_obj_center(backLabel);
```

#### Files Modified
- [src/ui/screens/screen_step.cpp](src/ui/screens/screen_step.cpp)
- [src/ui/screens/screen_jog.cpp](src/ui/screens/screen_jog.cpp)
- [src/ui/screens/screen_timer.cpp](src/ui/screens/screen_timer.cpp)

#### Design Notes
- **Size**: 80×40px for easy touch
- **Position**: Top-left (16, 12) for consistency
- **Text**: "BACK" instead of arrow symbols (symbol font not available)
- **Color**: Dimmed text (COL_TEXT_DIM) to indicate secondary action
- **Navigation**:
  - Step/Jog/Timer → SCREEN_MENU
  - Programs → SCREEN_MAIN

---

## 3. Programs Screen Redesign

### Problem
Original programs list was too small (456×180px) and didn't match the UI scale of other screens.

### Solution
Complete redesign with larger elements and better layout:

#### Before
```cpp
lv_obj_set_size(programList, 456, 180);  // Too small
lv_obj_set_pos(programList, 20, 75);
```

#### After
```cpp
// Header increased to 60px
lv_obj_set_size(header, SCREEN_W, 60);

// List significantly larger
programList = lv_list_create(screen);
lv_obj_set_size(programList, 760, 350);  // Almost full screen
lv_obj_set_pos(programList, 20, 75);
lv_obj_set_style_text_font(btn, &lv_font_montserrat_16, 0);  // Larger font
```

#### Files Modified
- [src/ui/screens/screen_programs.cpp](src/ui/screens/screen_programs.cpp)

#### Visual Improvements
- List items: 16px font (was unreadable)
- Better spacing and padding
- Improved touch targets
- Consistent with overall UI scale

---

## 4. Custom Program Presets from UI

### Problem
Program presets could only be created by manually editing JSON files in LittleFS. No way to create, edit, or delete programs from the device UI.

### Solution
Full CRUD interface for program management:

### Features Implemented

#### 4.1 Programs List Screen ([screen_programs.cpp](src/ui/screens/screen_programs.cpp))

**Layout:**
- Header with title, count (X / 16), and BACK button
- "+ NEW PROGRAM" button (prominent cyan)
- Scrollable list of program cards

**Each Program Card:**
```
┌────────────────────────────────────────────────────────────┐
│ 1: TIG Pipe 20mm                         ▶ ✏️              │
│ CONTINUOUS | 2.5 RPM                                     ▕  │
│                                                          │  │
└────────────────────────────────────────────────────────────┘
```

**Action Buttons:**
- ▶ **PLAY** - Load and run preset immediately
- ✏️ **EDIT** - Open program editor
- ✕ **DELETE** - Remove with auto-renumbering

**Callbacks:**
```cpp
static void load_preset_cb(lv_event_t* e);  // Play button
static void edit_preset_cb(lv_event_t* e);  // Edit button
static void delete_preset_cb(lv_event_t* e); // Delete button
static void new_program_cb(lv_event_t* e);   // New button
```

#### 4.2 Program Edit Screen ([screen_program_edit.cpp](src/ui/screens/screen_program_edit.cpp))

**Full Parameter Editor:**

```
┌────────────────────────────────────────────────────────────┐
│ CANCEL              EDIT PROGRAM [1]                       │
├────────────────────────────────────────────────────────────┤
│ PROGRAM NAME                                                 │
│ ┌──────────────────────────────────────────────────────┐   │
│ │ TIG Pipe 20mm                                    │   │
│ └──────────────────────────────────────────────────────┘   │
│                                                              │
│ OPERATION MODE                                               │
│ ┌────┐┌────┐┌────┐┌────┐                                  │
│ │CONT ││PULSE││STEP ││TIMER│                              │
│ └────┘└────┘└────┘└────┘                                   │
│                                                              │
│ SPEED (RPM)                                                 │
│ ┌──────┐          2.5          ┌──────┐                   │
│ │  −   │                       │  +   │                   │
│ └──────┘                       └──────┘                    │
│                                                              │
│ [PULSE ON TIME]    [PULSE OFF TIME]         ← PULSE only    │
│ ┌───┐  500 ms  ┌───┐      ┌───┐  500 ms  ┌───┐            │
│ │ − │          │ + │      │ − │          │ + │            │
│ └───┘          └───┘      └───┘          └───┘             │
│                                                              │
│ [STEP ANGLE]                                ← STEP only     │
│ ┌─────┐┌─────┐┌─────┐┌─────┐                                │
│ │ 45° ││ 90° ││180° ││360° │                                │
│ └─────┘└─────┘└─────┘└─────┘                                 │
│                                                              │
│ [DURATION]                                  ← TIMER only    │
│ ┌───┐    30 sec    ┌───┐                                   │
│ │ − │              │ + │                                   │
│ └───┘              └───┘                                    │
│                                                              │
│ ┌──────────────────────────────────────────────────────────┐ │
│ │                    SAVE PROGRAM                          │ │
│ └──────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────┘
```

**Mode-Specific Field Visibility:**
- **CONTINUOUS**: RPM only
- **PULSE**: RPM + ON time + OFF time
- **STEP**: RPM + Angle presets (45°, 90°, 180°, 360°)
- **TIMER**: RPM + Duration (1-3600 sec)

**Save Logic:**
```cpp
static void save_preset_cb(lv_event_t* e) {
  // Validate name
  const char* name = lv_textarea_get_text(nameInput);
  if (strlen(name) == 0) return;  // Require name

  // Update existing or create new
  if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
    g_presets[editSlot] = editPreset;  // Update
  } else {
    if (g_presets.size() >= MAX_PRESETS) return;  // Full
    editPreset.id = g_presets.size() + 1;
    g_presets.push_back(editPreset);  // Create new
  }

  // Copy name and save to LittleFS
  strncpy(g_presets[editPreset.id - 1].name, name, 31);
  storage_save_presets();

  screens_show(SCREEN_PROGRAMS);
}
```

#### Files Created/Modified
- **Created**: [src/ui/screens/screen_program_edit.cpp](src/ui/screens/screen_program_edit.cpp) (~500 lines)
- **Modified**: [src/ui/screens/screen_programs.cpp](src/ui/screens/screen_programs.cpp) (~270 lines)
- **Modified**: [src/ui/screens.h](src/ui/screens.h) - Added `SCREEN_PROGRAM_EDIT` enum and function declarations
- **Modified**: [src/ui/theme.h](src/ui/theme.h) - Added `COL_ACCENT_DARK` color

---

## 5. Theme Enhancements

### New Color Definition
Added to [src/ui/theme.h](src/ui/theme.h):
```cpp
#define COL_ACCENT_DARK lv_color_hex(0x003344)   // Darker accent for button backgrounds
```

### Usage
- **NEW PROGRAM button background** - Provides visual hierarchy
- **Disabled state** - When 16 presets reached, button uses `COL_BG_CARD`

---

## File Changes Summary

| File | Changes |
|------|---------|
| `platformio.ini` | Increased stack size, ST7701 flags |
| `lib/lv_conf.h` | RGB565 color depth |
| `src/ui/display.cpp` | BSP-matching config, custom ST7701 init, touch init |
| `src/ui/display.h` | Vsync callback declarations |
| `src/ui/lvgl_hal.cpp` | Internal DMA-RAM buffers, sw_rotate, RGB565 |
| `src/ui/lvgl_hal.h` | Buffer size config |
| `src/ui/theme.h` | Normal background color, COL_ACCENT_DARK |
| `src/ui/screens/screen_step.cpp` | Added back button |
| `src/ui/screens/screen_jog.cpp` | Added back button |
| `src/ui/screens/screen_timer.cpp` | Added back button |
| `src/ui/screens/screen_programs.cpp` | Complete redesign with CRUD |
| `src/ui/screens/screen_program_edit.cpp` | **NEW** - Full program editor |
| `src/ui/screens.h` | Added SCREEN_PROGRAM_EDIT enum and declarations |
| `STATUS.md` | Updated with all completed features |

---

## Build Command

```bash
cd "c:\Users\Rendalsniken\Documents\New folder (3) - Copy"
C:\Users\Rendalsniken\.platformio\penv\Scripts\pio.exe run -t upload
```

Or use PlatformIO in VSCode with `esp32p4-debug` environment for logging.

---

## Resources
- [JC4880P433C BSP](https://github.com/csvke/esp32_p4_jc4880p433c_bsp) - Reference implementation
- [ST7701 Driver](https://github.com/espressif/esp_lcd_st7701) - ESP-IDF component
- [GT911 Driver](https://github.com/espressif/esp_lcd_touch_gt911) - Touch controller
- [LVGL Documentation](https://docs.lvgl.io/) - UI framework
