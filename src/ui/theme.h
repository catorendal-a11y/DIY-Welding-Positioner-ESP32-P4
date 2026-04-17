// TIG Rotator Controller - UI Theme
// Brutalist v2.0 design matching new_ui.svg
// ESP32-P4 4.3" Touch Display: 800x480 landscape

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// BACKGROUND COLORS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x050505)   // SVG: #050505 screen background
#define COL_BG_HEADER   lv_color_hex(0x090909)   // SVG: #090909 header bar (.hd)
#define COL_BG_CARD     lv_color_hex(0x0D0D0D)   // SVG: #0D0D0D card background
#define COL_BG_CARD_ALT lv_color_hex(0x0B0B0B)   // Alt card (even rows in lists)
#define COL_BG_DIM      lv_color_hex(0x080808)   // SVG: #080808 dim area (.dim)
#define COL_BG_ROW      lv_color_hex(0x0E0E0E)   // SVG: #0E0E0E setting rows
#define COL_BG_EMPTY    lv_color_hex(0x1A1A1A)   // Empty-slot placeholder rows / neutral dark fill
#define COL_BTN_BG      lv_color_hex(0x141414)   // SVG: #141414 button fill (.b, .sm)
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
#define RADIUS_BTN      2      // Standard button radius (SVG: rx=2)
#define RADIUS_BTN_SM   2      // Small button radius
#define RADIUS_CARD     4      // Card/dialog radius (SVG: rx=4)
#define RADIUS_ROW      0      // Setting rows (no radius)

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN DIMENSIONS
// ───────────────────────────────────────────────────────────────────────────────
#define SCREEN_W        800
#define SCREEN_H        480

// ───────────────────────────────────────────────────────────────────────────────
// LAYOUT CONSTANTS (from new_ui.svg)
// ───────────────────────────────────────────────────────────────────────────────

// Header bar (SVG: height=30)
#define HEADER_H        30

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

// ───────────────────────────────────────────────────────────────────────────────
// MAIN SCREEN CONSTANTS (SCREEN_MAIN) — pot-only RPM: no +/- buttons, larger gauge
// ───────────────────────────────────────────────────────────────────────────────
#define MAIN_GAUGE_CX         400
// trackSize = 2*R+20; top = CY - trackSize/2 must stay on-screen (>= ~8 below header)
#define MAIN_GAUGE_CY         205
#define MAIN_GAUGE_R          178
#define MAIN_GAUGE_TRACK_PAD  12
#define MAIN_GAUGE_TRACK_W    14
#define MAIN_GAUGE_IND_W      10
// "RPM" unit row: TOP_MID on screen; value label re-aligned above with MAIN_RPM_VALUE_GAP
#define MAIN_RPM_TAG_Y        (MAIN_GAUGE_CY + 24)
#define MAIN_RPM_VALUE_GAP    6
// Extra upward shift for RPM digits (font already FONT_HUGE / 40pt max on P4)
#define MAIN_RPM_VALUE_LIFT   16
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
#define SET_HEADER_H       28
#define SET_HEADER_FONT    FONT_SUBTITLE
#define SET_ROW_H          48
#define SET_TOGGLE_W       80
#define SET_TOGGLE_H       40
#define SET_TOGGLE_R       12
#define SET_FOOTER_Y       440
#define SET_FOOTER_H       36
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
