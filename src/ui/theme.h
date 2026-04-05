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
#define COL_BG_DIM      lv_color_hex(0x080808)   // SVG: #080808 dim area (.dim)
#define COL_BG_ROW      lv_color_hex(0x0E0E0E)   // SVG: #0E0E0E setting rows
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
// MAIN SCREEN CONSTANTS (from new_ui.svg SCREEN_MAIN)
// ───────────────────────────────────────────────────────────────────────────────
// Gauge: semicircle from M220,370 to A190,190 with center ~400,270
#define GAUGE_CX        400
#define GAUGE_CY        270    // Approximate center of semicircle arc
#define GAUGE_R         190    // Arc radius

// RPM +/- buttons (SVG: x=330/404, y=388, 66x36)
#define RPM_BTN_W       66
#define RPM_BTN_H       36
#define RPM_BTN_Y       388
#define RPM_MINUS_X     330
#define RPM_PLUS_X      404

// Bottom button row (SVG: y=436, buttons 152x36 or similar)
#define BOTTOM_ROW_Y    436
#define BOTTOM_ROW_H    36

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
