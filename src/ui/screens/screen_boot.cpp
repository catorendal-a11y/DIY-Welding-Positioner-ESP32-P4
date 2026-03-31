// TIG Rotator Controller - Boot Screen
// Brutalist v2.0 terminal/console style - fills the full 800x480 screen

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* progressBar = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* percentLabel = nullptr;
static lv_obj_t* logLabels[8] = {};
static int currentProgress = 0;

// ───────────────────────────────────────────────────────────────────────────────
// BOOT LOG MESSAGES
// ───────────────────────────────────────────────────────────────────────────────
static const char* bootTags[] = {
    "SYS", "HW",  "MEM", "MTR",
    "ENC", "CFG", "UI",  "NET"
};

static const char* bootDescs[] = {
    "ESP32-P4 init",
    "GPIO scan",
    "PSRAM test",
    "Stepper driver",
    "Encoder check",
    "Config loaded",
    "LVGL started",
    "Ready"
};

// ───────────────────────────────────────────────────────────────────────────────
// PROGRESS UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_boot_set_progress(int percent, const char* message) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    currentProgress = percent;

    if (progressBar) lv_bar_set_value(progressBar, percent, LV_ANIM_ON);

    if (percentLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(percentLabel, buf);
    }

    if (statusLabel && message) lv_label_set_text(statusLabel, message);
}

void screen_boot_increment(int delta) {
    screen_boot_set_progress(currentProgress + delta, nullptr);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE - Full-screen terminal style (800x480)
// ───────────────────────────────────────────────────────────────────────────────
void screen_boot_create() {
    lv_obj_t* screen = screenRoots[SCREEN_BOOT];
    lv_obj_set_style_bg_color(screen, COL_BG, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // ── Title bar ──
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_set_size(header, SCREEN_W, 36);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "TIG-ROTATOR " FW_VERSION);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, COL_GREEN, 0);
    lv_obj_set_pos(title, PAD_X, 8);

    lv_obj_t* bootTag = lv_label_create(header);
    lv_label_set_text(bootTag, "BOOT");
    lv_obj_set_style_text_font(bootTag, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(bootTag, COL_TEXT_DIM, 0);
    lv_obj_set_pos(bootTag, SCREEN_W - 70, 10);

    // ── Terminal border (full width, y=40 to y=350) ──
    lv_obj_t* termBorder = lv_obj_create(screen);
    lv_obj_set_size(termBorder, SCREEN_W - 24, 305);
    lv_obj_set_pos(termBorder, 12, 40);
    lv_obj_set_style_bg_opa(termBorder, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(termBorder, COL_BORDER, 0);
    lv_obj_set_style_border_width(termBorder, 1, 0);
    lv_obj_set_style_radius(termBorder, 0, 0);
    lv_obj_set_style_pad_all(termBorder, 0, 0);
    lv_obj_remove_flag(termBorder, LV_OBJ_FLAG_SCROLLABLE);

    // ── 8 init log lines (y=50 to y=270, 32px spacing) ──
    for (int i = 0; i < 8; i++) {
        char recolorLine[80];
        snprintf(recolorLine, sizeof(recolorLine),
                 "[%s] %s ...... #00C853 OK#",
                 bootTags[i], bootDescs[i]);

        logLabels[i] = lv_label_create(screen);
        lv_label_set_text(logLabels[i], recolorLine);
        lv_obj_set_style_text_font(logLabels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(logLabels[i], lv_color_hex(0x3A3A3A), 0);
        lv_label_set_recolor(logLabels[i], true);
        lv_obj_set_pos(logLabels[i], 24, 48 + i * 32);
    }

    // ── Progress bar (wider: 600x10, y=310) ──
    progressBar = lv_bar_create(screen);
    lv_obj_set_size(progressBar, 600, 10);
    lv_obj_set_pos(progressBar, 24, 310);
    lv_bar_set_range(progressBar, 0, 100);
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_pad_all(progressBar, 0, 0);
    lv_obj_set_style_bg_color(progressBar, lv_color_hex(0x0A0A0A), LV_PART_MAIN);
    lv_obj_set_style_radius(progressBar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progressBar, 0, LV_PART_INDICATOR);

    // ── Status line "> SYSTEM READY" (y=326) ──
    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, "> SYSTEM READY");
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
    lv_obj_set_pos(statusLabel, 24, 326);

    // ── Percent label (right of progress bar) ──
    percentLabel = lv_label_create(screen);
    lv_label_set_text(percentLabel, "0%");
    lv_obj_set_style_text_font(percentLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(percentLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(percentLabel, 640, 306);

    // ── Separator line at y=352 ──
    lv_obj_t* sep = lv_obj_create(screen);
    lv_obj_set_size(sep, SCREEN_W, 1);
    lv_obj_set_pos(sep, 0, 352);
    lv_obj_set_style_bg_color(sep, COL_BORDER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    // ── Hardware specs panel (y=364 to y=476, 2 columns x 4 rows) ──
    lv_obj_t* infoPanel = lv_obj_create(screen);
    lv_obj_set_size(infoPanel, SCREEN_W, 112);
    lv_obj_set_pos(infoPanel, 0, 364);
    lv_obj_set_style_bg_color(infoPanel, COL_BG_DIM, 0);
    lv_obj_set_style_border_width(infoPanel, 0, 0);
    lv_obj_set_style_radius(infoPanel, 0, 0);
    lv_obj_set_style_pad_all(infoPanel, 8, 0);
    lv_obj_remove_flag(infoPanel, LV_OBJ_FLAG_SCROLLABLE);

    static const char* specsLeft[] = {
        "CPU: ESP32-P4 400MHz Dual-Core",
        "RAM: 32MB PSRAM + 768KB SRAM",
        "Flash: 16MB SPI-NOR",
        "Display: MIPI-DSI 800x480"
    };
    static const char* specsRight[] = {
        "Motor: TB6600 108:1 worm gear",
        "Microstep: 1/8 (1600 steps/rev)",
        "RPM Range: 0.1 - 5.0",
        "Firmware: " FW_VERSION " LVGL 9.x"
    };

    for (int i = 0; i < 4; i++) {
        lv_obj_t* lL = lv_label_create(infoPanel);
        lv_label_set_text(lL, specsLeft[i]);
        lv_obj_set_style_text_font(lL, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lL, COL_TEXT_VDIM, 0);
        lv_obj_set_pos(lL, 8, 4 + i * 26);

        lv_obj_t* lR = lv_label_create(infoPanel);
        lv_label_set_text(lR, specsRight[i]);
        lv_obj_set_style_text_font(lR, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lR, COL_TEXT_VDIM, 0);
        lv_obj_set_pos(lR, 420, 4 + i * 26);
    }

    LOG_I("Screen boot: full-screen terminal layout created");
}

void screen_boot_update() {
    // Empty update - for periodic refresh if needed
}

void screen_boot_update(int percent, const char* status) {
    screen_boot_set_progress(percent, status);
}
