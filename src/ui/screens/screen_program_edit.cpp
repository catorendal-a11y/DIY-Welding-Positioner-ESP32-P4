// Program drafts: explicit run mode, independent available modes and full-screen input.
#include "../screens.h"
#include "../theme.h"
#include "../value_format.h"
#include "../../utils/numeric_input.h"
#include "../../config.h"
#include "../../storage/storage.h"
#include "../../storage/name_policy.h"
#include "../../control/control.h"
#include "../../motor/speed.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int editSlot = -1;
static uint32_t saveTicket = 0;
static bool savePending = false;
static lv_obj_t* saveButton = nullptr;
static lv_obj_t* cancelButton = nullptr;
static Preset editPreset{}, renderedPreset{};
static bool renderedValid = false;
static lv_obj_t *nameButton = nullptr, *rpmButton = nullptr, *modeSettingsBtn = nullptr, *detailLabel = nullptr;
static lv_obj_t *runButtons[3]{}, *availableButtons[3]{};
static lv_obj_t *entryPanel = nullptr, *entryField = nullptr, *keyboard = nullptr, *entryError = nullptr;
static bool entryRpm = false, kbClosePending = false;
static lv_style_transition_dsc_t instantTransition;
static const lv_style_prop_t transitionProps[] = {LV_STYLE_BG_COLOR,LV_STYLE_BG_OPA,LV_STYLE_OPA,LV_STYLE_BORDER_COLOR,LV_STYLE_OUTLINE_OPA,LV_STYLE_RECOLOR,LV_STYLE_RECOLOR_OPA,0};
static const SystemState modes[] = {STATE_RUNNING, STATE_PULSE, STATE_STEP};
static const char* modeNames[] = {"CONTINUOUS", "PULSE", "STEP"};

