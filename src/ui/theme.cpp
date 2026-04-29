// Theme - Color palette and style constants for UI
#include "theme.h"
#include "screens.h"
#include "../storage/storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

lv_color_t g_accent;
lv_color_t g_accent_dim;
lv_color_t g_accent_border;

lv_color_t g_col_bg;
lv_color_t g_col_bg_header;
lv_color_t g_col_bg_card;
lv_color_t g_col_bg_card_alt;
lv_color_t g_col_bg_dim;
lv_color_t g_col_bg_row;
lv_color_t g_col_bg_empty;
lv_color_t g_col_btn_bg;
lv_color_t g_col_bg_danger;
lv_color_t g_col_panel_bg;
lv_color_t g_col_separator;
lv_color_t g_col_bg_input;
lv_color_t g_col_gauge_bg;
lv_color_t g_col_toggle_off;
lv_color_t g_col_bg_ok;
lv_color_t g_col_border_ok;
lv_color_t g_col_bg_warn_panel;
lv_color_t g_col_border_warn;
lv_color_t g_col_warn;
lv_color_t g_col_yellow;
lv_color_t g_col_text;
lv_color_t g_col_text_bright;
lv_color_t g_col_text_white;
lv_color_t g_col_text_dim;
lv_color_t g_col_text_vdim;
lv_color_t g_col_text_title;
lv_color_t g_col_border;
lv_color_t g_col_border_sm;
lv_color_t g_col_border_row;
lv_color_t g_col_border_dng;
lv_color_t g_col_gauge_tick;
lv_color_t g_col_gauge_minor;
lv_color_t g_col_slider_track;
lv_color_t g_col_slider_track2;
lv_color_t g_col_slider_track3;
lv_color_t g_col_slider_border;
lv_color_t g_col_slider_border2;
lv_color_t g_col_knob_border;
lv_color_t g_col_progress_bg;
lv_color_t g_col_protractor_bg;

typedef struct {
  uint32_t accent;
  uint32_t dim_dark;
  uint32_t dim_light;
  const char* name;
} ThemeEntry;

typedef struct {
  uint32_t bg;
  uint32_t bg_header;
  uint32_t bg_card;
  uint32_t bg_card_alt;
  uint32_t bg_dim;
  uint32_t bg_row;
  uint32_t bg_empty;
  uint32_t btn_bg;
  uint32_t bg_danger;
  uint32_t panel_bg;
  uint32_t separator;
  uint32_t bg_input;
  uint32_t gauge_bg;
  uint32_t toggle_off;
  uint32_t bg_ok;
  uint32_t border_ok;
  uint32_t bg_warn_panel;
  uint32_t border_warn;
  uint32_t warn;
  uint32_t yellow;
  uint32_t text;
  uint32_t text_bright;
  uint32_t text_white;
  uint32_t text_dim;
  uint32_t text_vdim;
  uint32_t text_title;
  uint32_t border;
  uint32_t border_sm;
  uint32_t border_row;
  uint32_t border_dng;
  uint32_t gauge_tick;
  uint32_t gauge_minor;
  uint32_t slider_track;
  uint32_t slider_track2;
  uint32_t slider_track3;
  uint32_t slider_border;
  uint32_t slider_border2;
  uint32_t knob_border;
  uint32_t progress_bg;
  uint32_t protractor_bg;
} NeutralPack;

// Dark: POST industrial (matches prior theme.h hex constants)
static const NeutralPack NEUT_DARK = {
    0x050505, 0x090909, 0x0B0B0B, 0x0B0B0B, 0x080808, 0x080808, 0x1A1A1A, 0x0B0B0B,
    0x1A0A0A, 0x141414, 0x1A1A1A, 0x0A0A0A, 0x111111, 0x333333, 0x062616, 0x00C853,
    0x241B00, 0xFFAA00, 0xFF9500, 0xFFAA00, 0xAAAAAA, 0xCCCCCC, 0xD0D0D0, 0x666666,
    0x555555, 0x777777, 0x2E2E2E, 0x333333, 0x1E1E1E, 0xFF1744, 0x444444, 0x2A2A2A,
    0x1A1A1A, 0x3A3A3A, 0x444444, 0x555555, 0x666666, 0xFFFFFF, 0x222222, 0x0A0A0A,
};

// Light: warm cream HMI (reference: main bg #F9F7F2, header bar #1A1A1A, subtle borders)
static const NeutralPack NEUT_LIGHT = {
    0xF9F7F2, 0x1A1A1A, 0xF7F5F0, 0xF4F1EA, 0xEEEAE4, 0xEEEAE4, 0xD9D5CD, 0xF5F3EE,
    0xFFEBEE, 0xEFEBE4, 0xD8D4CC, 0xFBFAF8, 0xE8E4DD, 0xBDB9B3, 0xE8F5E9, 0x00C853,
    0xFFF8E1, 0xFF9800, 0xE65100, 0xF57F17, 0x37474F, 0x263238, 0x212121, 0x607D8B,
    0x90A4AE, 0x455A64, 0xC4C0B8, 0xB0ACA5, 0xD8D4CC, 0xFF1744, 0x78909E, 0xB0BEC5,
    0xE3DFDB, 0xCFD8DC, 0xB0BEC5, 0x90A4AE, 0x78909C, 0x455A64, 0xE8E4DD, 0xF3F1EC,
};

