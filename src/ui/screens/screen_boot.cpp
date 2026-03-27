#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../../config.h"

static lv_obj_t* progressBar = nullptr;
static lv_obj_t* progressFill = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* percentLabel = nullptr;
static int currentProgress = 0;

static const char* bootMessages[] = {
    "Initializing system...",
    "Loading configuration...",
    "Checking motor systems...",
    "Starting UI system...",
    "Ready"
};

void screen_boot_set_progress(int percent, const char* message) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    currentProgress = percent;

    if (progressBar) {
        lv_bar_set_value(progressBar, percent, LV_ANIM_OFF);
    }

    if (percentLabel) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", percent);
        lv_label_set_text(percentLabel, buf);
    }

    if (statusLabel && message) {
        lv_label_set_text(statusLabel, message);
    }
}

void screen_boot_increment(int delta) {
    screen_boot_set_progress(currentProgress + delta, nullptr);
}

void screen_boot_create() {
    lv_obj_t* screen = screenRoots[SCREEN_BOOT];
    lv_obj_set_style_bg_color(screen, COL_BG, 0);

    lv_obj_t* subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "TIG ROTATOR");
    lv_obj_set_style_text_font(subtitle, FONT_SMALL, 0);
    lv_obj_set_style_text_color(subtitle, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_letter_space(subtitle, 3, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, SY(50));

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "CONTROLLER");
    lv_obj_set_style_text_font(title, FONT_HUGE, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, SY(78));

    lv_obj_t* deco = lv_obj_create(screen);
    lv_obj_set_size(deco, SW(120), SH(2));
    lv_obj_align(deco, LV_ALIGN_TOP_MID, 0, SY(88));
    lv_obj_set_style_bg_color(deco, COL_ACCENT, 0);
    lv_obj_set_style_radius(deco, 1, 0);
    lv_obj_set_style_border_width(deco, 0, 0);
    lv_obj_set_style_pad_all(deco, 0, 0);
    lv_obj_set_style_shadow_width(deco, 0, 0);
    lv_obj_clear_flag(deco, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* arc = lv_arc_create(screen);
    lv_obj_set_size(arc, SH(56), SH(56));
    lv_obj_set_pos(arc, SX(180) - SH(28), SY(132) - SH(28));
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_obj_set_style_arc_color(arc, COL_GAUGE_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* gearIcon = lv_label_create(screen);
    lv_label_set_text(gearIcon, "\xE2\x9A\x99");
    lv_obj_set_style_text_font(gearIcon, FONT_LARGE, 0);
    lv_obj_set_style_text_color(gearIcon, COL_ACCENT, 0);
    lv_obj_align(gearIcon, LV_ALIGN_TOP_MID, 0, SY(132) - 14);

    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, bootMessages[0]);
    lv_obj_set_style_text_font(statusLabel, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(statusLabel, COL_TEXT, 0);
    lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, SY(178));

    progressBar = lv_bar_create(screen);
    lv_obj_set_size(progressBar, SW(240), SH(4));
    lv_obj_align(progressBar, LV_ALIGN_TOP_MID, 0, SY(190));
    lv_bar_set_range(progressBar, 0, 100);
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progressBar, COL_GAUGE_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progressBar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(progressBar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(progressBar, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(progressBar, 0, LV_PART_INDICATOR);

    percentLabel = lv_label_create(screen);
    lv_label_set_text(percentLabel, "0%");
    lv_obj_set_style_text_font(percentLabel, FONT_SMALL, 0);
    lv_obj_set_style_text_color(percentLabel, COL_TEXT_DIM, 0);
    lv_obj_align(percentLabel, LV_ALIGN_TOP_MID, 0, SY(210));

    lv_obj_t* version = lv_label_create(screen);
    lv_label_set_text(version, "v1.2.0 \xC2\xB7 ESP32-P4 \xC2\xB7 LVGL 8.x");
    lv_obj_set_style_text_font(version, FONT_SMALL, 0);
    lv_obj_set_style_text_color(version, COL_TEXT_DIM, 0);
    lv_obj_align(version, LV_ALIGN_TOP_MID, 0, SY(232));

    LOG_I("Screen boot: created");
}

void screen_boot_update() {
}
