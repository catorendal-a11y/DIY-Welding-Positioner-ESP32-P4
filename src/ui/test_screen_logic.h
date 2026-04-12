// TIG Rotator Controller - Pure Screen Logic for Testing
// Extracted from screen_*.cpp files for native unit testing (no LVGL dependencies)

#pragma once
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ───────────────────────────────────────────────────────────────────────────────
// PULSE SCREEN — duty, cycle, frequency, bar percentages, total time
// Mirrors update_computed_info() in screen_pulse.cpp / screen_edit_pulse.cpp
// ───────────────────────────────────────────────────────────────────────────────

inline float pulse_compute_duty(uint32_t on_ms, uint32_t off_ms) {
  float onSec = on_ms / 1000.0f;
  float cycleSec = onSec + off_ms / 1000.0f;
  return (cycleSec > 0.0f) ? (onSec / cycleSec * 100.0f) : 0.0f;
}

inline float pulse_compute_cycle_time(uint32_t on_ms, uint32_t off_ms) {
  return (on_ms + off_ms) / 1000.0f;
}

inline float pulse_compute_frequency(uint32_t on_ms, uint32_t off_ms) {
  float cycleSec = (on_ms + off_ms) / 1000.0f;
  return (cycleSec > 0.0f) ? (1.0f / cycleSec) : 0.0f;
}

// Returns total time in seconds. cycles==0 means infinite, returns -1.0f
inline float pulse_compute_total_time(uint32_t on_ms, uint32_t off_ms, uint16_t cycles) {
  if (cycles == 0) return -1.0f;
  float cycleSec = (on_ms + off_ms) / 1000.0f;
  return cycleSec * cycles;
}

// Bar percent: maps ms from [min_ms, max_ms] -> [0, 100]
inline int pulse_bar_percent_time(uint32_t ms, uint32_t min_ms, uint32_t max_ms) {
  if (max_ms <= min_ms) return 0;
  if (ms <= min_ms) return 0;
  if (ms >= max_ms) return 100;
  int pct = (int)((ms - min_ms) * 100 / (max_ms - min_ms));
  if (pct > 100) pct = 100;
  return pct;
}