static const ThemeEntry theme_palette[] = {
    { 0xFF9500, 0x1A1600, 0xFFF0E0, "ORANGE" },
    { 0x00BCD4, 0x001A1E, 0xE0F7FA, "CYAN" },
    { 0x00E676, 0x001A0E, 0xE8F5E9, "GREEN" },
    { 0x448AFF, 0x0A1230, 0xE3F2FD, "BLUE" },
    { 0xFF5252, 0x1A0A0A, 0xFFEBEE, "RED" },
    { 0xBB86FC, 0x140E1A, 0xF3E5F5, "PURPLE" },
    { 0xFFD600, 0x1A1A00, 0xFFFDE7, "YELLOW" },
    { 0xE0E0E0, 0x141414, 0xF5F5F5, "WHITE" },
};

#define THEME_COUNT (sizeof(theme_palette) / sizeof(theme_palette[0]))

static void apply_neutral_pack(const NeutralPack* p) {
  g_col_bg = lv_color_hex(p->bg);
  g_col_bg_header = lv_color_hex(p->bg_header);
  g_col_bg_card = lv_color_hex(p->bg_card);
  g_col_bg_card_alt = lv_color_hex(p->bg_card_alt);
  g_col_bg_dim = lv_color_hex(p->bg_dim);
  g_col_bg_row = lv_color_hex(p->bg_row);
  g_col_bg_empty = lv_color_hex(p->bg_empty);
  g_col_btn_bg = lv_color_hex(p->btn_bg);
  g_col_bg_danger = lv_color_hex(p->bg_danger);
  g_col_panel_bg = lv_color_hex(p->panel_bg);
  g_col_separator = lv_color_hex(p->separator);
  g_col_bg_input = lv_color_hex(p->bg_input);
  g_col_gauge_bg = lv_color_hex(p->gauge_bg);
  g_col_toggle_off = lv_color_hex(p->toggle_off);
  g_col_bg_ok = lv_color_hex(p->bg_ok);
  g_col_border_ok = lv_color_hex(p->border_ok);
  g_col_bg_warn_panel = lv_color_hex(p->bg_warn_panel);
  g_col_border_warn = lv_color_hex(p->border_warn);
  g_col_warn = lv_color_hex(p->warn);
  g_col_yellow = lv_color_hex(p->yellow);
  g_col_text = lv_color_hex(p->text);
  g_col_text_bright = lv_color_hex(p->text_bright);
  g_col_text_white = lv_color_hex(p->text_white);
  g_col_text_dim = lv_color_hex(p->text_dim);
  g_col_text_vdim = lv_color_hex(p->text_vdim);
  g_col_text_title = lv_color_hex(p->text_title);
  g_col_border = lv_color_hex(p->border);
  g_col_border_sm = lv_color_hex(p->border_sm);
  g_col_border_row = lv_color_hex(p->border_row);
  g_col_border_dng = lv_color_hex(p->border_dng);
  g_col_gauge_tick = lv_color_hex(p->gauge_tick);
  g_col_gauge_minor = lv_color_hex(p->gauge_minor);
  g_col_slider_track = lv_color_hex(p->slider_track);
  g_col_slider_track2 = lv_color_hex(p->slider_track2);
  g_col_slider_track3 = lv_color_hex(p->slider_track3);
  g_col_slider_border = lv_color_hex(p->slider_border);
  g_col_slider_border2 = lv_color_hex(p->slider_border2);
  g_col_knob_border = lv_color_hex(p->knob_border);
  g_col_progress_bg = lv_color_hex(p->progress_bg);
  g_col_protractor_bg = lv_color_hex(p->protractor_bg);
}

static void theme_sync_colors(void) {
  bool light = false;
  uint8_t idx = 0;
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  light = (g_settings.color_scheme != 0);
  idx = g_settings.accent_color;
  xSemaphoreGive(g_settings_mutex);

  if (idx >= THEME_COUNT) {
    idx = 0;
  }

  apply_neutral_pack(light ? &NEUT_LIGHT : &NEUT_DARK);

  g_accent = lv_color_hex(theme_palette[idx].accent);
  g_accent_dim = lv_color_hex(light ? theme_palette[idx].dim_light : theme_palette[idx].dim_dark);
  g_accent_border = lv_color_hex(theme_palette[idx].accent);

  lv_display_t* disp = lv_display_get_default();
  if (!disp) {
    return;
  }
  lv_theme_t* th = lv_theme_default_init(
      disp, g_accent, g_accent_dim, !light, &lv_font_montserrat_14);
  if (th) {
    lv_display_set_theme(disp, th);
  }
}

void theme_init() {
  theme_sync_colors();
}

void theme_set_color(uint8_t idx) {
  if (idx >= THEME_COUNT) {
    idx = 0;
  }
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.accent_color = idx;
  xSemaphoreGive(g_settings_mutex);
  theme_sync_colors();
}

void theme_set_scheme(uint8_t scheme) {
  if (scheme > 1) {
    scheme = 0;
  }
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.color_scheme = scheme;
  xSemaphoreGive(g_settings_mutex);
  theme_sync_colors();
}

const char* theme_get_name(uint8_t idx) {
  if (idx >= THEME_COUNT) {
    idx = 0;
  }
  return theme_palette[idx].name;
}

uint8_t theme_get_count() {
  return (uint8_t)THEME_COUNT;
}

const char* theme_get_scheme_name(uint8_t scheme) {
  return (scheme != 0) ? "LIGHT" : "DARK";
}

uint8_t theme_get_scheme_count(void) {
  return 2;
}

void theme_refresh() {
  screens_reinit();
}
