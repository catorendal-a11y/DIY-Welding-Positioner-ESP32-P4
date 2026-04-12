// TIG Rotator Controller - Main Screen
// Gauge center with RPM, side buttons, bottom bar
// Gauge semicircle (theme MAIN_GAUGE_*), pot controls RPM
// RPM value in FONT_HUGE in center
// Side buttons: 3 left, 3 right, all gray by default, highlight accent when active
// Bottom bar: START + STOP (56px tall)

#include "../screens.h"
#include "../theme.h"
#include "../../config.h"
#include "../../motor/speed.h"
#include "../../control/control.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
// Quick-pulse defaults (ms) — used when pulse button is pressed from main screen
static uint32_t mainPulseOnMs  = 500;
static uint32_t mainPulseOffMs = 500;

static lv_obj_t* mainScreenPtr = nullptr;
static lv_obj_t* stateLabel = nullptr;
static lv_obj_t* rpmLabel = nullptr;
static lv_obj_t* rpmUnitLabel = nullptr;
static lv_obj_t* rpmIndicatorArc = nullptr;
static lv_obj_t* pulseOnLabel = nullptr;
static lv_obj_t* pulseOffLabel = nullptr;

static lv_obj_t* startBtn = nullptr;
static lv_obj_t* stopBtn = nullptr;
static lv_obj_t* jogBtn = nullptr;
static lv_obj_t* cwBtn = nullptr;
static lv_obj_t* pulseBtn = nullptr;
static lv_obj_t* pedalBtn = nullptr;
static lv_obj_t* pedalLabel = nullptr;

// ───────────────────────────────────────────────────────────────────────────────
// STATE
// ───────────────────────────────────────────────────────────────────────────────
static const char* state_strings[] = {
  "IDLE", "RUN", "PULSE", "STEP", "JOG", "STOP", "E-STOP"
};
static SystemState prevState = STATE_IDLE;
static Direction prevDir = DIR_CW;
static float prevRpm = -1.0f;
static bool prevPedalEnabled = false;
static bool mainDirty = true;

