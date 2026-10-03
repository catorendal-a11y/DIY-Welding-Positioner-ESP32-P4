// LVGL 9.6 Configuration File for TIG Rotator Controller
// ESP32-P4 4.3" Touch Display (800x480, 16-bit color, PSRAM)

#ifndef LV_CONF_H
#define LV_CONF_H

#ifndef LV_CONF_INCLUDE_SIMPLE
#define LV_CONF_INCLUDE_SIMPLE
#endif

// ───────────────────────────────────────────────────────────────────────────────
// COLOR SETTINGS
// ───────────────────────────────────────────────────────────────────────────────
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565

// LVGL 9.6 software blending/style caching improve rendering without a GPU.
#define LV_OBJ_STYLE_CACHE 1
#define LV_USE_CHECK_ARG 1
#define LV_USE_CHECK_OBJ_CLASSTYPE 0
#define LV_USE_CHECK_OBJ_VALIDITY 0

// ───────────────────────────────────────────────────────────────────────────────
// MEMORY SETTINGS
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB // CRITICAL: Use ESP-IDF heap_caps_malloc via CLIB!
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB
#define LV_MEM_SIZE             (128 * 1024U)  // Backup/ignored when using CLIB
#define LV_DRAW_BUF_ALIGN       64    // ESP32-P4 PPA requires 64-byte alignment

// ───────────────────────────────────────────────────────────────────────────────
// MONITORING (LVGL 9.x - LV_USE_SYSMON requires LV_USE_PERF_MONITOR or MEM_MONITOR)
// NOTE: LV_USE_GPU_ESP32_DMA does NOT exist in LVGL 9 — removed!
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_SYSMON           0     // Disable — requires OS hooks we don't need
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

// ───────────────────────────────────────────────────────────────────────────────
// FONTS (Montserrat family — same as before)
// ───────────────────────────────────────────────────────────────────────────────
#define LV_FONT_MONTSERRAT_8     0
#define LV_FONT_MONTSERRAT_10    1     // Used for warning labels (FONT_SMALL)
#define LV_FONT_MONTSERRAT_12    1
#define LV_FONT_MONTSERRAT_14    1
#define LV_FONT_MONTSERRAT_16    1
#define LV_FONT_MONTSERRAT_18    1
#define LV_FONT_MONTSERRAT_20    1
#define LV_FONT_MONTSERRAT_24    1
#define LV_FONT_MONTSERRAT_28    1
#define LV_FONT_MONTSERRAT_32    1
#define LV_FONT_MONTSERRAT_40    1
#define LV_FONT_MONTSERRAT_48    0
#define LV_FONT_DEFAULT          &lv_font_montserrat_14

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS — LVGL 9.6 names (some renamed from 8.x)
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_ANIMIMG           0     // Was LV_USE_ANIMIMAGE in LVGL 8 — renamed in 9!
#define LV_USE_ARC               1     // For RPM gauge arc
#define LV_USE_BAR               1
#define LV_USE_BUTTON            1
#define LV_USE_BUTTONMATRIX      1
#define LV_USE_CALENDAR          0
#define LV_USE_CANVAS            1     // For sequence preview bar
#define LV_USE_CHART             0
#define LV_USE_CHECKBOX          0
#define LV_USE_DROPDOWN          1
#define LV_USE_IMAGE             1
#define LV_USE_IMAGEBUTTON       0
#define LV_USE_KEYBOARD          1     // For program name entry
#define LV_USE_SPAN              0     // Unused; upstream 9.6 validator treats spans as widgets
#define LV_USE_LABEL             1
#define LV_USE_LED               0
#define LV_USE_LINE              1
#define LV_USE_LIST              0     // Lists use the supported flex container API
#define LV_USE_MENU              0
#define LV_USE_MSGBOX            1     // For confirm dialogs
#define LV_USE_ROLLER            0
#define LV_USE_SCALE             1     // CRITICAL: was 0 — needed for RPM gauge!
#define LV_USE_SLIDER            1     // For speed control
#define LV_USE_SPINBOX           1     // For numeric input
#define LV_USE_SPINNER           0
#define LV_USE_SWITCH            0
#define LV_USE_TEXTAREA          1     // For program name entry
#define LV_USE_TABLE             0
#define LV_USE_TABVIEW           0
#define LV_USE_TILEVIEW          0
#define LV_USE_WIN               0

// ───────────────────────────────────────────────────────────────────────────────
// LVGL 9 SPECIFIC FEATURES
// ───────────────────────────────────────────────────────────────────────────────
#define LV_USE_OBJ_PROPERTIES    0
#define LV_USE_FLEX              1     // Used for dynamic list containers
#define LV_USE_GRID              0
#define LV_USE_OBSERVER          1     // UI-thread status bindings (LVGL 9.6 public API)

#endif /* LV_CONF_H */
