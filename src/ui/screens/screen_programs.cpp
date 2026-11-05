// TIG Rotator Controller - Programs List Screen
// POST mockup #9: simple rows, chevron; tap left = run, tap right = edit; delete in program edit

#include <Arduino.h>
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../control/program_executor.h"
#include "../../motor/speed.h"
#include <atomic>
#include <cstdio>
#include <vector>

static lv_obj_t* programList = nullptr;
static lv_obj_t* newBtn = nullptr;
static lv_obj_t* countLabel = nullptr;
static std::atomic<bool> programsDirty{true};
static size_t lastPresetCount = 0;

static ScreenId backScreen = SCREEN_MENU;
void screen_programs_set_back(ScreenId screen) { backScreen = screen; }
static void back_event_cb(lv_event_t* e) { screens_show(backScreen); }

static Preset reviewedPreset;
static void start_reviewed_preset() {
  ProgramExecutorResult result = program_executor_start_preset(&reviewedPreset);
  if (result == PROGRAM_EXEC_OK) return;
  LOG_W("Program start blocked: %s", program_executor_result_name(result));
  const char* why = "The start request was rejected.\nTry again, or check Diagnostics.";
  if (result == PROGRAM_EXEC_INVALID_PRESET || result == PROGRAM_EXEC_INVALID_MODE) {
    why = "This program has invalid settings.\nCheck it in the editor.";
  } else if (result == PROGRAM_EXEC_BLOCKED_SAFETY) {
    why = "A fault or interlock blocks motion.\nCheck Diagnostics.";
  } else if (result == PROGRAM_EXEC_BLOCKED_STATE) {
    why = "The machine is busy.\nWait until it is idle, then start again.";
  }
  screen_confirm_create("PROGRAM BLOCKED", why, nullptr, nullptr, SCREEN_PROGRAMS);
}
static void load_preset_cb(lv_event_t* e) {
  int id = (int)(size_t)lv_event_get_user_data(e);
  if (!storage_get_preset(id, &reviewedPreset)) return;
  char details[320], timing[80];
  const Preset& p = reviewedPreset;
  if (!control_program_feasible(p)) {
    screen_confirm_create("PROGRAM BLOCKED", "Speed or angle exceeds the current motor range.\nAdjust RPM, part diameter or angle in the editor.", nullptr, nullptr, SCREEN_PROGRAMS);
    return;
  }
  const char* mode = p.mode == STATE_PULSE ? "Pulse" : p.mode == STATE_STEP ? "Step" : "Continuous";
  if (p.mode == STATE_STEP) snprintf(timing,sizeof(timing),"%.1f deg x%u / dwell %.1fs",(double)p.step_angle,p.step_repeats,(double)p.step_dwell_sec);
  else if (p.mode == STATE_PULSE) snprintf(timing,sizeof(timing),"On %.2f s / off %.2f s / %u cycles (0 = repeat)",p.pulse_on_ms/1000.0,p.pulse_off_ms/1000.0,p.pulse_cycles);
  else if (p.timer_auto_stop && p.timer_ms) snprintf(timing,sizeof(timing),"Auto-stop %.1fs / soft start %s",p.timer_ms/1000.0,p.cont_soft_start ? "on" : "off");
  else snprintf(timing,sizeof(timing),"Continuous / soft start %s",p.cont_soft_start ? "on" : "off");
  snprintf(details,sizeof(details),"%s\n%s / %.3f RPM / %s\nPart diameter: %.0f mm\n%s\nConfirm starts motion.",
      p.name,mode,(double)p.rpm,speed_resolve_direction((Direction)p.direction)==DIR_CCW ? "CCW" : "CW",
      (double)(p.workpiece_diameter_mm>0 ? p.workpiece_diameter_mm : D_EMNE*1000.0f),timing);
  screen_confirm_create("START PROGRAM",details,start_reviewed_preset,nullptr,SCREEN_MAIN,true);
}