static void screen_main_set_dirty() { mainDirty = true; }
static lv_color_t get_state_color(int state) {
  switch (state) {
    case STATE_RUNNING: return COL_GREEN;
    case STATE_ESTOP:   return COL_RED;
    default:            return COL_ACCENT;
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT handlers
// ───────────────────────────────────────────────────────────────────────────────
static void start_event_cb(lv_event_t*) {
  if (control_get_state() == STATE_IDLE) { control_start_continuous(); screen_main_set_dirty(); }
}
static void stop_event_cb(lv_event_t*) {
  control_stop(); screen_main_set_dirty();
}
static void jog_press_cb(lv_event_t*) {
  if (speed_get_direction() == DIR_CW) control_start_jog_cw();
  else control_start_jog_ccw();
  screen_main_set_dirty();
}
static void jog_release_cb(lv_event_t*) {
  control_stop(); screen_main_set_dirty();
}
static void cw_event_cb(lv_event_t*) {
  Direction d = speed_get_direction();
  speed_set_direction(d == DIR_CW ? DIR_CCW : DIR_CW);
  screen_main_set_dirty();
}
static void pulse_event_cb(lv_event_t*) {
  control_start_pulse(mainPulseOnMs, mainPulseOffMs);
  screen_main_set_dirty();
}
static void pulse_on_down_cb(lv_event_t*) {
  if (mainPulseOnMs >= 200) mainPulseOnMs -= 100;
  screen_main_set_dirty();
}
static void pulse_on_up_cb(lv_event_t*) {
  if (mainPulseOnMs <= 4900) mainPulseOnMs += 100;
  screen_main_set_dirty();
}
static void pulse_off_down_cb(lv_event_t*) {
  if (mainPulseOffMs >= 200) mainPulseOffMs -= 100;
  screen_main_set_dirty();
}
static void pulse_off_up_cb(lv_event_t*) {
  if (mainPulseOffMs <= 4900) mainPulseOffMs += 100;
  screen_main_set_dirty();
}
static void pedal_toggle_cb(lv_event_t*) {
  bool enabled = !speed_get_pedal_enabled();
  speed_set_pedal_enabled(enabled);
  screen_main_set_dirty();
}
static void menu_event_cb(lv_event_t*) {
  screens_show(SCREEN_MENU);
}

// ───────────────────────────────────────────────────────────────────────────────
// Helpers
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* make_btn(lv_obj_t* parent, int x, int y, int w, int h,
                           const char* text, bool accent) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_style_radius(btn, RADIUS_BTN, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);

  if (accent) {
    lv_obj_set_style_bg_color(btn, COL_BG_ACTIVE, 0);
    lv_obj_set_style_border_color(btn, COL_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
  } else {
    lv_obj_set_style_bg_color(btn, COL_BTN_BG, 0);
    lv_obj_set_style_border_color(btn, COL_BORDER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
  }

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(lbl, accent ? COL_ACCENT : COL_TEXT, 0);
  lv_obj_center(lbl);
  return btn;
}

static void set_btn_style(lv_obj_t* btn, lv_color_t bg, lv_color_t border,
                           int bw, lv_color_t text_col) {
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_border_color(btn, border, 0);
  lv_obj_set_style_border_width(btn, bw, 0);
  lv_obj_t* lbl = lv_obj_get_child(btn, 0);
  if (lbl) lv_obj_set_style_text_color(lbl, text_col, 0);
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN CREATE
// Layout: Header 30 | Side buttons | Gauge center | Bottom bar
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create() {
  mainScreenPtr = screenRoots[SCREEN_MAIN];
  lv_obj_set_style_bg_color(mainScreenPtr, COL_BG, 0);

  // ── Header (30px) ──
  lv_obj_t* header = lv_obj_create(mainScreenPtr);
  lv_obj_set_size(header, SCREEN_W, HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, COL_BG_HEADER, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "TIG-ROTATOR");
  lv_obj_set_style_text_font(title, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_pos(title, PAD_X, 8);

  stateLabel = lv_label_create(header);
  lv_label_set_text(stateLabel, "IDLE");
  lv_obj_set_style_text_font(stateLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stateLabel, COL_GREEN, 0);
  lv_obj_set_pos(stateLabel, 160, 8);

  // ── Gauge semicircle (theme MAIN_GAUGE_*) ──
  const int arcCX = MAIN_GAUGE_CX;
  const int arcCY = MAIN_GAUGE_CY;
  const int arcR = MAIN_GAUGE_R;

  // Track arc (background)
  lv_obj_t* gaugeTrack = lv_arc_create(mainScreenPtr);
  int trackSize = arcR * 2 + 20;
  lv_obj_set_size(gaugeTrack, trackSize, trackSize);
  lv_obj_set_pos(gaugeTrack, arcCX - trackSize/2, arcCY - trackSize/2);
  lv_obj_remove_flag(gaugeTrack, LV_OBJ_FLAG_SCROLLABLE);
  lv_arc_set_bg_angles(gaugeTrack, 135, 405);
  lv_arc_set_value(gaugeTrack, 0);
  lv_obj_remove_style(gaugeTrack, NULL, LV_PART_KNOB);
  lv_obj_remove_style(gaugeTrack, NULL, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(gaugeTrack, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(gaugeTrack, 0, 0);
  lv_obj_set_style_pad_all(gaugeTrack, MAIN_GAUGE_TRACK_PAD, 0);
  lv_obj_set_style_arc_color(gaugeTrack, COL_GAUGE_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_width(gaugeTrack, MAIN_GAUGE_TRACK_W, LV_PART_MAIN);
  lv_obj_remove_flag(gaugeTrack, LV_OBJ_FLAG_CLICKABLE);

  // Active arc (value indicator)
  rpmIndicatorArc = lv_arc_create(mainScreenPtr);
  int activeSize = arcR * 2;
  lv_obj_set_size(rpmIndicatorArc, activeSize, activeSize);
  lv_obj_set_pos(rpmIndicatorArc, arcCX - activeSize/2, arcCY - activeSize/2);
  lv_arc_set_rotation(rpmIndicatorArc, 135);
  lv_arc_set_bg_angles(rpmIndicatorArc, 0, 270);
  lv_arc_set_range(rpmIndicatorArc, 0, (int32_t)(speed_get_rpm_max() * 100.0f + 0.5f));
  lv_arc_set_value(rpmIndicatorArc, 0);
  lv_obj_remove_style(rpmIndicatorArc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(rpmIndicatorArc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(rpmIndicatorArc, COL_BG, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(rpmIndicatorArc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_color(rpmIndicatorArc, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(rpmIndicatorArc, MAIN_GAUGE_IND_W, LV_PART_INDICATOR);

  // "RPM" unit — horizontal center; value label stacked above (re-aligned in update)
  rpmUnitLabel = lv_label_create(mainScreenPtr);
  lv_label_set_text(rpmUnitLabel, "RPM");
  lv_obj_set_style_text_font(rpmUnitLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(rpmUnitLabel, COL_TEXT_DIM, 0);
  lv_obj_align(rpmUnitLabel, LV_ALIGN_TOP_MID, 0, MAIN_RPM_TAG_Y);

  rpmLabel = lv_label_create(mainScreenPtr);
  lv_label_set_text(rpmLabel, "0.00");
  lv_obj_set_style_text_font(rpmLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_update_layout(rpmLabel);
  lv_obj_set_style_transform_pivot_x(rpmLabel, lv_obj_get_width(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, lv_obj_get_height(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_zoom(rpmLabel, MAIN_RPM_VALUE_ZOOM, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, 0,
               MAIN_RPM_TAG_Y - MAIN_RPM_VALUE_GAP - lv_obj_get_height(rpmLabel) -
                   MAIN_RPM_VALUE_LIFT);

  // ── Left side buttons: JOG, CW, PULSE (170x72, gray by default) ──
  const int sideW = 170;
  const int sideH = 72;
  const int sideGap = 10;
  const int leftX = 8;
  const int rightX = 800 - 8 - sideW;

  jogBtn = make_btn(mainScreenPtr, leftX, 36, sideW, sideH, "JOG", false);
  lv_obj_add_event_cb(jogBtn, jog_press_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_release_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_release_cb, LV_EVENT_PRESS_LOST, nullptr);

  cwBtn = make_btn(mainScreenPtr, leftX, 36 + sideH + sideGap, sideW, sideH, "CW", false);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  pulseBtn = make_btn(mainScreenPtr, leftX, 36 + (sideH + sideGap) * 2, sideW, sideH, "PULSE", false);
  lv_obj_add_event_cb(pulseBtn, pulse_event_cb, LV_EVENT_CLICKED, nullptr);

  // ── Right side buttons: STEP, TIMER, MENU (170x72, gray) ──
  lv_obj_t* stepBtn = make_btn(mainScreenPtr, rightX, 36, sideW, sideH, "STEP", false);
  lv_obj_add_event_cb(stepBtn, [](lv_event_t*) { screens_show(SCREEN_STEP); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* timerBtn = make_btn(mainScreenPtr, rightX, 36 + sideH + sideGap, sideW, sideH, "3-2-1", false);
  lv_obj_add_event_cb(timerBtn, [](lv_event_t*) { screens_show(SCREEN_TIMER); }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* menuBtn = make_btn(mainScreenPtr, rightX, 36 + (sideH + sideGap) * 2, sideW, sideH, "MENU", false);
  lv_obj_add_event_cb(menuBtn, menu_event_cb, LV_EVENT_CLICKED, nullptr);

  pedalBtn = make_btn(mainScreenPtr, rightX, 36 + (sideH + sideGap) * 3, sideW, sideH, "PEDAL", false);
  lv_obj_add_event_cb(pedalBtn, pedal_toggle_cb, LV_EVENT_CLICKED, nullptr);
  pedalLabel = lv_obj_get_child(pedalBtn, 0);

  // ── Pulse time controls under PULSE button (left column x=8, w=170) ──
  // Layout: [-]  value  [+] ON  — buttons left, label right towards center
  const int pCol = 8;
  const int pW = 170;
  const int pH = 44;
  const int pBtnW = 44;

  // Row 1: ON  y=290
  lv_obj_t* pulseOnDownBtn = make_btn(mainScreenPtr, pCol + 2, 290, pBtnW, pH, "-", false);
  lv_obj_add_event_cb(pulseOnDownBtn, pulse_on_down_cb, LV_EVENT_CLICKED, nullptr);

  pulseOnLabel = lv_label_create(mainScreenPtr);
  lv_obj_set_style_text_font(pulseOnLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(pulseOnLabel, COL_TEXT, 0);
  lv_obj_set_pos(pulseOnLabel, pCol + 50, 296);

  lv_obj_t* pulseOnUpBtn = make_btn(mainScreenPtr, pCol + 116, 290, pBtnW, pH, "+", false);
  lv_obj_add_event_cb(pulseOnUpBtn, pulse_on_up_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* onTag = lv_label_create(mainScreenPtr);
  lv_label_set_text(onTag, "ON");
  lv_obj_set_style_text_font(onTag, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(onTag, COL_TEXT_DIM, 0);
  lv_obj_set_pos(onTag, pCol + pW + 4, 302);

  // Row 2: OFF y=340
  lv_obj_t* pulseOffDownBtn = make_btn(mainScreenPtr, pCol + 2, 340, pBtnW, pH, "-", false);
  lv_obj_add_event_cb(pulseOffDownBtn, pulse_off_down_cb, LV_EVENT_CLICKED, nullptr);

  pulseOffLabel = lv_label_create(mainScreenPtr);
  lv_obj_set_style_text_font(pulseOffLabel, FONT_XL, 0);
  lv_obj_set_style_text_color(pulseOffLabel, COL_TEXT, 0);
  lv_obj_set_pos(pulseOffLabel, pCol + 50, 346);

  lv_obj_t* pulseOffUpBtn = make_btn(mainScreenPtr, pCol + 116, 340, pBtnW, pH, "+", false);
  lv_obj_add_event_cb(pulseOffUpBtn, pulse_off_up_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* offTag = lv_label_create(mainScreenPtr);
  lv_label_set_text(offTag, "OFF");
  lv_obj_set_style_text_font(offTag, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(offTag, COL_TEXT_DIM, 0);
  lv_obj_set_pos(offTag, pCol + pW + 4, 352);

  // ── Bottom bar: START + STOP (y=420, h=56) ──
  const int botY = 420;
  const int botH = 56;
  const int botBtnW = 392;
  const int botGap = 8;

  startBtn = ui_create_btn(mainScreenPtr, 4, botY, botBtnW, botH, "> START", FONT_SUBTITLE,
                           UI_BTN_NORMAL, start_event_cb, nullptr);
  stopBtn = ui_create_btn(mainScreenPtr, 4 + botBtnW + botGap, botY, botBtnW, botH, "X STOP",
                         FONT_SUBTITLE, UI_BTN_DANGER, stop_event_cb, nullptr);

  LOG_I("Screen main: centered gauge layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_invalidate_widgets() {
  mainScreenPtr = nullptr;
  stateLabel = nullptr;
  rpmLabel = nullptr;
  rpmUnitLabel = nullptr;
  rpmIndicatorArc = nullptr;
  pulseOnLabel = nullptr;
  pulseOffLabel = nullptr;
  startBtn = nullptr;
  stopBtn = nullptr;
  jogBtn = nullptr;
  cwBtn = nullptr;
  pulseBtn = nullptr;
  pedalBtn = nullptr;
  pedalLabel = nullptr;
}

void screen_main_update() {
  if (!screens_is_active(SCREEN_MAIN)) return;

  SystemState state = control_get_state();
  Direction dir = speed_get_direction();

  float rpm;
  bool is_running = (state == STATE_RUNNING || state == STATE_PULSE ||
                     state == STATE_STEP || state == STATE_JOG);
  if (is_running) {
    rpm = speed_get_actual_rpm();
    if (rpm < 0.01f) rpm = speed_get_target_rpm();
  } else {
    rpm = speed_get_target_rpm();
  }

  bool stateChanged = (state != prevState);
  bool dirChanged = (dir != prevDir);
  // Epsilon so small actual-RPM changes still refresh the gauge needle during motion
  bool rpmChanged = (fabsf(rpm - prevRpm) > 0.003f);
  bool pedalChanged = (speed_get_pedal_enabled() != prevPedalEnabled);

  if (!mainDirty && !stateChanged && !dirChanged && !rpmChanged && !pedalChanged) return;

  mainDirty = false;
  prevState = state;
  prevDir = dir;
  prevRpm = rpm;
  prevPedalEnabled = speed_get_pedal_enabled();

  float mx = speed_get_rpm_max();
  int32_t arcMax = (int32_t)(mx * 100.0f + 0.5f);
  if (arcMax < 1) arcMax = 1;
  lv_arc_set_range(rpmIndicatorArc, 0, arcMax);

  int32_t val = (int32_t)(rpm * 100.0f);
  if (val > arcMax) val = arcMax;
  if (val < 0) val = 0;

  // Header state
  if (state < (int)(sizeof(state_strings) / sizeof(state_strings[0]))) {
    lv_label_set_text(stateLabel, state_strings[state]);
    lv_obj_set_style_text_color(stateLabel, get_state_color(state), 0);
  }

  // RPM in gauge (re-center value above "RPM" when width changes)
  lv_label_set_text_fmt(rpmLabel, "%.2f", rpm);
  lv_obj_update_layout(rpmLabel);
  lv_obj_set_style_transform_pivot_x(rpmLabel, lv_obj_get_width(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, lv_obj_get_height(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_zoom(rpmLabel, MAIN_RPM_VALUE_ZOOM, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, 0,
               MAIN_RPM_TAG_Y - MAIN_RPM_VALUE_GAP - lv_obj_get_height(rpmLabel) -
                   MAIN_RPM_VALUE_LIFT);
  lv_arc_set_value(rpmIndicatorArc, val);

  // START button: gray when idle, accent when running
  if (is_running) {
    set_btn_style(startBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
  } else {
    set_btn_style(startBtn, COL_BTN_BG, COL_BORDER, 1, COL_TEXT);
  }

  // JOG button: accent when jogging, gray otherwise
  if (state == STATE_JOG) {
    set_btn_style(jogBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
  } else {
    set_btn_style(jogBtn, COL_BTN_BG, COL_BORDER, 1, COL_TEXT);
  }

  // CW button: accent when CW, gray when CCW
  lv_obj_t* cwLbl = lv_obj_get_child(cwBtn, 0);
  if (cwLbl) lv_label_set_text(cwLbl, (dir == DIR_CW) ? "CW" : "CCW");
  if (dir == DIR_CW) {
    set_btn_style(cwBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
  } else {
    set_btn_style(cwBtn, COL_BTN_BG, COL_BORDER, 1, COL_TEXT);
  }

  // PULSE button: accent when pulsing
  if (state == STATE_PULSE) {
    set_btn_style(pulseBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
  } else {
    set_btn_style(pulseBtn, COL_BTN_BG, COL_BORDER, 1, COL_TEXT);
  }

  // PEDAL button: accent when enabled
  if (pedalBtn) {
    if (speed_get_pedal_enabled()) {
      set_btn_style(pedalBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
    } else {
      set_btn_style(pedalBtn, COL_BTN_BG, COL_BORDER, 1, COL_TEXT);
    }
  }

  // Pulse time labels
  if (pulseOnLabel) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fs", mainPulseOnMs / 1000.0f);
    lv_label_set_text(pulseOnLabel, buf);
  }
  if (pulseOffLabel) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1fs", mainPulseOffMs / 1000.0f);
    lv_label_set_text(pulseOffLabel, buf);
  }

}

void screen_main_set_program_pulse_times(uint32_t on_ms, uint32_t off_ms) {
  if (on_ms < 100) on_ms = 100;
  if (on_ms > 10000) on_ms = 10000;
  if (off_ms < 100) off_ms = 100;
  if (off_ms > 10000) off_ms = 10000;
  mainPulseOnMs = on_ms;
  mainPulseOffMs = off_ms;
  screen_main_set_dirty();
}