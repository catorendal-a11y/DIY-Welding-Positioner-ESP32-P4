#pragma once
#include <Arduino.h>

void calibration_init();
void calibration_set_factor(float factor); // Runtime draft only, never autosaved
void calibration_discard_draft(); // Request restoration after motion is idle
void calibration_process_pending(); // controlTask: call before dispatch when idle
float calibration_get_factor();
float calibration_get_saved_factor();

long calibration_apply_steps(long steps);
float calibration_apply_angle(float angle);

uint32_t calibration_save(); // Returns the queued persistence generation
bool calibration_validate();