static void edit_preset_cb(lv_event_t* e) {
  int slot = (int)(size_t)lv_event_get_user_data(e);
  screens_set_edit_slot(slot);
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void new_program_cb(lv_event_t* e) {
  (void)e;
  screens_set_edit_slot(-1);
  screens_show(SCREEN_PROGRAM_EDIT);
}

static void format_details(char* buf, size_t len, const Preset& p) {
  char rpm[16], extra[40] = "", diameter[20] = "";
  ui_format_rpm(rpm, sizeof(rpm), p.rpm);
  const char* mode = p.mode == STATE_PULSE ? "Pulse" : p.mode == STATE_STEP ? "Step" : "Continuous";
  if (p.mode == STATE_PULSE) {
    snprintf(extra, sizeof(extra), " / %.1f s on, %.1f s off", p.pulse_on_ms / 1000.0f, p.pulse_off_ms / 1000.0f);
  } else if (p.mode == STATE_STEP) {
    if (p.step_repeats > 1) snprintf(extra, sizeof(extra), " / %.0f\xC2\xB0 x%u", p.step_angle, (unsigned)p.step_repeats);
    else snprintf(extra, sizeof(extra), " / %.0f\xC2\xB0", p.step_angle);
  }
  if (p.workpiece_diameter_mm >= 1.0f) snprintf(diameter, sizeof(diameter), " / Dia %.0f mm", p.workpiece_diameter_mm);
  snprintf(buf, len, "%s / %s RPM%s%s", mode, rpm, extra, diameter);
}

void screen_programs_invalidate_widgets() {
  programList = nullptr;
  countLabel = nullptr;
  newBtn = nullptr;
  programsDirty.store(true, std::memory_order_relaxed);
}

void screen_programs_create() {
  lv_obj_t* screen = screenRoots[SCREEN_PROGRAMS];
  lv_obj_clean(screen);
  ui_create_header(screen, "Welding programs", "", &countLabel);
  programList = lv_obj_create(screen);
  lv_obj_set_size(programList, 800, 300);
  lv_obj_set_pos(programList, 0, 94);
  lv_obj_set_style_bg_opa(programList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(programList, 0, 0);
  lv_obj_set_style_pad_all(programList, 0, 0);
  lv_obj_set_scrollbar_mode(programList, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_scroll_dir(programList, LV_DIR_VER);
  ui_create_btn(screen, 24, 408, 152, 56, UI_LABEL_BACK, FONT_BTN, UI_BTN_NORMAL, back_event_cb, nullptr);
  ui_create_text(screen, 192, 428, 260, "Tap a program to review", FONT_NORMAL, COL_TEXT_DIM);
  newBtn =
      ui_create_btn(screen, 496, 408, 280, 56, "+ New", FONT_LARGE, UI_BTN_ACCENT, new_program_cb, nullptr);
  programsDirty.store(true, std::memory_order_relaxed);
}

void screen_programs_update() {
  if (!programList || !newBtn || !countLabel) return;

  std::vector<Preset> snapshot;
  size_t presetCount = 0;
  {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    presetCount = g_presets.size();
    if (!programsDirty.load(std::memory_order_relaxed) && presetCount == lastPresetCount) {
      xSemaphoreGive(g_presets_mutex);
      return;
    }
    programsDirty.store(false, std::memory_order_relaxed);
    lastPresetCount = presetCount;
    snapshot = g_presets;
    xSemaphoreGive(g_presets_mutex);
  }

  lv_obj_clean(programList);
  bool isFull = presetCount >= MAX_PRESETS;
  bool isEmpty = snapshot.empty();

  lv_label_set_text_fmt(countLabel, "%d / %d", (int)presetCount, MAX_PRESETS);
  lv_obj_set_style_text_color(countLabel, COL_TEXT_DIM, 0);

  if (isFull) {
    ui_btn_style_post(newBtn, UI_BTN_NORMAL);
    lv_obj_set_style_opa(newBtn, LV_OPA_50, 0);
    lv_obj_set_clickable(newBtn, false);
    lv_obj_t* nl = lv_obj_get_child(newBtn, 0);
    if (nl) lv_obj_set_style_text_color(nl, COL_TEXT_VDIM, 0);
  } else {
    ui_btn_style_post(newBtn, UI_BTN_ACCENT);
    lv_obj_set_style_opa(newBtn, LV_OPA_COVER, 0);
    lv_obj_set_clickable(newBtn, true);
    lv_obj_t* nl = lv_obj_get_child(newBtn, 0);
    if (nl) lv_obj_set_style_text_color(nl, ui_btn_label_color_post(UI_BTN_ACCENT), 0);
  }

  const int cardW = 752;
  const int cardH = 90;
  const int cardGap = 10;
  const int cardX = 24;
  const int runHitW = 616;
  const int editW = 112;
  const int editH = 56;
  int yPos = 0;

  for (size_t i = 0; i < presetCount; i++) {
    const auto& p = snapshot[i];

    lv_obj_t* card = lv_obj_create(programList);
    lv_obj_set_size(card, cardW, cardH);
    lv_obj_set_pos(card, cardX, yPos);
    lv_obj_set_style_bg_color(card, COL_BG_CARD, 0);
    lv_obj_set_style_radius(card, RADIUS_CARD, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, COL_BORDER, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_scrollable(card, false);
    lv_obj_set_clickable(card, false);

    char detailBuf[96];
    format_details(detailBuf, sizeof(detailBuf), p);

    lv_obj_t* nameLabel = lv_label_create(card);
    lv_label_set_text(nameLabel, p.name);
    lv_obj_set_style_text_font(nameLabel, FONT_XL, 0);
    lv_obj_set_style_text_color(nameLabel, COL_TEXT_BRIGHT, 0);
    lv_obj_set_pos(nameLabel, 22, 14);
    lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(nameLabel, runHitW - 36);

    lv_obj_t* detailLabel = lv_label_create(card);
    lv_label_set_text(detailLabel, detailBuf);
    lv_obj_set_style_text_font(detailLabel, FONT_SUBTITLE, 0);
    lv_obj_set_style_text_color(detailLabel, COL_TEXT_DIM, 0);
    lv_obj_set_pos(detailLabel, 22, 52);
    lv_label_set_long_mode(detailLabel, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_width(detailLabel, runHitW - 36);

    lv_obj_t* runBtn = lv_button_create(card);
    lv_obj_set_size(runBtn, runHitW, cardH);
    lv_obj_set_pos(runBtn, 0, 0);
    lv_obj_set_style_bg_opa(runBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(runBtn, 0, 0);
    lv_obj_set_style_shadow_width(runBtn, 0, 0);
    lv_obj_add_event_cb(runBtn, load_preset_cb, LV_EVENT_CLICKED, (void*)(size_t)p.id);

    ui_create_btn(card, cardW - editW - 16, (cardH - editH) / 2, editW, editH, "Edit", FONT_BTN,
                  UI_BTN_NORMAL, edit_preset_cb, (void*)(size_t)i);

    yPos += cardH + cardGap;
  }

  if (isEmpty) {
    lv_obj_t* emptyLabel = lv_label_create(programList);
    lv_label_set_text(emptyLabel, "No programs yet.\nTap + New to create one.");
    lv_obj_set_style_text_font(emptyLabel, FONT_LARGE, 0);
    lv_obj_set_style_text_color(emptyLabel, COL_TEXT_DIM, 0);
    lv_obj_align(emptyLabel, LV_ALIGN_CENTER, 0, 0);
  }
}

void screen_programs_mark_dirty() { programsDirty.store(true, std::memory_order_relaxed); }
