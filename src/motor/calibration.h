#pragma once
#include <Arduino.h>

void calibration_init();
void calibration_set_factor(float factor);
float calibration_get_factor();

long calibration_apply_steps(long steps);
float calibration_apply_angle(float angle);

void calibration_save();
bool calibration_validate();
