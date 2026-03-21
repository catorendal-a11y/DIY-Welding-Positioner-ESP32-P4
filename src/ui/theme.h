// TIG Rotator Controller - UI Theme
// Modern industrial dark theme, cyan accents, glove-safe
// ESP32-P4 4.3" Touch Display: 800×480 landscape

#pragma once
#include "lvgl.h"

// ───────────────────────────────────────────────────────────────────────────────
// BACKGROUND COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BG          lv_color_hex(0x050505)   // Deep black/grey - main background
#define COL_BG_CARD     lv_color_hex(0x121212)   // Card background - panels
#define COL_BG_INPUT    lv_color_hex(0x1A1A1A)   // Input fields

// ───────────────────────────────────────────────────────────────────────────────
// ACCENT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_ACCENT      lv_color_hex(0x00E5FF)   // Neon Cyan - highlights, glows, active state
#define COL_ACCENT_DIM  lv_color_hex(0x005566)   // Dimmed accent for borders

// ───────────────────────────────────────────────────────────────────────────────
// STATUS COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_GREEN       lv_color_hex(0x00E676)   // Running / OK / Start
#define COL_GREEN_DARK  lv_color_hex(0x004D26)   // Start button rest state
#define COL_RED         lv_color_hex(0xFF1744)   // ESTOP / Stop
#define COL_RED_DARK    lv_color_hex(0x5E0D1B)   // Stop button rest state
#define COL_AMBER       lv_color_hex(0xFF9100)   // Warning / JOG
#define COL_PURPLE      lv_color_hex(0xD500F9)   // Pulse mode
#define COL_TEAL        lv_color_hex(0x1DE9B6)   // Step mode

// ───────────────────────────────────────────────────────────────────────────────
// TEXT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_TEXT        lv_color_hex(0xFFFFFF)   // Primary - crisp white
#define COL_TEXT_DIM    lv_color_hex(0x8A8A8A)   // Muted secondary (grey)
#define COL_TEXT_LABEL  lv_color_hex(0xB0BEC5)   // Label - light grey

// ───────────────────────────────────────────────────────────────────────────────
// UI ELEMENT COLORS
// ───────────────────────────────────────────────────────────────────────────────
#define COL_BORDER      lv_color_hex(0x333333)   // Panel/button borders
#define COL_BTN_NORMAL  lv_color_hex(0x181818)   // Button default bg
#define COL_BTN_PRESS   lv_color_hex(0x2A2A2A)   // Button pressed bg

// ───────────────────────────────────────────────────────────────────────────────
// SIZING CONSTANTS (scaled for 800×480)
// ───────────────────────────────────────────────────────────────────────────────
#define BTN_W           130   // Standard side panel button width
#define BTN_H           80    // Min button height (glove-safe)
#define BTN_H_SM        60    // Secondary button height
#define RADIUS_BTN       6    // Button corner radius (slightly square)
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

// Main UI Layout Areas
#define PANEL_SIDE_W    150   // Width of the left & right side panels
#define CENTER_W        (SCREEN_W - (2 * PANEL_SIDE_W)) // 500px for central gauge
