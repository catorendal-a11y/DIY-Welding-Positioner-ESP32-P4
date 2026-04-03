#pragma once
#include <Arduino.h>

typedef enum {
  MICROSTEP_4  = 4,
  MICROSTEP_8  = 8,
  MICROSTEP_16 = 16,
  MICROSTEP_32 = 32
} MicrostepSetting;

void microstep_init();
MicrostepSetting microstep_get();
void microstep_set(MicrostepSetting setting);
const char* microstep_get_string();
uint32_t microstep_get_steps_per_rev();
void microstep_save();
