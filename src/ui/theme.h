// TIG Rotator Controller - UI Theme
// POST industrial panels (ui_screens_post_style_proposal.svg + boot screen)
// ESP32-P4 4.3" Touch Display: 800x480 landscape

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// BACKGROUND COLORS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x050505)   // SVG: #050505 screen background
#define COL_BG_HEADER   lv_color_hex(0x090909)   // SVG: #090909 header bar (.hd)
#define COL_BG_CARD     lv_color_hex(0x0B0B0B)   // POST card background
#define COL_BG_CARD_ALT lv_color_hex(0x0B0B0B)   // Alt card (even rows in lists)
#define COL_BG_DIM      lv_color_hex(0x080808)   // SVG: #080808 dim area (.dim)
#define COL_BG_ROW      lv_color_hex(0x080808)   // POST data rows
#define COL_BG_EMPTY    lv_color_hex(0x1A1A1A)   // Empty-slot placeholder rows / neutral dark fill
#define COL_BTN_BG      lv_color_hex(0x0B0B0B)   // POST button/card fill
// COL_BG_ACTIVE is now dynamic (see g_accent_dim below)
#define COL_BG_DANGER   lv_color_hex(0x1A0A0A)   // SVG: #1A0A0A danger/stop (.br)
#define COL_PANEL_BG    lv_color_hex(0x141414)   // Panel background
#define COL_SEPARATOR   lv_color_hex(0x1A1A1A)   // SVG: separator line color
#define COL_BG_INPUT    lv_color_hex(0x0A0A0A)   // Input field background
#define COL_GAUGE_BG    lv_color_hex(0x111111)   // SVG: #111 gauge track
#define COL_TOGGLE_OFF  lv_color_hex(0x333333)   // Toggle switch "off" background

// ───────────────────────────────────────────────────────────────────────────────
// DYNAMIC ACCENT COLORS (set at runtime from theme palette)
// ───────────────────────────────────────────────────────────────────────────────
extern lv_color_t g_accent;
extern lv_color_t g_accent_dim;
extern lv_color_t g_accent_border;

#define COL_ACCENT      g_accent
#define COL_BG_ACTIVE   g_accent_dim
#define COL_BORDER_ACT  g_accent_border
#define COL_AMBER       g_accent
#define COL_GREEN       lv_color_hex(0x00C853)
#define COL_RED         lv_color_hex(0xFF1744)
// POST "OK" pill / pass row (proposal .ok)
#define COL_BG_OK       lv_color_hex(0x062616)
#define COL_BORDER_OK   lv_color_hex(0x00C853)
// POST warning panel (proposal .warn) — not the orange accent COL_WARN
#define COL_BG_WARN_PANEL lv_color_hex(0x241B00)
#define COL_BORDER_WARN   lv_color_hex(0xFFAA00)
// Semantic warning colors (NOT tied to accent — kept stable across themes)
#define COL_WARN        lv_color_hex(0xFF9500)   // Orange: flash usage / driver alarm / active cal step
#define COL_YELLOW      lv_color_hex(0xFFAA00)   // Amber-yellow: countdown mid-range, transient warnings

void theme_init();
void theme_set_color(uint8_t idx);
void theme_refresh();
const char* theme_get_name(uint8_t idx);
uint8_t theme_get_count();

// ───────────────────────────────────────────────────────────────────────────────
// TEXT COLORS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_TEXT        lv_color_hex(0xAAAAAA)   // SVG: #AAA value text
#define COL_TEXT_BRIGHT lv_color_hex(0xCCCCCC)   // SVG: #CCC bright text
#define COL_TEXT_WHITE  lv_color_hex(0xD0D0D0)   // SVG: #D0D0D0 large values
#define COL_TEXT_DIM    lv_color_hex(0x666666)   // Label text (was #4A4A4A)
#define COL_TEXT_VDIM   lv_color_hex(0x555555)   // Very dim text (was #333333)
#define COL_TEXT_TITLE  lv_color_hex(0x777777)   // Title text (was #555555)

// ───────────────────────────────────────────────────────────────────────────────
// BORDER COLORS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BORDER      lv_color_hex(0x2E2E2E)   // SVG: #2E2E2E inactive border (.b)
// COL_BORDER_ACT is now dynamic (see g_accent_border below)
#define COL_BORDER_SM   lv_color_hex(0x333333)   // SVG: #333 small button border (.sm)
#define COL_BORDER_ROW  lv_color_hex(0x1E1E1E)   // SVG: #1E1E1E row border
#define COL_BORDER_DNG  lv_color_hex(0xFF1744)   // SVG: #FF1744 danger border (.br)
#define COL_GAUGE_TICK  lv_color_hex(0x444444)   // SVG: #444 gauge tick marks
#define COL_GAUGE_MINOR lv_color_hex(0x2A2A2A)   // SVG: #2A2A2A minor ticks

