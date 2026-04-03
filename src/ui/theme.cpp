// Theme - Color palette and style constants for UI
#include "theme.h"
#include "screens.h"
#include "../storage/storage.h"

lv_color_t g_accent;
lv_color_t g_accent_dim;
lv_color_t g_accent_border;

typedef struct {
    uint32_t accent;
    uint32_t dim;
    const char* name;
} ThemeEntry;

static const ThemeEntry theme_palette[] = {
    { 0xFF9500, 0x1A1600, "ORANGE" },
    { 0x00BCD4, 0x001A1E, "CYAN" },
    { 0x00E676, 0x001A0E, "GREEN" },
    { 0x448AFF, 0x0A1230, "BLUE" },
    { 0xFF5252, 0x1A0A0A, "RED" },
    { 0xBB86FC, 0x140E1A, "PURPLE" },
    { 0xFFD600, 0x1A1A00, "YELLOW" },
    { 0xE0E0E0, 0x141414, "WHITE" },
};

#define THEME_COUNT (sizeof(theme_palette) / sizeof(theme_palette[0]))

void theme_init() {
    uint8_t idx = g_settings.accent_color;
    if (idx >= THEME_COUNT) idx = 0;
    g_accent = lv_color_hex(theme_palette[idx].accent);
    g_accent_dim = lv_color_hex(theme_palette[idx].dim);
    g_accent_border = lv_color_hex(theme_palette[idx].accent);

    lv_theme_t* th = lv_theme_default_init(
        lv_display_get_default(), g_accent, g_accent_dim, true, &lv_font_montserrat_14
    );
    if (th) {
        lv_theme_set_apply_cb(th, nullptr);
    }
}

void theme_set_color(uint8_t idx) {
    if (idx >= THEME_COUNT) idx = 0;
    g_settings.accent_color = idx;
    g_accent = lv_color_hex(theme_palette[idx].accent);
    g_accent_dim = lv_color_hex(theme_palette[idx].dim);
    g_accent_border = lv_color_hex(theme_palette[idx].accent);
}

const char* theme_get_name(uint8_t idx) {
    if (idx >= THEME_COUNT) idx = 0;
    return theme_palette[idx].name;
}

uint8_t theme_get_count() {
  return (uint8_t)THEME_COUNT;
}

void theme_refresh() {
  screens_reinit();
}
