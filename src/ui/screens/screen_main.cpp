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
#include "../../safety/safety.h"
#include "../../storage/storage.h"
#include "freertos/semphr.h"

// ───────────────────────────────────────────────────────────────────────────────
// WIDGETS
// ───────────────────────────────────────────────────────────────────────────────
// Quick-pulse defaults (ms) — used when pulse button is pressed from main screen
static uint32_t mainPulseOnMs  = 500;
static uint32_t mainPulseOffMs = 500;

static lv_obj_t* mainScreenPtr = nullptr;
static lv_obj_t* stateLabel = nullptr;
static lv_obj_t* sourceLabel = nullptr;
static lv_obj_t* statusDetailLabel = nullptr;
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
static bool prevAdsPedalPresent = false;
static int prevSourceMode = -1;
static const char* prevStartBlockReason = nullptr;
static bool mainDirty = true;

static void screen_main_set_dirty() { mainDirty = true; }
static lv_color_t get_state_color(int state) {
  switch (state) {
    case STATE_RUNNING: return COL_GREEN;
    case STATE_ESTOP:   return COL_RED;
    default:            return COL_ACCENT;
  }
}

static const char* main_start_block_reason(SystemState state) {
  if (safety_is_driver_alarm_latched()) return "ALM ACTIVE";
  if (safety_is_estop_active()) return "E-STOP ACTIVE";
  if (state != STATE_IDLE) return "NOT IDLE";
  if (speed_get_target_rpm() < MIN_RPM) return "RPM TOO LOW";
  return nullptr;
}

static int main_speed_source_mode(void) {
  if (speed_pedal_analog_available()) return 2;
  if (speed_using_slider()) return 1;
  return 0;
}

static const char* main_speed_source_label(int sourceMode) {
  switch (sourceMode) {
    case 2: return "SRC PEDAL ADC";
    case 1: return "SRC SLIDER";
    default: return "SRC POT";
  }
}

// ───────────────────────────────────────────────────────────────────────────────
// EVENT handlers
// ───────────────────────────────────────────────────────────────────────────────
static void start_event_cb(lv_event_t*) {
  SystemState state = control_get_state();
  const char* blockReason = main_start_block_reason(state);
  if (blockReason != nullptr) {
    if (statusDetailLabel) {
      lv_label_set_text(statusDetailLabel, blockReason);
      lv_obj_set_style_text_color(statusDetailLabel, COL_WARN, 0);
    }
    screen_main_set_dirty();
    return;
  }
  control_start_continuous();
  screen_main_set_dirty();
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
  if (mainPulseOnMs > PULSE_MS_MIN) mainPulseOnMs -= 100;
  screen_main_set_dirty();
}
static void pulse_on_up_cb(lv_event_t*) {
  if (mainPulseOnMs < PULSE_MS_MAX) mainPulseOnMs += 100;
  screen_main_set_dirty();
}
static void pulse_off_down_cb(lv_event_t*) {
  if (mainPulseOffMs > PULSE_MS_MIN) mainPulseOffMs -= 100;
  screen_main_set_dirty();
}
static void pulse_off_up_cb(lv_event_t*) {
  if (mainPulseOffMs < PULSE_MS_MAX) mainPulseOffMs += 100;
  screen_main_set_dirty();
}
static void pedal_toggle_cb(lv_event_t*) {
  bool wantOn = !speed_get_pedal_enabled();
  speed_set_pedal_enabled(wantOn);
  xSemaphoreTake(g_settings_mutex, portMAX_DELAY);
  g_settings.pedal_enabled = speed_get_pedal_enabled();
  xSemaphoreGive(g_settings_mutex);
  storage_save_settings();
  screen_main_set_dirty();
}
static void menu_event_cb(lv_event_t*) {
  screens_show(SCREEN_MENU);
}
static void step_screen_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_STEP);
}
static void timer_screen_cb(lv_event_t* e) {
  (void)e;
  screens_show(SCREEN_TIMER);
}