// ───────────────────────────────────────────────────────────────────────────────
// SLIDER / BAR COLORS (shared across sliders, bars, progress widgets)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_SLIDER_TRACK    lv_color_hex(0x1A1A1A)   // Slider/bar main track
#define COL_SLIDER_TRACK2   lv_color_hex(0x3A3A3A)   // Lighter track on dark rows
#define COL_SLIDER_TRACK3   lv_color_hex(0x444444)   // Even lighter track (display brightness)
#define COL_SLIDER_BORDER   lv_color_hex(0x555555)   // Slider track border
#define COL_SLIDER_BORDER2  lv_color_hex(0x666666)   // Alt slider track border
#define COL_KNOB_BORDER     lv_color_hex(0xFFFFFF)   // Slider knob highlight border
#define COL_PROGRESS_BG     lv_color_hex(0x222222)   // Progress bar / wizard bar background
#define COL_PROTRACTOR_BG   lv_color_hex(0x0A0A0A)   // Protractor / waveform box background

// ───────────────────────────────────────────────────────────────────────────────
// BUTTON STYLES (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────
// Active: .ba { fill:#1A1600; stroke:#FF9500; stroke-width:2 }
// Normal: .b  { fill:#141414; stroke:#2E2E2E; stroke-width:1.5 }
// Danger: .br { fill:#1A0A0A; stroke:#FF1744; stroke-width:2 }
// Small:  .sm { fill:#141414; stroke:#333;    stroke-width:1.5 }
#define COL_BTN_NORMAL  COL_BTN_BG              // Normal button fill
#define COL_BTN_ACTIVE  COL_BG_ACTIVE           // Active button fill
#define COL_BTN_DANGER  COL_BG_DANGER           // Danger button fill

// ───────────────────────────────────────────────────────────────────────────────
// RADIUS CONSTANTS (from new_ui.svg — mostly rx=2 or rx=4)
// ───────────────────────────────────────────────────────────────────────────────
#define RADIUS_BTN      3      // Standard POST button radius
#define RADIUS_BTN_SM   3      // Small button radius
#define RADIUS_CARD     3      // POST card radius (proposal rx=3)
#define RADIUS_ROW      3      // POST data row radius

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN DIMENSIONS
// ───────────────────────────────────────────────────────────────────────────────
#define SCREEN_W        800
#define SCREEN_H        480

// ───────────────────────────────────────────────────────────────────────────────
// LAYOUT CONSTANTS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────

// Header bar (POST proposal: 38px on all screens)
#define HEADER_H        38
// Inset accent hairline below header strip (ui_screens_post_style_proposal.svg y=44; gap under 38px hdr)
#define HEADER_ACCENT_LINE_Y   (HEADER_H + 6)
#define HEADER_ACCENT_PAD_X    48

// Standard button sizes (from SVG specs bar)
// BTN: 152x36 (bottom row on main), CARD: 784x56, GRID: 380x170
// ACTION: 260x52, STOP: 300x60
#define BTN_W_BOTTOM    152    // Bottom row button width
#define BTN_H_BOTTOM    36     // Bottom row button height
#define BTN_W_ACTION    260    // Action button (SAVE/CANCEL)
#define BTN_H_ACTION    52     // Action button height
#define BTN_W_STOP      180    // Stop button width
#define BTN_W_START     180    // Start button width
#define BTN_H_STOP      42     // Stop/start button height

// Small +/- buttons
#define BTN_W_PM        44     // Plus/minus button width
#define BTN_H_PM        30     // Plus/minus button height

// Menu grid items
#define GRID_W          380    // Menu grid item width
#define GRID_H          170    // Menu grid item height

// Program cards
#define CARD_W          784    // Program card width
#define CARD_H          56     // Program card height

// Side padding
#define PAD_X           16     // Standard X padding
#define PAD_X2          12     // Tight X padding (SVG: 12)

// Boot screen POST layout
#define BOOT_HEADER_H       HEADER_H
#define BOOT_PAD           16
#define BOOT_TITLE_Y       52
#define BOOT_TITLE_H       58
#define BOOT_LEFT_X        16
#define BOOT_LEFT_Y        126
#define BOOT_LEFT_W        456
#define BOOT_LEFT_H        250
#define BOOT_RIGHT_X       488
#define BOOT_RIGHT_Y       126
#define BOOT_RIGHT_W       296
#define BOOT_RIGHT_H       250
#define BOOT_BOTTOM_X      16
#define BOOT_BOTTOM_Y      392
#define BOOT_BOTTOM_W      768
#define BOOT_BOTTOM_H      72
#define BOOT_ROW_X         32
#define BOOT_ROW_Y         178
#define BOOT_ROW_W         424
#define BOOT_ROW_H         24
#define BOOT_ROW_GAP       28
#define BOOT_PROGRESS_X    32
#define BOOT_PROGRESS_Y    426
#define BOOT_PROGRESS_W    736
#define BOOT_PROGRESS_H    10

