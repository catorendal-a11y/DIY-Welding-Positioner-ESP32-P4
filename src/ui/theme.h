#pragma once
#include "lvgl.h"

#define SCREEN_W  800
#define SCREEN_H  480

#define SVG_W     360
#define SVG_H     250

#define SX(x)  ((int16_t)((x) * SCREEN_W / SVG_W))
#define SY(y)  ((int16_t)((y) * SCREEN_H / SVG_H))
#define SW(w)  ((int16_t)((w) * SCREEN_W / SVG_W))
#define SH(h)  ((int16_t)((h) * SCREEN_H / SVG_H))

#define COL_BG          lv_color_hex(0x000000)
#define COL_HEADER_BG   lv_color_hex(0x0A0A0A)
#define COL_BTN_FILL    lv_color_hex(0x1A1A1C)
#define COL_BTN_ACTIVE  lv_color_hex(0x0C1A1C)
#define COL_PANEL_BG    lv_color_hex(0x1A1A1C)
#define COL_ACCENT      lv_color_hex(0x00E5FF)
#define COL_GREEN       lv_color_hex(0x00E676)
#define COL_RED         lv_color_hex(0xFF1744)
#define COL_TEXT        lv_color_hex(0xFFFFFF)
#define COL_TEXT_DIM    lv_color_hex(0x555555)
#define COL_BORDER      lv_color_hex(0x888888)
#define COL_SEPARATOR   lv_color_hex(0x1A1A1A)
#define COL_GAUGE_BG    lv_color_hex(0x1A1A1A)
#define COL_ESTOP_BG    lv_color_hex(0x1A0000)
#define COL_ESTOP_HDR   lv_color_hex(0x200000)
#define COL_ESTOP_SEP   lv_color_hex(0x330000)

#define COL_BG_CARD     COL_BTN_FILL
#define COL_BG_INPUT    COL_BTN_FILL
#define COL_BG_ACTIVE   COL_BTN_ACTIVE
#define COL_ACCENT_DIM  lv_color_hex(0x005566)
#define COL_ACCENT_DARK lv_color_hex(0x003344)
#define COL_GREEN_DARK  lv_color_hex(0x004D26)
#define COL_RED_DARK    lv_color_hex(0x5E0D1B)
#define COL_AMBER       lv_color_hex(0xFF9100)
#define COL_PURPLE      lv_color_hex(0xD500F9)
#define COL_TEAL        lv_color_hex(0x1DE9B6)
#define COL_TEXT_LABEL  COL_TEXT_DIM
#define COL_TEXT_SCALE  COL_TEXT_DIM
#define COL_BORDER_SPD  COL_BORDER
#define COL_BTN_NORMAL  COL_BTN_FILL
#define COL_BTN_PRESS   lv_color_hex(0x2A2A2C)
#define COL_GAUGE_TRACK COL_GAUGE_BG
#define COL_GAUGE_TICK  COL_GAUGE_BG

#define FONT_TINY    &lv_font_montserrat_10
#define FONT_SMALL   &lv_font_montserrat_14
#define FONT_NORMAL  &lv_font_montserrat_18
#define FONT_MEDIUM  &lv_font_montserrat_20
#define FONT_BODY    &lv_font_montserrat_22
#define FONT_SUB     &lv_font_montserrat_24
#define FONT_LARGE   &lv_font_montserrat_28
#define FONT_XL      &lv_font_montserrat_32
#define FONT_XXL     &lv_font_montserrat_40
#define FONT_HUGE    &lv_font_montserrat_48

#define HEADER_H    61
#define SEP_H       2
#define RADIUS_SM   13
#define RADIUS_MD   18
#define RADIUS_LG   22

#define BTN_W           SX(60)
#define BTN_H           SH(36)
#define STATUS_BAR_H    HEADER_H
#define STATUS_BAR_Y    0
#define LEFT_BTN_X      SX(16)
#define RIGHT_BTN_X     SX(284)
#define BTN_ROW_1_Y     SY(44)
#define BTN_ROW_2_Y     SY(86)
#define BTN_ROW_3_Y     SY(128)
#define SPD_BTN_Y       SY(204)
#define SPD_BTN_L_X     SX(120)
#define SPD_BTN_R_X     SX(190)
#define GAUGE_CX        SX(180)
#define GAUGE_CY        SY(146)
#define GAUGE_R         SX(72)
#define PANEL_SIDE_W    SX(60)
#define CENTER_W        (SCREEN_W - (2 * PANEL_SIDE_W))