// ───────────────────────────────────────────────────────────────────────────────
// Helpers
// ───────────────────────────────────────────────────────────────────────────────
static lv_obj_t* make_btn(lv_obj_t* parent, int x, int y, int w, int h,
                           const char* text, bool accent) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_pos(btn, x, y);
  ui_btn_style_post(btn, accent ? UI_BTN_ACCENT : UI_BTN_NORMAL);

  lv_obj_t* lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  lv_obj_set_style_text_font(lbl, FONT_SUBTITLE, 0);
  lv_obj_set_style_text_color(lbl, ui_btn_label_color_post(accent ? UI_BTN_ACCENT : UI_BTN_NORMAL), 0);
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
// Layout: HEADER_H (38) | Side buttons | Gauge center | Bottom bar
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_create() {
  mainScreenPtr = screenRoots[SCREEN_MAIN];
  lv_obj_set_style_bg_color(mainScreenPtr, COL_BG, 0);

  // ── Header (POST 38px) ──
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
  lv_obj_set_pos(title, PAD_X, 11);

  stateLabel = lv_label_create(header);
  lv_label_set_text(stateLabel, "IDLE");
  lv_obj_set_style_text_font(stateLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(stateLabel, COL_GREEN, 0);
  lv_obj_set_pos(stateLabel, 164, 12);

  sourceLabel = lv_label_create(header);
  lv_label_set_text(sourceLabel, "SRC POT");
  lv_obj_set_style_text_font(sourceLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(sourceLabel, COL_TEXT_DIM, 0);
  lv_obj_set_width(sourceLabel, 160);
  lv_obj_set_pos(sourceLabel, 302, 12);

  statusDetailLabel = lv_label_create(header);
  lv_label_set_text(statusDetailLabel, "READY");
  lv_obj_set_style_text_font(statusDetailLabel, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(statusDetailLabel, COL_TEXT_DIM, 0);
  lv_obj_set_style_text_align(statusDetailLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(statusDetailLabel, 240);
  lv_obj_align(statusDetailLabel, LV_ALIGN_TOP_RIGHT, -16, 12);

  ui_add_post_header_accent(mainScreenPtr);

  // ── Gauge semicircle (theme MAIN_GAUGE_*) ──
  const int arcCX = MAIN_GAUGE_CX;
  const int arcCY = MAIN_GAUGE_CY;
  const int arcR = MAIN_GAUGE_R;

  // Track arc (background)
  lv_obj_t* gaugeTrack = lv_arc_create(mainScreenPtr);
  int trackSize = arcR * 2 + MAIN_GAUGE_TRACK_EXTRA;
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
  lv_obj_align(rpmUnitLabel, LV_ALIGN_TOP_MID, MAIN_RPM_CENTER_OFS_X, MAIN_RPM_TAG_Y);

  rpmLabel = lv_label_create(mainScreenPtr);
  lv_label_set_text(rpmLabel, "0.00");
  lv_obj_set_style_text_font(rpmLabel, FONT_HUGE, 0);
  lv_obj_set_style_text_color(rpmLabel, COL_ACCENT, 0);
  lv_obj_update_layout(rpmLabel);
  lv_obj_set_style_transform_pivot_x(rpmLabel, lv_obj_get_width(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, lv_obj_get_height(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_zoom(rpmLabel, MAIN_RPM_VALUE_ZOOM, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, MAIN_RPM_CENTER_OFS_X,
               MAIN_RPM_TAG_Y - MAIN_RPM_VALUE_GAP - lv_obj_get_height(rpmLabel) -
                   MAIN_RPM_VALUE_LIFT);

  lv_obj_t* potHint = lv_label_create(mainScreenPtr);
  lv_label_set_text(potHint, "speed follows potentiometer");
  lv_obj_set_style_text_font(potHint, FONT_SMALL, 0);
  lv_obj_set_style_text_color(potHint, COL_TEXT_VDIM, 0);
  lv_obj_set_style_text_align(potHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(potHint, 520);
  lv_obj_align(potHint, LV_ALIGN_TOP_MID, MAIN_RPM_CENTER_OFS_X, MAIN_POT_HINT_Y);

  // ── Side mode buttons (slightly taller than POST mockup for touch) ──
  const int mainTop = HEADER_H + 16;
  const int sideW = 150;
  const int sideH = 64;
  const int sideGap = 12;
  const int leftX = 16;
  const int rightX = SCREEN_W - 16 - sideW;

  jogBtn = make_btn(mainScreenPtr, leftX, mainTop, sideW, sideH, "JOG", false);
  lv_obj_add_event_cb(jogBtn, jog_press_cb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_release_cb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(jogBtn, jog_release_cb, LV_EVENT_PRESS_LOST, nullptr);

  cwBtn = make_btn(mainScreenPtr, leftX, mainTop + sideH + sideGap, sideW, sideH, "CW", false);
  lv_obj_add_event_cb(cwBtn, cw_event_cb, LV_EVENT_CLICKED, nullptr);

  pulseBtn = make_btn(mainScreenPtr, leftX, mainTop + (sideH + sideGap) * 2, sideW, sideH, "PULSE", false);
  lv_obj_add_event_cb(pulseBtn, pulse_event_cb, LV_EVENT_CLICKED, nullptr);

  // ── Right: STEP, 3-2-1, MENU, PEDAL ──
  lv_obj_t* stepBtn = make_btn(mainScreenPtr, rightX, mainTop, sideW, sideH, "STEP", false);
  lv_obj_add_event_cb(stepBtn, step_screen_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* timerBtn =
      make_btn(mainScreenPtr, rightX, mainTop + sideH + sideGap, sideW, sideH, "3-2-1", false);
  lv_obj_add_event_cb(timerBtn, timer_screen_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* menuBtn =
      make_btn(mainScreenPtr, rightX, mainTop + (sideH + sideGap) * 2, sideW, sideH, "MENU", false);
  lv_obj_add_event_cb(menuBtn, menu_event_cb, LV_EVENT_CLICKED, nullptr);

  pedalBtn = make_btn(mainScreenPtr, rightX, mainTop + (sideH + sideGap) * 3, sideW, sideH, "PEDAL", false);
  lv_obj_add_event_cb(pedalBtn, pedal_toggle_cb, LV_EVENT_CLICKED, nullptr);

  // ── Pulse time editors (left column, below mode buttons; align X with side buttons) ──
  const int pCol = leftX;
  const int pW = sideW;
  const int pH = 44;
  const int pBtnW = 48;
  const int pBtnGap = 4;
  // Vertically center FONT_NORMAL tags on the 44px rows (baseline ~= rowCenter - 5)
  const int pulseTagYOff = (pH / 2) - 5;
  const int pulseRow1Y = mainTop + (sideH + sideGap) * 3 + 6;

  lv_obj_t* pulseOnDownBtn = make_btn(mainScreenPtr, pCol, pulseRow1Y, pBtnW, pH, "-", false);
  lv_obj_add_event_cb(pulseOnDownBtn, pulse_on_down_cb, LV_EVENT_CLICKED, nullptr);

  pulseOnLabel = lv_label_create(mainScreenPtr);
  lv_obj_set_style_text_font(pulseOnLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(pulseOnLabel, COL_TEXT, 0);
  lv_obj_set_pos(pulseOnLabel, pCol + pBtnW + pBtnGap, pulseRow1Y + 8);

  lv_obj_t* pulseOnUpBtn =
      make_btn(mainScreenPtr, pCol + 100, pulseRow1Y, pBtnW, pH, "+", false);
  lv_obj_add_event_cb(pulseOnUpBtn, pulse_on_up_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* onTag = lv_label_create(mainScreenPtr);
  lv_label_set_text(onTag, "ON");
  lv_obj_set_style_text_font(onTag, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(onTag, COL_TEXT_DIM, 0);
  lv_obj_set_pos(onTag, pCol + pW + 14, pulseRow1Y + pulseTagYOff);

  const int pulseRow2Y = pulseRow1Y + pH + 8;
  lv_obj_t* pulseOffDownBtn = make_btn(mainScreenPtr, pCol, pulseRow2Y, pBtnW, pH, "-", false);
  lv_obj_add_event_cb(pulseOffDownBtn, pulse_off_down_cb, LV_EVENT_CLICKED, nullptr);

  pulseOffLabel = lv_label_create(mainScreenPtr);
  lv_obj_set_style_text_font(pulseOffLabel, FONT_LARGE, 0);
  lv_obj_set_style_text_color(pulseOffLabel, COL_TEXT, 0);
  lv_obj_set_pos(pulseOffLabel, pCol + pBtnW + pBtnGap, pulseRow2Y + 8);

  lv_obj_t* pulseOffUpBtn =
      make_btn(mainScreenPtr, pCol + 100, pulseRow2Y, pBtnW, pH, "+", false);
  lv_obj_add_event_cb(pulseOffUpBtn, pulse_off_up_cb, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* offTag = lv_label_create(mainScreenPtr);
  lv_label_set_text(offTag, "OFF");
  lv_obj_set_style_text_font(offTag, FONT_NORMAL, 0);
  lv_obj_set_style_text_color(offTag, COL_TEXT_DIM, 0);
  lv_obj_set_pos(offTag, pCol + pW + 14, pulseRow2Y + pulseTagYOff);

  // ── Bottom bar (376x52, gap 16) ──
  const int botY = 412;
  const int botH = 52;
  const int botBtnW = 376;
  const int botGap = 16;

  startBtn = ui_create_btn(mainScreenPtr, 16, botY, botBtnW, botH, "> START", FONT_SUBTITLE,
                           UI_BTN_NORMAL, start_event_cb, nullptr);
  stopBtn = ui_create_btn(mainScreenPtr, 16 + botBtnW + botGap, botY, botBtnW, botH, "X STOP",
                         FONT_SUBTITLE, UI_BTN_DANGER, stop_event_cb, nullptr);

  LOG_I("Screen main: centered gauge layout created");
}

// ───────────────────────────────────────────────────────────────────────────────
// SCREEN UPDATE
// ───────────────────────────────────────────────────────────────────────────────
void screen_main_invalidate_widgets() {
  mainScreenPtr = nullptr;
  stateLabel = nullptr;
  sourceLabel = nullptr;
  statusDetailLabel = nullptr;
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
}

void screen_main_update() {
  if (!screens_is_active(SCREEN_MAIN)) return;

  SystemState state = control_get_state();
  Direction dir = speed_get_direction();
  const char* startBlockReason = main_start_block_reason(state);
  int sourceMode = main_speed_source_mode();

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
  bool adsPresent = speed_ads1115_pedal_present();
  bool pedalUiChanged =
      (speed_get_pedal_enabled() != prevPedalEnabled) || (adsPresent != prevAdsPedalPresent);
  bool sourceChanged = (sourceMode != prevSourceMode);
  bool startBlockChanged = (startBlockReason != prevStartBlockReason);

  if (!mainDirty && !stateChanged && !dirChanged && !rpmChanged && !pedalUiChanged &&
      !sourceChanged && !startBlockChanged) return;

  mainDirty = false;
  prevState = state;
  prevDir = dir;
  prevRpm = rpm;
  prevPedalEnabled = speed_get_pedal_enabled();
  prevAdsPedalPresent = adsPresent;
  prevSourceMode = sourceMode;
  prevStartBlockReason = startBlockReason;

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
  if (sourceLabel) {
    lv_label_set_text(sourceLabel, main_speed_source_label(sourceMode));
    lv_obj_set_style_text_color(sourceLabel, sourceMode == 2 ? COL_ACCENT : COL_TEXT_DIM, 0);
  }
  if (statusDetailLabel) {
    if (is_running) {
      lv_label_set_text(statusDetailLabel, "MOTOR ACTIVE");
      lv_obj_set_style_text_color(statusDetailLabel, COL_ACCENT, 0);
    } else if (startBlockReason != nullptr) {
      lv_label_set_text(statusDetailLabel, startBlockReason);
      lv_obj_set_style_text_color(statusDetailLabel, COL_WARN, 0);
    } else {
      lv_label_set_text(statusDetailLabel, "READY");
      lv_obj_set_style_text_color(statusDetailLabel, COL_GREEN, 0);
    }
  }

  // RPM in gauge (re-center value above "RPM" when width changes)
  lv_label_set_text_fmt(rpmLabel, "%.2f", rpm);
  lv_obj_update_layout(rpmLabel);
  lv_obj_set_style_transform_pivot_x(rpmLabel, lv_obj_get_width(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_pivot_y(rpmLabel, lv_obj_get_height(rpmLabel) / 2, 0);
  lv_obj_set_style_transform_zoom(rpmLabel, MAIN_RPM_VALUE_ZOOM, 0);
  lv_obj_align(rpmLabel, LV_ALIGN_TOP_MID, MAIN_RPM_CENTER_OFS_X,
               MAIN_RPM_TAG_Y - MAIN_RPM_VALUE_GAP - lv_obj_get_height(rpmLabel) -
                   MAIN_RPM_VALUE_LIFT);
  lv_arc_set_value(rpmIndicatorArc, val);

  // START button: gray when idle, accent when running
  if (is_running) {
    set_btn_style(startBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
  } else if (startBlockReason != nullptr) {
    set_btn_style(startBtn, COL_BTN_BG, COL_WARN, 2, COL_WARN);
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

  if (pedalBtn) {
    lv_obj_add_flag(pedalBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* pedalLbl = lv_obj_get_child(pedalBtn, 0);
    if (pedalLbl) {
      if (speed_pedal_analog_available()) {
        lv_label_set_text(pedalLbl, "PEDAL ADC");
      } else if (speed_get_pedal_enabled()) {
        lv_label_set_text(pedalLbl, "PEDAL SW");
      } else {
        lv_label_set_text(pedalLbl, "PEDAL");
      }
    }
    if (speed_get_pedal_enabled()) {
      set_btn_style(pedalBtn, COL_BG_ACTIVE, COL_ACCENT, 2, COL_ACCENT);
    } else {
      set_btn_style(pedalBtn, COL_BTN_BG, adsPresent ? COL_BORDER : COL_TEXT_VDIM, 1, COL_TEXT);
    }
  }

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
  if (on_ms < PULSE_MS_MIN) on_ms = PULSE_MS_MIN;
  if (on_ms > PULSE_MS_MAX) on_ms = PULSE_MS_MAX;
  if (off_ms < PULSE_MS_MIN) off_ms = PULSE_MS_MIN;
  if (off_ms > PULSE_MS_MAX) off_ms = PULSE_MS_MAX;
  mainPulseOnMs = on_ms;
  mainPulseOffMs = off_ms;
  screen_main_set_dirty();
}
