// TIG Rotator Controller - Menu Screen
// Brutalist v2.0 design matching new_ui.svg: 2x2 mode grid + programs + bottom buttons

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// EVENT HANDLERS
// ───────────────────────────────────────────────────────────────────────────────
static void back_event_cb(lv_event_t* e) { screens_show(SCREEN_MAIN); }
static void pulse_event_cb(lv_event_t* e) { screens_show(SCREEN_PULSE); }
static void step_event_cb(lv_event_t* e) { screens_show(SCREEN_STEP); }
static void jog_event_cb(lv_event_t* e) { screens_show(SCREEN_JOG); }
static void timer_event_cb(lv_event_t* e) { screens_show(SCREEN_TIMER); }
static void settings_event_cb(lv_event_t* e) { screens_show(SCREEN_SETTINGS); }
static void programs_event_cb(lv_event_t* e) { screens_show(SCREEN_PROGRAMS); }

// ───────────────────────────────────────────────────────────────────────────────
// HELPER: create a menu grid button (glove-friendly)
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* create_grid_btn(lv_obj_t* parent, int16_t x, int16_t y,
                                  int16_t w, int16_t h, const char* text,
                                  lv_event_cb_t cb, bool active) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_bg_color(btn, active ? COL_BG_ACTIVE : COL_BTN_BG, 0);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(btn, active ? 2 : 1, 0);
  lv_obj_set_style_border_color(btn, active ? COL_ACCENT : COL_BORDER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_XL, 0);
  lv_obj_set_style_text_color(lbl, active ? COL_ACCENT : COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE — v2.0: Header + 2x2 grid + PROGRAMS + BACK/SETTINGS
// ───────────────────────────────────────────────────────────────────────────────
void screen_menu_create() {
  lv_obj_t* screen = screenRoots[SCREEN_MENU];
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, COL_BG, 0);

  ui_create_header(screen, "MENU", HEADER_H, FONT_NORMAL, 7);
  ui_create_separator(screen, HEADER_H);

  // ── 2x2 Mode Grid (glove-friendly, min 48x48) ──
  // Grid area: y=40 to y=280, buttons 380x110 each with 16px gap
  const int gridStartY = 40;
  const int gridBtnW = 380;
  const int gridBtnH = 110;
  const int gridGapX = 16;
  const int gridGapY = 16;
  const int gridStartX = (SCREEN_W - gridBtnW * 2 - gridGapX) / 2;  // = 12

  // Top-left: PULSE
  create_grid_btn(screen, gridStartX, gridStartY,
                  gridBtnW, gridBtnH, "PULSE", pulse_event_cb, false);

  // Top-right: STEP
  create_grid_btn(screen, gridStartX + gridBtnW + gridGapX, gridStartY,
                  gridBtnW, gridBtnH, "STEP", step_event_cb, false);

  // Bottom-left: JOG
  create_grid_btn(screen, gridStartX, gridStartY + gridBtnH + gridGapY,
                  gridBtnW, gridBtnH, "JOG", jog_event_cb, false);

  // Bottom-right: TIMER
  create_grid_btn(screen, gridStartX + gridBtnW + gridGapX, gridStartY + gridBtnH + gridGapY,
                  gridBtnW, gridBtnH, "TIMER", timer_event_cb, false);

  // ── PROGRAMS button (full-width, accent style) ──
  const int progY = gridStartY + (gridBtnH + gridGapY) * 2 + 8;
  lv_obj_t* programsBtn = lv_button_create(screen);
  lv_obj_set_size(programsBtn, SCREEN_W - 2 * PAD_X, 52);
  lv_obj_set_pos(programsBtn, PAD_X, progY);
  lv_obj_set_style_bg_color(programsBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(programsBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(programsBtn, 2, 0);
  lv_obj_set_style_border_color(programsBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(programsBtn, 0, 0);
  lv_obj_set_style_pad_all(programsBtn, 0, 0);
  lv_obj_add_event_cb(programsBtn, programs_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* programsLabel = lv_label_create(programsBtn);
  lv_label_set_text(programsLabel, "PROGRAMS");
  lv_obj_set_style_text_font(programsLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(programsLabel, COL_ACCENT, 0);
  lv_obj_center(programsLabel);

  const int sepY = progY + 60;
  ui_create_separator(screen, sepY);

  const int bottomY = sepY + 10;
  const int bottomBtnW = (SCREEN_W - PAD_X * 2 - gridGapX) / 2;
  const int bottomBtnH = 56;

  ui_create_btn(screen, PAD_X, bottomY, bottomBtnW, bottomBtnH, "<  BACK", FONT_SUBTITLE, false, false,
                back_event_cb, nullptr);
  ui_create_btn(screen, PAD_X + bottomBtnW + gridGapX, bottomY, bottomBtnW, bottomBtnH, "SETTINGS",
                FONT_SUBTITLE, true, false, settings_event_cb, nullptr);

  LOG_I("Screen menu: v2.0 layout created");
}