inline int pulse_bar_percent_rpm(float rpm, float min_rpm, float max_rpm) {
  if (max_rpm <= min_rpm) return 0;
  float span = max_rpm - min_rpm;
  int pct = (int)((rpm - min_rpm) * 100.0f / span + 0.5f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// ───────────────────────────────────────────────────────────────────────────────
// TIMER SCREEN — countdown color, arc progress, seconds clamp
// Mirrors countdown_color() and screen_timer_update() in screen_timer.cpp
// ───────────────────────────────────────────────────────────────────────────────

// Returns 0=green, 1=yellow, 2=red based on remaining/total ratio
inline int countdown_color_index(int remaining, int total) {
  if (total <= 0) return 0;
  int pct = remaining * 100 / total;
  if (pct > 66) return 0;
  if (pct > 33) return 1;
  return 2;
}

inline int countdown_arc_pct(int remaining, int total) {
  if (total <= 0) return 0;
  int pct = remaining * 100 / total;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

inline int countdown_clamp_seconds(int sec) {
  if (sec < 1) return 1;
  if (sec > 10) return 10;
  return sec;
}

// ───────────────────────────────────────────────────────────────────────────────
// STEP SCREEN — angle validation, total computations, progress arc
// Mirrors screen_step.cpp logic
// ───────────────────────────────────────────────────────────────────────────────

inline bool step_angle_valid(float degrees) {
  return degrees > 0.0f && degrees <= 3600.0f;
}

// CUSTOM input: "300mm", "dia 300 mm", "OD300" -> diameter (mm); plain "90" / "90,5" -> degrees
inline bool step_string_has_mm_ci(const char* s) {
  if (!s) return false;
  for (; *s; ++s) {
    if (!s[1]) break;
    if ((s[0] == 'm' || s[0] == 'M') && (s[1] == 'm' || s[1] == 'M')) return true;
  }
  return false;
}

inline bool step_string_has_utf8_diameter_letter(const char* s) {
  if (!s) return false;
  for (; *s; ++s) {
    if ((unsigned char)s[0] == 0xC3 && s[1]) {
      unsigned char b = (unsigned char)s[1];
      if (b == 0xB8 || b == 0x98) return true;  // ø U+00F8, Ø U+00D8
    }
  }
  return false;
}

inline bool step_input_looks_like_diameter_mm(const char* s) {
  return step_string_has_mm_ci(s) || step_string_has_utf8_diameter_letter(s);
}

inline const char* step_skip_to_number(const char* s) {
  if (!s) return nullptr;
  while (*s) {
    unsigned char c = (unsigned char)*s;
    if (isdigit(c)) return s;
    if ((c == '+' || c == '-') && isdigit((unsigned char)s[1])) return s;
    if ((c == '.' || c == ',') && isdigit((unsigned char)s[1])) return s;
    ++s;
  }
  return nullptr;
}

inline float step_parse_first_float(const char* s) {
  const char* p = step_skip_to_number(s);
  if (!p) return 0.0f;
  char tmp[48];
  size_t j = 0;
  if (*p == '+' || *p == '-') {
    if (j < sizeof(tmp) - 1) tmp[j++] = *p++;
  }
  bool has_dot = false;
  while (*p && j < sizeof(tmp) - 1) {
    unsigned char c = (unsigned char)*p;
    if (isdigit(c)) {
      tmp[j++] = (char)c;
      ++p;
      continue;
    }
    if ((c == '.' || c == ',') && !has_dot) {
      tmp[j++] = '.';
      has_dot = true;
      ++p;
      continue;
    }
    break;
  }
  if (j == 0) return 0.0f;
  tmp[j] = '\0';
  return strtof(tmp, nullptr);
}

// Arc length along workpiece circumference per STEP (default 1 mm)
inline float step_angle_degrees_from_diameter_mm(float D_mm, float arc_mm_per_step) {
  if (D_mm < 1.0f) D_mm = 1.0f;
  if (arc_mm_per_step <= 0.0f) arc_mm_per_step = 1.0f;
  const float kPi = 3.14159265f;
  const float circum = kPi * D_mm;
  float deg = (arc_mm_per_step / circum) * 360.0f;
  if (deg < 0.01f) deg = 0.01f;
  if (deg > 3600.0f) deg = 3600.0f;
  return deg;
}

inline float step_compute_total_angle(float angle, int repeats) {
  return angle * repeats;
}

inline long step_compute_total_steps(float angle, int repeats,
                                     float gear_ratio, float d_rulle,
                                     float d_emne, uint32_t spr, float cal) {
  float motor_deg = angle * gear_ratio * (d_emne / d_rulle);
  long steps_one = (long)(motor_deg / 360.0f * (float)spr);
  steps_one = (long)((float)steps_one * cal);
  return steps_one * repeats;
}

// Duration = (total_angle / (rpm * 360)) * 60 + dwell_between_steps
inline float step_compute_duration(float angle, int repeats, float dwell_s,
                                   float rpm, float gear_ratio,
                                   float d_emne, float d_rulle, uint32_t spr) {
  if (rpm <= 0.0f) return 0.0f;
  float total_angle = angle * repeats;
  float minutes = total_angle / (rpm * 360.0f);
  float move_seconds = minutes * 60.0f;
  float dwell_total = (repeats > 1) ? dwell_s * (repeats - 1) : 0.0f;
  return move_seconds + dwell_total;
}

inline int step_progress_arc(float accumulated, float target) {
  if (target <= 0.0f) return 0;
  int progress = (int)((accumulated / target) * 360.0f);
  if (progress > 360) progress = 360;
  if (progress < 0) progress = 0;
  return progress;
}

inline int step_arc_angle_clamped(float angle) {
  int a = (int)angle;
  if (a > 360) a = 360;
  if (a < 0) a = 0;
  return a;
}

// ───────────────────────────────────────────────────────────────────────────────
// SYSINFO SCREEN — uptime, heap percentage, core load
// Mirrors screen_sysinfo.cpp computation
// ───────────────────────────────────────────────────────────────────────────────

inline void uptime_format(uint32_t elapsed_sec, uint32_t* h, uint32_t* m, uint32_t* s) {
  *h = elapsed_sec / 3600;
  *m = (elapsed_sec % 3600) / 60;
  *s = elapsed_sec % 60;
}

inline int heap_percent(uint32_t free_bytes, uint32_t total_bytes) {
  if (total_bytes == 0) return 0;
  return (int)((uint64_t)free_bytes * 100 / total_bytes);
}

inline int core_load_percent(uint32_t delta_idle, uint32_t delta_total) {
  if (delta_total == 0) return 0;
  int load = 100 - (int)(delta_idle * 100 / delta_total);
  if (load < 0) load = 0;
  if (load > 100) load = 100;
  return load;
}

// ───────────────────────────────────────────────────────────────────────────────
// CALIBRATION SCREEN — steps per degree, error, tolerance
// Mirrors screen_calibration.cpp update logic
// ───────────────────────────────────────────────────────────────────────────────

inline float cal_steps_per_deg(uint32_t spr, float gear_ratio, float factor) {
  return (float)spr * gear_ratio / 360.0f * factor;
}

inline float cal_error_degrees(float factor) {
  return (factor - 1.0f) * 360.0f;
}

inline bool cal_is_within_tolerance(float factor, float tolerance) {
  float err = (factor - 1.0f) * 360.0f;
  if (err < 0.0f) err = -err;
  return err <= tolerance;
}

inline int cal_bar_pct(float factor, float tolerance) {
  float err = (factor - 1.0f) * 360.0f;
  if (err < 0.0f) err = -err;
  if (tolerance <= 0.0f) return 100;
  int pct = (int)(err / tolerance * 100.0f);
  if (pct > 100) pct = 100;
  return pct;
}

// ───────────────────────────────────────────────────────────────────────────────
// DISPLAY SCREEN — brightness conversions
// Mirrors screen_display.cpp slider/settings logic
// ───────────────────────────────────────────────────────────────────────────────

inline int brightness_raw_to_pct(uint8_t raw) {
  int pct = (int)((uint32_t)raw * 100 / 255);
  if (pct < 20) pct = 20;
  return pct;
}

inline uint8_t brightness_pct_to_raw(int pct) {
  return (uint8_t)(pct * 255 / 100);
}

// ───────────────────────────────────────────────────────────────────────────────
// BOOT SCREEN — progress clamping
// Mirrors screen_boot.cpp set_progress / increment
// ───────────────────────────────────────────────────────────────────────────────

inline int boot_clamp_progress(int pct) {
  if (pct < 0) return 0;
  if (pct > 100) return 100;
  return pct;
}

inline int boot_increment(int current, int delta) {
  return boot_clamp_progress(current + delta);
}

// ───────────────────────────────────────────────────────────────────────────────
// MAIN SCREEN — state color index, arc value, RPM source selection
// Mirrors screen_main.cpp logic
// ───────────────────────────────────────────────────────────────────────────────

// Returns 0=accent(default), 1=green(running), 2=red(estop)
// STATE_RUNNING=1, STATE_ESTOP=6
inline int get_state_color_index(int state) {
  if (state == 1) return 1;  // STATE_RUNNING
  if (state == 6) return 2;  // STATE_ESTOP
  return 0;
}

inline int32_t rpm_arc_value(float rpm) {
  return (int32_t)(rpm * 100.0f);
}

inline float rpm_adjust(float current, float delta, float min_rpm, float max_rpm) {
  float result = current + delta;
  if (result < min_rpm) result = min_rpm;
  if (result > max_rpm) result = max_rpm;
  return result;
}

// Returns true if motor is in an active running state
inline bool is_running_state(int state) {
  return state == 1 || state == 2 || state == 3 || state == 4;
  // STATE_RUNNING=1, STATE_PULSE=2, STATE_STEP=3, STATE_JOG=4
}

// ───────────────────────────────────────────────────────────────────────────────
// PROGRAMS SCREEN — format preset details string
// Mirrors format_details() in screen_programs.cpp
// ───────────────────────────────────────────────────────────────────────────────

// mode: 0=continuous, 1=pulse, 2=step
inline void format_preset_details(char* buf, int buflen, int mode, float rpm,
                                  uint32_t on_ms, uint32_t off_ms,
                                  float angle, uint16_t cycles) {
  switch (mode) {
    case 0:
      snprintf(buf, buflen, "Continuous  %.2f RPM", rpm);
      break;
    case 1:
      if (cycles > 0)
        snprintf(buf, buflen, "Pulse  %.1fs/%.1fs  %u cyc  %.2f RPM",
                 on_ms / 1000.0f, off_ms / 1000.0f, cycles, rpm);
      else
        snprintf(buf, buflen, "Pulse  %.1fs/%.1fs  INF  %.2f RPM",
                 on_ms / 1000.0f, off_ms / 1000.0f, rpm);
      break;
    case 2:
      snprintf(buf, buflen, "Step  %.1f deg  %.2f RPM", angle, rpm);
      break;
    default:
      snprintf(buf, buflen, "Unknown mode %d", mode);
      break;
  }
}
