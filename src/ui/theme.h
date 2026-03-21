// TIG Rotator Controller - UI Theme
// Dark industrial theme with glove-safe buttons
// ESP32-P4 4.3" Touch Display: 800×480 landscape

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// BACKGROUND COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x0B1929)   // Deep navy - main background
#define COL_BG_CARD     lv_color_hex(0x0D2137)   // Card background - panels
#define COL_BG_INPUT    lv_color_hex(0x071523)   // Input fields

// ───────────────────────────────────────────────────────────────────────────────
// ACCENT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_ACCENT      lv_color_hex(0x29B6F6)   // Cyan - titles, highlights
#define COL_ACCENT_DIM  lv_color_hex(0x1A6EA8)   // Dimmed accent

// ───────────────────────────────────────────────────────────────────────────────
// STATUS COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_GREEN       lv_color_hex(0x43A047)   // Running / OK
#define COL_GREEN_DARK  lv_color_hex(0x1B5E20)   // Start button
#define COL_RED         lv_color_hex(0xC62828)   // ESTOP / Stop
#define COL_AMBER       lv_color_hex(0xF57C00)   // Warning / JOG
#define COL_PURPLE      lv_color_hex(0x7B1FA2)   // Pulse mode
#define COL_TEAL        lv_color_hex(0x00897B)   // Step mode

// ───────────────────────────────────────────────────────────────────────────────
// TEXT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_TEXT        lv_color_hex(0xE8F4FD)   // Primary - near white
#define COL_TEXT_DIM    lv_color_hex(0x78909C)   // Muted secondary
#define COL_TEXT_LABEL  lv_color_hex(0x90CAF9)   // Label - light blue

// ───────────────────────────────────────────────────────────────────────────────
// UI ELEMENT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BORDER      lv_color_hex(0x1E3A5F)   // Panel borders
#define COL_BTN_NORMAL  lv_color_hex(0x132840)   // Button default bg
#define COL_BTN_PRESS   lv_color_hex(0x1E4D7A)   // Button pressed

// ───────────────────────────────────────────────────────────────────────────────
// SIZING CONSTANTS (scaled for 800×480)
// ───────────────────────────────────────────────────────────────────────────────
#define BTN_H           70    // Min button height (glove-safe, scaled up)
#define BTN_H_SM        54    // Secondary button height
#define RADIUS_BTN       8    // Button corner radius
#define RADIUS_CARD     12    // Card corner radius
#define PAD_SCREEN      16    // Screen edge padding

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN COORDINATES (800 × 480 landscape)
// ───────────────────────────────────────────────────────────────────────────────
#define SCREEN_W        800
#define SCREEN_H        480

// Status bar
#define STATUS_BAR_H    40
#define STATUS_BAR_Y    0

// Bottom nav
#define NAV_BAR_H       52
#define NAV_BAR_Y       (SCREEN_H - NAV_BAR_H)
