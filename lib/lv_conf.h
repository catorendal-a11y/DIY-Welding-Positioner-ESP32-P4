// LVGL Configuration File for TIG Rotator Controller
// ESP32-P4 4.3" Touch Display (800×480, 16-bit color, PSRAM)

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif

// ───────────────────────────────────────────────────────────────────────────────
// COLOR SETTINGS
// ───────────────────────────────────────────────────────────────────────────────
#define LV_COLOR_DEPTH          16      // RGB565
#define LV_COLOR_SCREEN_TRANSP  0       // No transparency needed

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN RESOLUTION
// ───────────────────────────────────────────────────────────────────────────────
#define LV_HOR_RES_MAX         800
#define LV_VER_RES_MAX         480

// ───────────────────────────────────────────────────────────────────────────────
// MEMORY SETTINGS
// ───────────────────────────────────────────────────────────────────────────────
#define LV_MEM_SIZE     (64*1024U)     // 64KB internal memory (buffers use PSRAM)

// ───────────────────────────────────────────────────────────────────────────────
// GPU / DMA ACCELERATION
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_GPU_ESP32_DMA     0     // Disabled — ESP32-S3 specific, not for P4

// ───────────────────────────────────────────────────────────────────────────────
// MONITORING (debug phase only)
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_PERF_MONITOR      1     // Show FPS and memory usage
#define LV_USE_MEM_MONITOR        0     // Memory monitor (optional)
#define LV_DISP_DEF_REFR_PERIOD  10     // Refresh period in ms

// ───────────────────────────────────────────────────────────────────────────────
// FONTS (Montserrat family)
// ───────────────────────────────────────────────────────────────────────────────
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_48    1

// Disable built-in fonts we don't use
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    1     // Used for warning labels
#define LV_FONT_MONTSERRAT_12    0
#define LV_FONT_MONTSERRAT_28    0
#define LV_FONT_MONTSERRAT_32    0
#define LV_FONT_MONTSERRAT_40    0
#define LV_FONT_DEFAULT          &lv_font_montserrat_14

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS (disable unused to save flash)
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_ANIMIMG           0
#define LV_USE_BAR               1
#define LV_USE_BUTTON            1
#define LV_USE_BUTTONMATRIX      1
#define LV_USE_CALENDAR          0
#define LV_USE_CANVAS            1      // For sequence preview bar
#define LV_USE_CHART             0
#define LV_USE_CHECKBOX          0
#define LV_USE_DROPDOWN          1
#define LV_USE_IMAGE             1
#define LV_USE_IMAGEBUTTON       0
#define LV_USE_KEYBOARD          1      // For program name entry
#define LV_USE_LABEL             1
#define LV_USE_LED               0
#define LV_USE_LINE              1
#define LV_USE_LIST              1      // For programs list
#define LV_USE_MENU              0
#define LV_USE_MSGBOX            1      // For confirm dialogs
#define LV_USE_ROLLER            0
#define LV_USE_SCALE             0
#define LV_USE_SLIDER            1      // For speed control
#define LV_USE_SPINBOX           1      // For numeric input
#define LV_USE_SPINNER           0
#define LV_USE_SWITCH            0
#define LV_USE_TEXTAREA          1      // For program name entry
#define LV_USE_TABLE             0
#define LV_USE_TABVIEW           0
#define LV_USE_TILEVIEW          0
#define LV_USE_WIN               1
#define LV_USE_ARC               1      // For RPM gauge
#define LV_USE_OBJ_PROPERTIES    0

// ───────────────────────────────────────────────────────────────────────────────
// LAYOUTS
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_FLEX              1
#define LV_USE_GRID              1

#endif /* LV_CONF_H */
