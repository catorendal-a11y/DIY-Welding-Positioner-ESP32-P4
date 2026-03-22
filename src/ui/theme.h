// TIG Rotator Controller - UI Theme
// Pixel-perfect match to ui_mockup.svg
// ESP32-P4 4.3" Touch Display: 800×480 landscape

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// BACKGROUND COLORS (from SVG)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x050505)   // SVG: #050505 main background
#define COL_BG_CARD     lv_color_hex(0x121212)   // SVG: #121212 inactive button fill
#define COL_BG_INPUT    lv_color_hex(0x1A1A1A)   // Input fields
#define COL_BG_ACTIVE   lv_color_hex(0x0C1A1C)   // SVG: #0c1a1c active button fill (dark teal)

// ───────────────────────────────────────────────────────────────────────────────
// ACCENT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_ACCENT      lv_color_hex(0x00E5FF)   // SVG: #00E5FF neon cyan - gauge arc, active borders
#define COL_ACCENT_DIM  lv_color_hex(0x005566)   // Dimmed accent

// ───────────────────────────────────────────────────────────────────────────────
// STATUS COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_GREEN       lv_color_hex(0x00E676)   // SVG: #00E676 "RUNNING" badge
#define COL_GREEN_DARK  lv_color_hex(0x004D26)
#define COL_RED         lv_color_hex(0xFF1744)   // ESTOP
#define COL_RED_DARK    lv_color_hex(0x5E0D1B)
#define COL_AMBER       lv_color_hex(0xFF9100)   // Warning / JOG
#define COL_PURPLE      lv_color_hex(0xD500F9)   // Pulse mode
#define COL_TEAL        lv_color_hex(0x1DE9B6)   // Step mode

// ───────────────────────────────────────────────────────────────────────────────
// TEXT COLORS (from SVG)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_TEXT        lv_color_hex(0xFFFFFF)   // SVG: #FFF primary
#define COL_TEXT_DIM    lv_color_hex(0x555555)   // SVG: #555 inactive button text
#define COL_TEXT_LABEL  lv_color_hex(0x444444)   // SVG: #444 "TIG ROTATOR" title, scale labels
#define COL_TEXT_SCALE  lv_color_hex(0x555555)   // SVG: #555 gauge scale numbers

// ───────────────────────────────────────────────────────────────────────────────
// UI ELEMENT COLORS (from SVG)
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BORDER      lv_color_hex(0x222222)   // SVG: #222 inactive button stroke
#define COL_BORDER_SPD  lv_color_hex(0x333333)   // SVG: #333 speed button border
#define COL_BTN_NORMAL  lv_color_hex(0x121212)   // SVG: #121212 inactive button bg
#define COL_BTN_PRESS   lv_color_hex(0x2A2A2A)   // Button pressed bg
#define COL_GAUGE_TRACK lv_color_hex(0x111111)   // SVG: #111 gauge background arc
#define COL_GAUGE_TICK  lv_color_hex(0x222222)   // SVG: #222 gauge tick marks

// ───────────────────────────────────────────────────────────────────────────────
// SIZING CONSTANTS (pixel-matched to ui_mockup.svg)
// ───────────────────────────────────────────────────────────────────────────────

// Side panel buttons (SVG: width=130 height=75 rx=12)
#define BTN_W           130
#define BTN_H           75
#define BTN_H_SM        55    // Speed buttons (SVG: height=55)
#define BTN_W_SM        95    // Speed buttons (SVG: width=95)
#define RADIUS_BTN      12    // SVG: rx=12
#define RADIUS_BTN_SM   10    // SVG: rx=10 for speed buttons
#define RADIUS_CARD     12
#define PAD_SCREEN      16

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN COORDINATES (800 × 480 landscape)
// ───────────────────────────────────────────────────────────────────────────────
#define SCREEN_W        800
#define SCREEN_H        480

// Status bar (SVG: top row at y=35)
#define STATUS_BAR_H    45
#define STATUS_BAR_Y    0

// Side button absolute positions from SVG
#define LEFT_BTN_X      35     // SVG: translate(35, ...)
#define RIGHT_BTN_X     635    // SVG: translate(635, ...)
#define BTN_ROW_1_Y     120    // SVG: first button row
#define BTN_ROW_2_Y     210    // SVG: second button row
#define BTN_ROW_3_Y     300    // SVG: third button row

// Speed buttons (SVG: y=400)
#define SPD_BTN_Y       400
#define SPD_BTN_L_X     295    // SVG: translate(295, 400)
#define SPD_BTN_R_X     410    // SVG: translate(410, 400)

// Gauge center (SVG: centered at 400, 240 with radius 160)
#define GAUGE_CX        400
#define GAUGE_CY        240
#define GAUGE_R         160

// Main UI Layout Areas
#define PANEL_SIDE_W    150
#define CENTER_W        (SCREEN_W - (2 * PANEL_SIDE_W))