static lv_obj_t* text(lv_obj_t* parent, int x, int y, int width, const char* value, const lv_font_t* font, lv_color_t color) {
  auto obj = lv_label_create(parent); lv_obj_set_pos(obj,x,y); lv_obj_set_width(obj,width);
  lv_label_set_text(obj,value); lv_label_set_long_mode(obj,LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_style_text_font(obj,font,0); lv_obj_set_style_text_color(obj,color,0); return obj;
}
static void do_cleanup_kb() {
  if (entryPanel) { lv_keyboard_set_textarea(keyboard,nullptr); lv_obj_delete_async(entryPanel); }
  entryPanel = entryField = keyboard = entryError = nullptr; kbClosePending = false;
}
void screen_program_edit_leave() { do_cleanup_kb(); }
static void lock_pending_draft(lv_obj_t* obj) {
  if (!savePending || !obj) return;
  if (lv_obj_check_type(obj,&lv_button_class) && obj != saveButton && obj != cancelButton)
    lv_obj_set_disabled(obj,true);
  for (uint32_t i=0; i<lv_obj_get_child_count(obj); ++i)
    lock_pending_draft(lv_obj_get_child(obj,i));
}
void screen_program_edit_poll_keyboard() {
  if (kbClosePending) do_cleanup_kb();
  if (!savePending) return;
  const StorageStatus status = storage_presets_save_status(saveTicket);
  if (status == STORAGE_SAVED) { savePending = false; screens_request_show(SCREEN_PROGRAMS); }
  else if (saveButton) lv_label_set_text(lv_obj_get_child(saveButton,0),status == STORAGE_ERROR ? "SAVE FAILED / RETRYING" : "SAVING...");
}
static void input_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CANCEL) { kbClosePending = true; return; }
  if (lv_event_get_code(e) != LV_EVENT_READY) return;
  const char* value = lv_textarea_get_text(entryField);
  if (entryRpm) {
    float rpm = 0;
    if (!parse_float_entry(value,rpm) || rpm < MIN_RPM || rpm > speed_get_rpm_max()) {
      char error[96], minimum[16], maximum[16];
      ui_format_rpm(minimum,sizeof(minimum),MIN_RPM); ui_format_rpm(maximum,sizeof(maximum),speed_get_rpm_max());
      snprintf(error,sizeof(error),"Enter a speed from %s to %s RPM.",minimum,maximum); lv_label_set_text(entryError,error); return;
    }
    editPreset.rpm = rpm;
  } else {
    while (*value == ' ') ++value;
    size_t length = strlen(value); while (length && value[length-1] == ' ') --length;
    // LVGL limits characters; the stored field limits UTF-8 bytes. Never split a character.
    if (length >= sizeof(editPreset.name)) { lv_label_set_text(entryError,"Name must fit 31 UTF-8 bytes. Shorten it and try again."); return; }
    if (length && !program_name_valid(value, sizeof(editPreset.name), true)) { lv_label_set_text(entryError,"Use English letters, numbers and printable symbols (max 31)."); return; }
    if (!length) strlcpy(editPreset.name,"Untitled",sizeof(editPreset.name));
    else { memcpy(editPreset.name,value,length); editPreset.name[length] = 0; }
  }
  kbClosePending = true;
}
static void open_input_cb(lv_event_t* e) {
  if (savePending || entryPanel) return;
  entryRpm = (intptr_t)lv_event_get_user_data(e) == 1;
  entryPanel = lv_obj_create(lv_layer_top()); lv_obj_set_size(entryPanel,SCREEN_W,SCREEN_H);
  lv_obj_set_pos(entryPanel,0,0); lv_obj_set_style_pad_all(entryPanel,0,0);
  lv_obj_set_style_bg_color(entryPanel,COL_BG,0); lv_obj_set_style_bg_opa(entryPanel,LV_OPA_COVER,0);
  lv_obj_set_scrollable(entryPanel,false);
  text(entryPanel,24,24,570,entryRpm ? "Program speed" : "Program name",FONT_XL,COL_TEXT);
  ui_create_btn(entryPanel,620,20,156,48,"CANCEL",FONT_BTN,UI_BTN_NORMAL,
                [](lv_event_t*) { kbClosePending = true; },nullptr);
  text(entryPanel,24,70,752,entryRpm ? "Set exact workpiece RPM. This edits the program only." : "English letters, numbers and symbols. Maximum 31 characters.",FONT_SUBTITLE,COL_TEXT_DIM);
  entryField = lv_textarea_create(entryPanel); lv_textarea_set_one_line(entryField,true);
  lv_obj_set_pos(entryField,24,112); lv_obj_set_size(entryField,752,54);
  lv_obj_set_style_text_font(entryField,entryRpm ? FONT_XL : FONT_LARGE,0);
  if (entryRpm) {
    lv_textarea_set_max_length(entryField,12); lv_textarea_set_accepted_chars(entryField,"0123456789.,");
    char value[24]; snprintf(value,sizeof(value),"%.3f",(double)editPreset.rpm); lv_textarea_set_text(entryField,value);
  } else {
    lv_textarea_set_max_length(entryField,31);
    lv_textarea_set_text(entryField,editPreset.name);
  }
  entryError = text(entryPanel,24,178,752,"",FONT_SUBTITLE,COL_RED); lv_label_set_max_lines(entryError,2);
  keyboard = lv_keyboard_create(entryPanel); lv_obj_set_size(keyboard,800,236);
  lv_obj_align(keyboard,LV_ALIGN_BOTTOM_MID,0,0);
  lv_keyboard_set_mode(keyboard,entryRpm ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(keyboard,entryField);
  lv_obj_add_event_cb(keyboard,input_cb,LV_EVENT_READY,nullptr); lv_obj_add_event_cb(keyboard,input_cb,LV_EVENT_CANCEL,nullptr);
}
static void mode_select_cb(lv_event_t* e) {
  if (savePending) return;
  editPreset.mode = modes[(intptr_t)lv_event_get_user_data(e)];
  editPreset.mode_mask |= preset_mode_to_mask(editPreset.mode);
  screen_program_edit_update_ui();
}
static void available_cb(lv_event_t* e) {
  if (savePending) return;
  const SystemState mode = modes[(intptr_t)lv_event_get_user_data(e)];
  if (mode == editPreset.mode) return; // The active run mode must remain available.
  editPreset.mode_mask ^= preset_mode_to_mask(mode); screen_program_edit_update_ui();
}
static void rpm_adjust_cb(lv_event_t* e) {
  if (savePending) return;
  const float increment = ui_rpm_increment(editPreset.rpm);
  editPreset.rpm = constrain(editPreset.rpm + ((intptr_t)lv_event_get_user_data(e) < 0 ? -increment : increment),MIN_RPM,speed_get_rpm_max());
  screen_program_edit_update_ui();
}
static void mode_settings_cb(lv_event_t*) {
  if (savePending) return;
  if (editPreset.mode == STATE_PULSE) screens_show(SCREEN_EDIT_PULSE);
  else if (editPreset.mode == STATE_STEP) screens_show(SCREEN_EDIT_STEP);
  else screens_show(SCREEN_EDIT_CONT);
}
static void cancel_cb(lv_event_t*) { screens_show(SCREEN_PROGRAMS); }
static void delete_preset_from_edit_do() {
  int slot = editSlot;
  if (slot < 0) return;
  xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
  if (slot >= 0 && slot < (int)g_presets.size()) {
    g_presets.erase(g_presets.begin() + slot);
    for (size_t i = 0; i < g_presets.size(); i++) {
      g_presets[i].id = i + 1;
    }
    xSemaphoreGive(g_presets_mutex);
    storage_save_presets();
  } else {
    xSemaphoreGive(g_presets_mutex);
  }
  screen_programs_mark_dirty();
}

static void delete_preset_prompt_cb(lv_event_t* e) {
  if (savePending) return;
  (void)e;
  char buf[64];
  snprintf(buf, sizeof(buf), "Delete \"%s\"?", editPreset.name);
  screen_confirm_create(buf, "This action cannot be undone.", delete_preset_from_edit_do, nullptr,
                        SCREEN_PROGRAMS);
}


static void save_preset_cb(lv_event_t*) {
  if (savePending) return;
  preset_clamp_mode_to_mask(&editPreset);
  editPreset.rpm = constrain(editPreset.rpm,MIN_RPM,speed_get_rpm_max());
  xSemaphoreTake(g_presets_mutex,portMAX_DELAY);
  if (editSlot >= 0 && editSlot < (int)g_presets.size()) {
    editPreset.id = editSlot+1; g_presets[editSlot] = editPreset;
  } else {
    if (g_presets.size() >= MAX_PRESETS) {
      xSemaphoreGive(g_presets_mutex);
      screen_confirm_create("FULL","Maximum 16 programs reached.",nullptr,nullptr,SCREEN_NONE); return;
    }
    editSlot = (int)g_presets.size();
    editPreset.id = g_presets.size()+1; g_presets.push_back(editPreset);
  }
  xSemaphoreGive(g_presets_mutex); saveTicket = storage_request_presets_save(); savePending = true;
  if (saveButton) lv_label_set_text(lv_obj_get_child(saveButton,0),"SAVING...");
  lock_pending_draft(screenRoots[SCREEN_PROGRAM_EDIT]);
  screen_programs_mark_dirty();
}
void screen_program_edit_create(int slot) {
  do_cleanup_kb(); auto root = screenRoots[SCREEN_PROGRAM_EDIT]; lv_obj_clean(root);
  lv_obj_set_scrollable(root,false); lv_obj_set_style_bg_color(root,COL_BG,0);
  if (slot != -2) { editSlot = slot; savePending = false; saveTicket = 0; }
  if (slot != -2) {
    xSemaphoreTake(g_presets_mutex, portMAX_DELAY);
    if (slot >= 0 && slot < (int)g_presets.size()) {
      editPreset = g_presets[slot];
      preset_clamp_mode_to_mask(&editPreset);
    } else {
      editSlot = -1;
      editPreset = Preset{};
      // New preset — start from current machine state (direction, mode, RPM) so it feels consistent.
      editPreset.id = 0;
      snprintf(editPreset.name, sizeof(editPreset.name), "New Program");
      {
        SystemState cs = control_get_state();
        if (cs == STATE_PULSE) {
          editPreset.mode = STATE_PULSE;
        } else if (cs == STATE_STEP) {
          editPreset.mode = STATE_STEP;
        } else {
          editPreset.mode = STATE_RUNNING;
        }
      }
      editPreset.rpm = speed_get_target_rpm();
      editPreset.pulse_on_ms = 500;
      editPreset.pulse_off_ms = 500;
      editPreset.step_angle = 90.0f;
      editPreset.workpiece_diameter_mm = speed_get_workpiece_diameter_mm();
      editPreset.timer_ms = 30000;
      editPreset.direction = (uint8_t)speed_get_requested_direction();
      editPreset.pulse_cycles = 0;  // infinite
      editPreset.step_repeats = 1;
      editPreset.step_dwell_sec = 0.0f;
      editPreset.timer_auto_stop = 1;  // auto stop
      editPreset.cont_soft_start = 0;
      editPreset.mode_mask = PRESET_MASK_ALL;
      preset_clamp_mode_to_mask(&editPreset);
    }
    xSemaphoreGive(g_presets_mutex);
  }

  renderedValid = false;
  if (!std::isfinite(editPreset.rpm)) editPreset.rpm = MIN_RPM;
  editPreset.rpm = constrain(editPreset.rpm,MIN_RPM,speed_get_rpm_max());
  ui_create_header(root,editSlot >= 0 ? "Edit program" : "New program","PROGRAM DRAFT",nullptr);
  nameButton = ui_create_btn(root,20,90,760,56,editPreset.name,FONT_XL,UI_BTN_NORMAL,open_input_cb,nullptr);
  auto name = lv_obj_get_child(nameButton,0); lv_obj_set_width(name,660); lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_LEFT,0);
  lv_label_set_long_mode(name,LV_LABEL_LONG_MODE_WRAP); lv_label_set_max_lines(name,1); lv_obj_align(name,LV_ALIGN_LEFT_MID,16,0);
  text(nameButton,684,18,64,"EDIT",FONT_SUBTITLE,COL_ACCENT);
  text(root,20,154,200,"RUN MODE",FONT_SUBTITLE,COL_TEXT_DIM);
  text(root,400,154,380,"One mode runs at a time",FONT_SUBTITLE,COL_TEXT_DIM);
  lv_style_transition_dsc_init(&instantTransition,transitionProps,lv_anim_path_linear,0,0,nullptr);
  for (int i=0;i<3;++i) {
    runButtons[i] = ui_create_btn(root,20+i*257,178,246,48,modeNames[i],FONT_BTN,UI_BTN_NORMAL,mode_select_cb,(void*)(intptr_t)i);
    availableButtons[i] = ui_create_btn(root,166+i*207,238,200,36,"",FONT_SMALL,UI_BTN_NORMAL,available_cb,(void*)(intptr_t)i);
    for (auto button : {runButtons[i],availableButtons[i]}) {
      lv_obj_set_style_transition(button,&instantTransition,0);
      lv_obj_set_style_transition(button,&instantTransition,LV_STATE_CHECKED);
      lv_obj_set_style_transition(button,&instantTransition,LV_STATE_DISABLED);
    }
  }
  text(root,20,247,138,"AVAILABLE",FONT_SMALL,COL_TEXT_DIM);
  auto speed = ui_create_post_card(root,20,288,368,100);
  text(speed,14,10,220,"TARGET SPEED / RPM",FONT_SMALL,COL_TEXT_DIM);
  rpmButton = ui_create_btn(speed,14,31,170,56,"",FONT_HUGE,UI_BTN_NORMAL,open_input_cb,(void*)1);
  lv_obj_set_style_border_width(rpmButton,0,0); lv_obj_set_style_bg_opa(rpmButton,LV_OPA_TRANSP,0);
  ui_create_btn(speed,194,35,70,52,"-",FONT_XL,UI_BTN_NORMAL,rpm_adjust_cb,(void*)-1);
  ui_create_btn(speed,274,35,78,52,"+",FONT_XL,UI_BTN_ACCENT,rpm_adjust_cb,(void*)1);
  modeSettingsBtn = ui_create_btn(root,404,288,376,100,"MODE SETTINGS >",FONT_BTN,UI_BTN_NORMAL,mode_settings_cb,nullptr);
  auto settingsTitle = lv_obj_get_child(modeSettingsBtn,0); lv_obj_align(settingsTitle,LV_ALIGN_TOP_LEFT,14,12);
  detailLabel = text(modeSettingsBtn,14,43,344,"",FONT_SMALL,COL_TEXT_DIM); lv_label_set_max_lines(detailLabel,3);
  if (editSlot >= 0) {
    ui_create_btn(root,20,408,168,54,"DELETE",FONT_BTN,UI_BTN_DANGER,delete_preset_prompt_cb,nullptr);
    cancelButton = ui_create_btn(root,204,408,186,54,"CANCEL",FONT_BTN,UI_BTN_NORMAL,cancel_cb,nullptr);
    saveButton = ui_create_btn(root,406,408,374,54,"SAVE",FONT_BTN,UI_BTN_ACCENT,save_preset_cb,nullptr);
  } else {
    cancelButton = ui_create_btn(root,20,408,240,54,"CANCEL",FONT_BTN,UI_BTN_NORMAL,cancel_cb,nullptr);
    saveButton = ui_create_btn(root,276,408,504,54,"SAVE",FONT_BTN,UI_BTN_ACCENT,save_preset_cb,nullptr);
  }
  screen_program_edit_update_ui();
  lock_pending_draft(root);
}
void screen_program_edit_update_ui() {
  screen_program_edit_poll_keyboard();
  if (!nameButton) return;
  // Avoid 25 Hz label allocations/style invalidations when the draft is unchanged.
  if (renderedValid && strcmp(renderedPreset.name,editPreset.name) == 0 &&
      renderedPreset.rpm == editPreset.rpm && renderedPreset.mode == editPreset.mode &&
      renderedPreset.mode_mask == editPreset.mode_mask && renderedPreset.direction == editPreset.direction &&
      renderedPreset.pulse_on_ms == editPreset.pulse_on_ms && renderedPreset.pulse_off_ms == editPreset.pulse_off_ms &&
      renderedPreset.pulse_cycles == editPreset.pulse_cycles && renderedPreset.step_angle == editPreset.step_angle &&
      renderedPreset.step_repeats == editPreset.step_repeats && renderedPreset.step_dwell_sec == editPreset.step_dwell_sec &&
      renderedPreset.workpiece_diameter_mm == editPreset.workpiece_diameter_mm &&
      renderedPreset.cont_soft_start == editPreset.cont_soft_start && renderedPreset.timer_ms == editPreset.timer_ms &&
      renderedPreset.timer_auto_stop == editPreset.timer_auto_stop) return;
  renderedPreset = editPreset; renderedValid = true;
  lv_label_set_text(lv_obj_get_child(nameButton,0),editPreset.name);
  lv_obj_set_style_text_font(lv_obj_get_child(nameButton,0),strlen(editPreset.name) > 20 ? FONT_BTN : FONT_XL,0);
  auto rpm = lv_obj_get_child(rpmButton,0); ui_set_rpm(rpm,editPreset.rpm); lv_obj_set_style_text_color(rpm,COL_ACCENT,0);
  for (int i=0;i<3;++i) {
    const bool active = editPreset.mode == modes[i], included = editPreset.mode_mask & preset_mode_to_mask(modes[i]);
    ui_btn_style_post(runButtons[i],active ? UI_BTN_ACCENT : UI_BTN_NORMAL); lv_obj_set_checked(runButtons[i],active);
    lv_obj_set_style_bg_color(runButtons[i],COL_ACCENT,LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(runButtons[i],LV_OPA_COVER,LV_STATE_CHECKED);
    lv_obj_set_style_recolor_opa(runButtons[i],LV_OPA_TRANSP,LV_STATE_CHECKED);
    lv_obj_set_style_text_color(lv_obj_get_child(runButtons[i],0),ui_btn_label_color_post(active ? UI_BTN_ACCENT : UI_BTN_NORMAL),0);
    auto allowed = availableButtons[i]; lv_obj_set_checked(allowed,included); lv_obj_set_disabled(allowed,active);
    ui_btn_style_post(allowed,UI_BTN_NORMAL);
    lv_obj_set_style_bg_color(allowed,COL_BTN_BG,LV_STATE_CHECKED);
    lv_obj_set_style_recolor_opa(allowed,LV_OPA_TRANSP,LV_STATE_CHECKED);
    lv_obj_set_style_border_color(allowed,included ? COL_ACCENT : COL_BORDER,0);
    char status[40]; snprintf(status,sizeof(status),"%s %s",i == 0 ? "CONT" : modeNames[i],included ? "ON" : "OFF");
    lv_label_set_text(lv_obj_get_child(allowed,0),status);
    lv_obj_set_style_text_color(lv_obj_get_child(allowed,0),included ? COL_TEXT : COL_TEXT_VDIM,0);
  }
  char detail[160]; const char* direction = speed_resolve_direction((Direction)editPreset.direction) == DIR_CCW ? "CCW" : "CW";
  if (editPreset.mode == STATE_PULSE) {
    char cycles[24]; if (editPreset.pulse_cycles) snprintf(cycles,sizeof(cycles),"%u cycles",editPreset.pulse_cycles); else strlcpy(cycles,"Repeat continuously",sizeof(cycles));
    snprintf(detail,sizeof(detail),"%s / ON %.2fs / OFF %.2fs\n%s",direction,editPreset.pulse_on_ms/1000.0,editPreset.pulse_off_ms/1000.0,cycles);
  } else if (editPreset.mode == STATE_STEP) {
    char diameter[32]; if (editPreset.workpiece_diameter_mm >= 1) snprintf(diameter,sizeof(diameter),"OD %.1f mm",(double)editPreset.workpiece_diameter_mm); else strlcpy(diameter,"Default diameter",sizeof(diameter));
    snprintf(detail,sizeof(detail),"%s / %.1f deg / %u repeats\n%s / dwell %.1fs",direction,(double)editPreset.step_angle,editPreset.step_repeats,diameter,(double)editPreset.step_dwell_sec);
  } else {
    char timer[32]; if (editPreset.timer_auto_stop && editPreset.timer_ms) snprintf(timer,sizeof(timer),"Auto stop %.1fs",editPreset.timer_ms/1000.0); else strlcpy(timer,"No auto stop",sizeof(timer));
    snprintf(detail,sizeof(detail),"%s / Soft start %s\n%s",direction,editPreset.cont_soft_start ? "ON" : "OFF",timer);
  }
  lv_label_set_text(detailLabel,detail);
  lock_pending_draft(screenRoots[SCREEN_PROGRAM_EDIT]);
}
Preset* screen_program_edit_get_preset() { return &editPreset; }
void screen_program_edit_invalidate_widgets() {
  do_cleanup_kb(); saveButton = cancelButton = nullptr; nameButton = rpmButton = modeSettingsBtn = detailLabel = nullptr; renderedValid = false;
  for (auto& button : runButtons) button = nullptr;
  for (auto& button : availableButtons) button = nullptr;
}
