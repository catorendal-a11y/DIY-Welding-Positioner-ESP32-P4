#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>

// A measurement belongs to a completed move and its unchanged machine context.
// The draft never changes persisted settings until the verified Save action.
struct CalibrationSession {
  enum Stage { Prepare, Moving, Measure, VerifyReady, Verifying, VerifyMeasure, Result, Saving, Saved };
  Stage stage = Prepare;
  float factor = 1, measured = 0, verified = 0, diameter = 300;
  uint32_t started = 0, timeout = 0;
  long completed_before = 0;
  bool direction_cw = true;
  uint8_t microstep = 0;
  const char* error = nullptr;

  void reset(float saved_factor, float wp_diameter) {
    *this = CalibrationSession{}; factor = saved_factor; diameter = wp_diameter;
  }
  bool moving() const { return stage == Moving || stage == Verifying; }
  bool passed() const { return stage == Result && std::isfinite(verified) && std::fabs(verified - 360) <= 0.5f; }
  bool begin(uint32_t now, uint32_t budget, long completed, bool cw, uint8_t steps) {
    if (stage != Prepare && stage != VerifyReady && stage != Result) return false;
    if (stage != Prepare && (cw != direction_cw || steps != microstep)) {
      abort("Machine settings changed. Restart calibration."); return false;
    }
    const bool verify = stage != Prepare;
    stage = verify ? Verifying : Moving;
    started = now; timeout = budget; completed_before = completed;
    direction_cw = cw; microstep = steps; measured = verify ? measured : 0; verified = 0; error = nullptr;
    return true;
  }
  void abort(const char* reason) {
    stage = Prepare; measured = verified = 0; error = reason;
  }
  // Fault/STOP are handled before completion, including a final-cycle fault.
  void observe(uint32_t now, bool idle, bool fault, long completed, bool cw, uint8_t steps) {
    if (!moving()) return;
    if (fault) { abort("Move interrupted. Clear the fault and restart."); return; }
    if (cw != direction_cw || steps != microstep) { abort("Machine settings changed. Restart calibration."); return; }
    if (uint32_t(now - started) > timeout) { abort("Move timed out. Restart calibration."); return; }
    if (!idle) return;
    if (completed == completed_before + 1) { stage = stage == Moving ? Measure : VerifyMeasure; return; }
    if (uint32_t(now - started) > 500) abort("Move did not complete. Restart calibration.");
  }
  bool measurement(float angle) {
    if ((stage != Measure && stage != VerifyMeasure) || !std::isfinite(angle) || angle < 0.5f || angle > 720) {
      error = "Enter a valid angle from 0.5 to 720 degrees."; return false;
    }
    error = nullptr;
    if (stage == VerifyMeasure) { verified = angle; stage = Result; }
    else measured = angle;
    return true;
  }
  bool apply() {
    if (stage != Measure || !std::isfinite(measured) || measured < 0.5f) return false;
    const float candidate = factor * 360 / measured;
    if (!std::isfinite(candidate) || candidate < 0.5f || candidate > 1.5f) {
      error = "Correction outside 0.5-1.5. Check diameter and gearing."; return false;
    }
    factor = candidate; verified = 0; stage = VerifyReady; error = nullptr; return true;
  }
};

// Require the whole entry to be numeric; accept either decimal separator.
inline bool calibration_parse_angle(const char* text, float& value) {
  if (!text || !*text) return false;
  char entry[24]; unsigned n = 0;
  for (; text[n] && n < sizeof(entry)-1; ++n) entry[n] = text[n] == ',' ? '.' : text[n];
  if (text[n]) return false;
  entry[n] = 0; char* end = nullptr; value = std::strtof(entry, &end);
  return end != entry && *end == 0 && std::isfinite(value);
}
