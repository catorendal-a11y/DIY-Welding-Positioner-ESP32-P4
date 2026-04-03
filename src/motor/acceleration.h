#pragma once
#include <Arduino.h>

void acceleration_init();
void acceleration_set(uint32_t accel);
uint32_t acceleration_get();
void acceleration_save();
bool acceleration_has_pending_apply();
void acceleration_clear_pending();