// ───────────────────────────────────────────────────────────────────────────────
// MAIN SCREEN CONSTANTS (SCREEN_MAIN) — pot-only RPM: no +/- buttons, larger gauge
// ───────────────────────────────────────────────────────────────────────────────
#define MAIN_GAUGE_CX         400
// Arc bbox: height = 2*MAIN_GAUGE_R + MAIN_GAUGE_TRACK_EXTRA (screen_main arc lv_arc widget).
// CY chosen so top clears header + hdr-accent (~44); tighter EXTRA avoids fouling footer @412.
#define MAIN_GAUGE_CY         228
#define MAIN_GAUGE_R          178
#define MAIN_GAUGE_TRACK_EXTRA 8
#define MAIN_GAUGE_TRACK_PAD  12
#define MAIN_GAUGE_TRACK_W    14
#define MAIN_GAUGE_IND_W      10
// "RPM" unit row: TOP_MID on screen; value label stacked above with MAIN_RPM_VALUE_GAP
#define MAIN_RPM_TAG_Y        (MAIN_GAUGE_CY + 24)
#define MAIN_RPM_VALUE_GAP    6
// Extra upward shift for RPM digits (font already FONT_HUGE / 40pt max on P4)
#define MAIN_RPM_VALUE_LIFT   12
// Optical horizontal shift (SCREEN_MAIN): LV_ALIGN_TOP_MID x_ofs for value + unit + pot hint
#define MAIN_RPM_CENTER_OFS_X (-6)
// Pot hint below RPM cluster (same absolute row as pre-CY tune when MAIN_GAUGE_CY + 164)
#define MAIN_POT_HINT_Y       (MAIN_GAUGE_CY + 164)
// 256 = 100% (lv scale); slight zoom since montserrat_48 is unsafe on ESP32-P4
#define MAIN_RPM_VALUE_ZOOM   332

// Legacy aliases (other code / docs)
#define GAUGE_CX        MAIN_GAUGE_CX
#define GAUGE_CY        MAIN_GAUGE_CY
#define GAUGE_R         MAIN_GAUGE_R

// Bottom button row (SVG: y=436, buttons 152x36 or similar)
#define BOTTOM_ROW_Y    436
#define BOTTOM_ROW_H    36

// ───────────────────────────────────────────────────────────────────────────────
// JOG screen (SCREEN_JOG) — RPM row + / - (right-aligned, non-overlapping)
// ───────────────────────────────────────────────────────────────────────────────
#define JOG_RPM_ROW_Y         56
#define JOG_RPM_TITLE_X       20
#define JOG_RPM_VAL_X         160
#define JOG_RPM_BAR_X         240
#define JOG_RPM_BAR_W         392
#define JOG_RPM_BAR_H         3
#define JOG_RPM_BAR_Y_OFF     12
#define JOG_RPM_BTN_W         64
#define JOG_RPM_BTN_H         52
#define JOG_RPM_BTN_Y         (JOG_RPM_ROW_Y - 3)
#define JOG_RPM_BTN_GAP       14
#define JOG_RPM_BTN_PLUS_X    (SCREEN_W - PAD_X - JOG_RPM_BTN_W)
#define JOG_RPM_BTN_MINUS_X   (JOG_RPM_BTN_PLUS_X - JOG_RPM_BTN_GAP - JOG_RPM_BTN_W)

// ───────────────────────────────────────────────────────────────────────────────
// SETTINGS SCREEN LAYOUT (shared across all settings sub-screens)
// ───────────────────────────────────────────────────────────────────────────────
#define SET_HEADER_H       HEADER_H
#define SET_HEADER_FONT    FONT_SUBTITLE
#define SET_ROW_H          52
#define SET_TOGGLE_W       80
#define SET_TOGGLE_H       40
#define SET_TOGGLE_R       12
#define SET_FOOTER_Y       428
#define SET_FOOTER_H       44
#define SET_BTN_MIN_W      140
#define SET_CYCLE_W        110
#define SET_CYCLE_H        36
#define SET_SLIDER_H       20
#define SET_BAR_H          10
#define SET_SECTION_FONT   FONT_NORMAL
#define SET_KEY_FONT       FONT_NORMAL
#define SET_VAL_FONT       FONT_SUBTITLE
#define SET_BTN_FONT       FONT_SUBTITLE
#define SET_CHEVRON_COL    COL_TEXT_VDIM

