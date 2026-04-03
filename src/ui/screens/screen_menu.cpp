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

  // ── Header bar (0,0,800,32) ──
  lv_obj_t* header = lv_obj_create(screen);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  // Title
  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "MENU");
  lv_obj_set_style_text_font(title, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 7);

  // ── Separator line at y=32 ──
  lv_obj_t* line = lv_obj_create(screen);
  lv_obj_set_size(line, SCREEN_W, 1);
  lv_obj_set_pos(line, 0, HEADER_H);
  lv_obj_set_style_bg_color(line, COL_BORDER, 0);
  lv_obj_set_style_pad_all(line, 0, 0);
  lv_obj_set_style_border_width(line, 0, 0);
  lv_obj_set_style_radius(line, 0, 0);
  lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);

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

  // ── Separator ──
  const int sepY = progY + 60;
  lv_obj_t* sep = lv_obj_create(screen);
  lv_obj_set_size(sep, SCREEN_W, 1);
  lv_obj_set_pos(sep, 0, sepY);
  lv_obj_set_style_bg_color(sep, COL_BORDER, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_radius(sep, 0, 0);
  lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

  // ── Bottom row: BACK + SETTINGS (glove-friendly 380x56) ──
  const int bottomY = sepY + 10;
  const int bottomBtnW = (SCREEN_W - PAD_X * 2 - gridGapX) / 2;
  const int bottomBtnH = 56;

  // BACK button (left)
  lv_obj_t* backBtn = lv_button_create(screen);
  lv_obj_set_size(backBtn, bottomBtnW, bottomBtnH);
  lv_obj_set_pos(backBtn, PAD_X, bottomY);
  lv_obj_set_style_bg_color(backBtn, COL_BTN_BG, 0);
  lv_obj_set_style_radius(backBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(backBtn, 1, 0);
  lv_obj_set_style_border_color(backBtn, COL_BORDER, 0);
  lv_obj_set_style_shadow_width(backBtn, 0, 0);
  lv_obj_set_style_pad_all(backBtn, 0, 0);
  lv_obj_add_event_cb(backBtn, back_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* backLabel = lv_label_create(backBtn);
  lv_label_set_text(backLabel, "<  BACK");
  lv_obj_set_style_text_font(backLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(backLabel, COL_TEXT, 0);
  lv_obj_center(backLabel);

  // SETTINGS button (right, active style)
  lv_obj_t* settingsBtn = lv_button_create(screen);
  lv_obj_set_size(settingsBtn, bottomBtnW, bottomBtnH);
  lv_obj_set_pos(settingsBtn, PAD_X + bottomBtnW + gridGapX, bottomY);
  lv_obj_set_style_bg_color(settingsBtn, COL_BG_ACTIVE, 0);
  lv_obj_set_style_radius(settingsBtn, RADIUS_BTN, 0);
  lv_obj_set_style_border_width(settingsBtn, 2, 0);
  lv_obj_set_style_border_color(settingsBtn, COL_ACCENT, 0);
  lv_obj_set_style_shadow_width(settingsBtn, 0, 0);
  lv_obj_set_style_pad_all(settingsBtn, 0, 0);
  lv_obj_add_event_cb(settingsBtn, settings_event_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* settingsLabel = lv_label_create(settingsBtn);
  lv_label_set_text(settingsLabel, "SETTINGS");
  lv_obj_set_style_text_font(settingsLabel, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(settingsLabel, COL_ACCENT, 0);
  lv_obj_center(settingsLabel);

  LOG_I("Screen menu: v2.0 layout created");
}