// ───────────────────────────────────────────────────────────────────────────────
// SYSTEM INFO (SCREEN_SYSINFO) — layout matches docs/images/ui_screens_post_style_proposal.svg #19
// ───────────────────────────────────────────────────────────────────────────────
#define SYSINFO_CARD_X     20
#define SYSINFO_CARD_W     760
#define SYSINFO_TEXT_X     42
#define SYSINFO_VAL_COL    180
#define SYSINFO_BAR_X      180
#define SYSINFO_BAR_W      500
#define SYSINFO_CARD1_Y    62
#define SYSINFO_CARD1_H    72
#define SYSINFO_CARD2_Y    158
#define SYSINFO_CARD2_H    126
#define SYSINFO_CARD3_Y    306
#define SYSINFO_CARD3_H    60
#define SYSINFO_MEM_TITLE_Y   174
#define SYSINFO_HEAP_KEY_Y    208
#define SYSINFO_HEAP_BAR_Y    211
#define SYSINFO_PSRAM_KEY_Y   238
#define SYSINFO_PSRAM_BAR_Y   241
#define SYSINFO_SYS_ROW_Y     330
#define SYSINFO_CORE_KEY_X    330
#define SYSINFO_CORE_VAL_X    480
#define SYSINFO_HEAP_VAL_X    688
#define SYSINFO_FOOTER_Y      414
#define SYSINFO_FOOTER_H      50
#define SYSINFO_FOOT_BTN_W    180

// ───────────────────────────────────────────────────────────────────────────────
// CALIBRATION (SCREEN_CALIBRATION) — readable + touch-friendly (800x480 budget)
// ───────────────────────────────────────────────────────────────────────────────
#define CAL_CARD_LEFT      20
#define CAL_TOP_Y          53
#define CAL_TOP_H          60
#define CAL_TOP_W_A        220
#define CAL_TOP_W_B        220
#define CAL_TOP_W_C        286
#define CAL_TOP_GAP        15
#define CAL_PM_BTN_W       52
#define CAL_PM_BTN_H       38
#define CAL_FACTOR_PM_Y    (CAL_TOP_H - CAL_PM_BTN_H - 10)
#define CAL_FACTOR_PM_MINUS_X 96
#define CAL_FACTOR_PM_PLUS_X  160
#define CAL_BLOCK_GAP      6
#define CAL_WIZ_Y          (CAL_TOP_Y + CAL_TOP_H + CAL_BLOCK_GAP)
#define CAL_WIZ_W          456
#define CAL_WIZ_H          138
#define CAL_WIZ_TRACK_X    28
#define CAL_WIZ_TRACK_W    400
#define CAL_RIGHT_X        (CAL_CARD_LEFT + CAL_WIZ_W + 14)
#define CAL_RIGHT_W        286
#define CAL_WIZ_PAD        14
#define CAL_INFO_Y         (CAL_WIZ_Y + CAL_WIZ_H + CAL_BLOCK_GAP)
#define CAL_INFO_H         56
#define CAL_MEAS_FIELD_Y   17
#define CAL_MEAS_FIELD_W   216
#define CAL_MEAS_FIELD_H   34
#define CAL_APPLY_MEAS_H   34
#define CAL_ACTION_GAP     11
#define CAL_ACTION_Y       (CAL_INFO_Y + CAL_INFO_H + CAL_ACTION_GAP)
#define CAL_ACTION_BTN_H   40
#define CAL_ACTION_MOVE_W  204
#define CAL_ACTION_STOP_W  210
#define CAL_JOG_BTN_W      144
#define CAL_JOG_GAP        14
#define CAL_RESULT_TOPGAP  4
#define CAL_RESULT_Y       (CAL_ACTION_Y + CAL_ACTION_BTN_H + CAL_RESULT_TOPGAP)
#define CAL_RESULT_H       56
#define CAL_RESULT_GAP     2
#define CAL_FOOT_Y         (CAL_RESULT_Y + CAL_RESULT_H + CAL_RESULT_GAP)
#define CAL_FOOT_H         38

// ───────────────────────────────────────────────────────────────────────────────
// FONT SIZES — Montserrat (closest match to Courier New monospace)
// ───────────────────────────────────────────────────────────────────────────────
#define FONT_TINY           &lv_font_montserrat_12
#define FONT_SMALL          &lv_font_montserrat_12
#define FONT_NORMAL         &lv_font_montserrat_14
#define FONT_BODY           &lv_font_montserrat_12
#define FONT_MED            &lv_font_montserrat_14
#define FONT_SUBTITLE       &lv_font_montserrat_16
#define FONT_BTN            &lv_font_montserrat_18
#define FONT_LARGE          &lv_font_montserrat_20
#define FONT_XL             &lv_font_montserrat_24
#define FONT_XXL            &lv_font_montserrat_28
#define FONT_HUGE           &lv_font_montserrat_40
